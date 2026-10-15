// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gamebar.h"

#include "appmodel.h"
#include "notifications.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

#include <csignal>
#include <sys/prctl.h>

namespace {

const QString gameModeService = QStringLiteral("com.feralinteractive.GameMode");

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.read(4096) : QByteArray();
}

// Asks the session bus whether `service` runs or can be started.
void hasService(QObject* owner, const QString& service, std::function<void(bool)> done)
{
    for (const char* method : { "NameHasOwner", "ListActivatableNames" }) {
        QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
            QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"), QLatin1String(method));
        if (qstrcmp(method, "NameHasOwner") == 0) {
            call << service;
        }
        auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), owner);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, owner, [watcher, service, done] {
            watcher->deleteLater();
            const QVariant answer = watcher->reply().arguments().value(0);
            if (answer.toBool() || answer.toStringList().contains(service)) {
                done(true);
            }
        });
    }
}

} // namespace

GameBar::GameBar(AppModel* apps, NotificationServer* notifications, QObject* parent)
    : QObject(parent)
    , m_apps(apps)
    , m_notifications(notifications)
    , m_replayProgram(QStandardPaths::findExecutable(QStringLiteral("gpu-screen-recorder")))
    , m_mangoHud(!QStandardPaths::findExecutable(QStringLiteral("mangohud")).isEmpty())
{
    m_staleTimer.setSingleShot(true);
    m_staleTimer.setInterval(5000);
    connect(&m_staleTimer, &QTimer::timeout, this, [this] {
        m_perfStale = true;
        emit perfChanged();
    });
    m_sampler.setInterval(1000);
    connect(&m_sampler, &QTimer::timeout, this, &GameBar::sample);
    findSensors();
    hasService(this, gameModeService, [this](bool) {
        if (!m_gameModeAvailable) {
            m_gameModeAvailable = true;
            emit gameModeChanged();
        }
    });
    connect(m_notifications, &NotificationServer::ActionInvoked, this, [this](uint id, const QString&) {
        const QString path = m_saved.take(id);
        if (!path.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
    });
}

GameBar::~GameBar()
{
    stopNvidia();
    if (m_replay) {
        m_replay->disconnect(this);
        m_replay->terminate();
        m_replay->waitForFinished(1000);
    }
}

void GameBar::findSensors()
{
    for (const QFileInfo& hwmon : QDir(QStringLiteral("/sys/class/hwmon")).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QByteArray name = readFile(hwmon.filePath() + QStringLiteral("/name")).trimmed();
        if (name == "k10temp" || name == "coretemp" || name == "zenpower") {
            m_cpuTempFile = hwmon.filePath() + QStringLiteral("/temp1_input");
            break;
        }
    }
    // shortcut: the first NVIDIA GPU (nvidia-smi index 0) or the first card
    // with gpu_busy_percent (amdgpu); pick the one the game renders on when
    // hybrid laptops ask for it.
    m_nvidia = QFile::exists(QStringLiteral("/proc/driver/nvidia/version"))
        && !QStandardPaths::findExecutable(QStringLiteral("nvidia-smi")).isEmpty();
    if (m_nvidia) {
        return;
    }
    const QDir drm(QStringLiteral("/sys/class/drm"));
    for (const QString& card : drm.entryList({ QStringLiteral("card?"), QStringLiteral("card??") }, QDir::Dirs | QDir::System)) {
        const QString device = drm.filePath(card) + QStringLiteral("/device");
        if (QFile::exists(device + QStringLiteral("/gpu_busy_percent"))) {
            m_gpuDevice = device;
            const QStringList hwmons = QDir(device + QStringLiteral("/hwmon")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            if (!hwmons.isEmpty()) {
                m_gpuHwmon = device + QStringLiteral("/hwmon/") + hwmons.first();
            }
            break;
        }
    }
}

void GameBar::setActive(bool active)
{
    if (active == m_active) {
        return;
    }
    m_active = active;
    if (active) {
        m_cpuTimes = gamestats::parseProcStat(readFile(QStringLiteral("/proc/stat")));
        sample();
        m_sampler.start();
        if (m_nvidia) {
            startNvidia();
        }
    } else {
        m_sampler.stop();
        stopNvidia();
    }
    emit activeChanged();
}

void GameBar::sample()
{
    const gamestats::CpuTimes times = gamestats::parseProcStat(readFile(QStringLiteral("/proc/stat")));
    m_cpu = gamestats::cpuPercent(m_cpuTimes, times);
    m_cpuTimes = times;
    m_cpuTemp = m_cpuTempFile.isEmpty() ? -1 : gamestats::parseSysfs(readFile(m_cpuTempFile), 1000);
    const gamestats::MemInfo mem = gamestats::parseMeminfo(readFile(QStringLiteral("/proc/meminfo")));
    m_ramTotal = mem.totalKb < 0 ? -1 : mem.totalKb / 1024.0;
    m_ramUsed = mem.totalKb < 0 || mem.availableKb < 0 ? -1 : (mem.totalKb - mem.availableKb) / 1024.0;
    if (!m_gpuDevice.isEmpty()) {
        sampleGpuSysfs();
    }
    emit statsChanged();
}

// amdgpu: busy %, VRAM (bytes) and the card's own hwmon (edge temperature, W).
void GameBar::sampleGpuSysfs()
{
    m_gpu.busy = gamestats::parseSysfs(readFile(m_gpuDevice + QStringLiteral("/gpu_busy_percent")));
    m_gpu.memUsedMb = gamestats::parseSysfs(readFile(m_gpuDevice + QStringLiteral("/mem_info_vram_used")), 1024.0 * 1024);
    m_gpu.memTotalMb = gamestats::parseSysfs(readFile(m_gpuDevice + QStringLiteral("/mem_info_vram_total")), 1024.0 * 1024);
    if (!m_gpuHwmon.isEmpty()) {
        m_gpu.temp = gamestats::parseSysfs(readFile(m_gpuHwmon + QStringLiteral("/temp1_input")), 1000);
        m_gpu.power = gamestats::parseSysfs(readFile(m_gpuHwmon + QStringLiteral("/power1_average")), 1e6);
        if (m_gpu.power < 0) {
            m_gpu.power = gamestats::parseSysfs(readFile(m_gpuHwmon + QStringLiteral("/power1_input")), 1e6);
        }
    }
}

// One long-running nvidia-smi printing a line a second, only while active.
void GameBar::startNvidia()
{
    if (m_nvidiaSmi) {
        return;
    }
    auto* smi = new QProcess(this);
    m_nvidiaSmi = smi;
    smi->setChildProcessModifier([] { ::prctl(PR_SET_PDEATHSIG, SIGTERM); });
    connect(smi, &QProcess::readyReadStandardOutput, this, [this, smi] {
        while (smi->canReadLine()) {
            const gamestats::GpuSample sample = gamestats::parseNvidiaCsv(smi->readLine());
            if (sample.index == 0) {
                m_gpu = sample;
                emit statsChanged();
            }
        }
    });
    connect(smi, &QProcess::finished, smi, &QObject::deleteLater);
    connect(smi, &QProcess::errorOccurred, smi, [smi](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            smi->deleteLater();
        }
    });
    smi->start(QStringLiteral("nvidia-smi"),
        { QStringLiteral("--query-gpu=index,utilization.gpu,temperature.gpu,power.draw,memory.used,memory.total"),
            QStringLiteral("--format=csv,noheader,nounits"), QStringLiteral("-lms"), QStringLiteral("1000") });
}

void GameBar::stopNvidia()
{
    if (m_nvidiaSmi) {
        m_nvidiaSmi->disconnect(this);
        m_nvidiaSmi->kill(); // finished deletes it
        m_nvidiaSmi = nullptr;
    }
}

void GameBar::perf(double fps, double frameMs, int missed)
{
    m_fps = fps;
    m_frameMs = frameMs;
    m_missed = missed;
    m_perfStale = false;
    m_fpsHistory.push(fps);
    m_frameHistory.push(frameMs);
    m_staleTimer.start();
    emit perfChanged();
}

void GameBar::focusedApp(const QString& appId, int pid)
{
    const QString id = appId == QLatin1String("-") ? QString() : appId;
    if (id != m_appId || pid != m_pid) {
        m_appId = id;
        m_pid = pid;
        emit appChanged();
        emit gameModeChanged();
    }
}

QString GameBar::appName() const
{
    const QString desktopId = m_appId.isEmpty() ? QString() : m_apps->findDesktopId(m_appId);
    return desktopId.isEmpty() ? m_appId : m_apps->name(desktopId);
}

QString GameBar::appIcon() const
{
    return m_appId.isEmpty() ? QString() : m_apps->iconForAppId(m_appId);
}

// RegisterGame(pid) / UnregisterGame(pid): 0 is success.
void GameBar::setGameMode(bool on)
{
    const int pid = on ? m_pid : m_gameModePid;
    if (!m_gameModeAvailable || pid <= 0 || on == gameMode()) {
        return;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(gameModeService, QStringLiteral("/com/feralinteractive/GameMode"),
        gameModeService, on ? QStringLiteral("RegisterGame") : QStringLiteral("UnregisterGame"));
    call << pid;
    auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, on, pid] {
        watcher->deleteLater();
        const QDBusPendingReply<int> reply = *watcher;
        if (reply.isError() || reply.value() != 0) {
            qWarning() << "vela-shell: GameMode" << (on ? "RegisterGame" : "UnregisterGame") << pid
                       << (reply.isError() ? reply.error().message() : QString::number(reply.value()));
            return;
        }
        m_gameModePid = on ? pid : 0;
        emit gameModeChanged();
    });
}

void GameBar::setReplay(bool on)
{
    if (on == replay() || m_replayProgram.isEmpty()) {
        return;
    }
    if (!on) {
        ::kill(pid_t(m_replay->processId()), SIGINT); // finished clears it
        return;
    }
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) + QStringLiteral("/Captures");
    QDir().mkpath(folder);
    m_replay = new QProcess(this);
    m_replay->setChildProcessModifier([] { ::prctl(PR_SET_PDEATHSIG, SIGINT); });
    // It prints the path of every replay it saves.
    connect(m_replay, &QProcess::readyReadStandardOutput, this, [this] {
        while (m_replay && m_replay->canReadLine()) {
            const QString path = QString::fromUtf8(m_replay->readLine()).trimmed();
            if (!QFileInfo(path).isFile()) {
                continue;
            }
            const uint id = m_notifications->Notify(tr("Game Bar"), 0, QStringLiteral("media-record"), tr("Replay saved"),
                tr("Saved to Videos > Captures. Click to open it."),
                { QStringLiteral("default"), tr("Open") }, QVariantMap(), 8000);
            m_saved.insert(id, path);
        }
    });
    connect(m_replay, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (status == QProcess::CrashExit || code != 0) {
            qWarning() << "vela-shell: gpu-screen-recorder stopped:" << m_replay->readAllStandardError().right(400);
        }
        m_replay->deleteLater();
        m_replay = nullptr;
        emit replayChanged();
    });
    connect(m_replay, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_replay) {
            m_replay->deleteLater();
            m_replay = nullptr;
            emit replayChanged();
        }
    });
    m_replay->start(m_replayProgram, { QStringLiteral("-w"), QStringLiteral("screen"), QStringLiteral("-f"), QStringLiteral("60"),
                                         QStringLiteral("-r"), QStringLiteral("30"), QStringLiteral("-c"), QStringLiteral("mp4"),
                                         QStringLiteral("-o"), folder });
    emit replayChanged();
}

// SIGUSR1: gpu-screen-recorder writes the last 30 s and prints the file.
void GameBar::saveReplay()
{
    if (m_replay && m_replay->processId() > 0) {
        ::kill(pid_t(m_replay->processId()), SIGUSR1);
    }
}
