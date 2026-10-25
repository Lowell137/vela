// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "systemstats.h"

#include <QDir>
#include <QFile>
#include <QLocale>
#include <QStorageInfo>

#include <algorithm>

namespace {

QList<qulonglong> readCpu()
{
    QFile file(QStringLiteral("/proc/stat"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QList<QByteArray> parts = file.readLine().simplified().split(' ');
    QList<qulonglong> values;
    for (int i = 1; i < parts.size(); ++i) {
        values.append(parts.at(i).toULongLong());
    }
    return values;
}

QByteArray readSys(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll().trimmed() : QByteArray();
}

QString gigabytes(double bytes)
{
    return QLocale().toString(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1);
}

} // namespace

SystemStats::SystemStats(QObject* parent)
    : QObject(parent)
{
    m_timer.setInterval(1500);
    connect(&m_timer, &QTimer::timeout, this, &SystemStats::sample);

    connect(&m_smi, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const auto gpu = status == QProcess::NormalExit && code == 0
            ? parseNvidiaSmi(m_smi.readAllStandardOutput().split('\n').value(0))
            : std::nullopt;
        if (gpu) {
            setGpu(*gpu);
        } else {
            m_noSmi = true;
        }
    });
    connect(&m_smiApps, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_gpuApps.clear();
        if (status != QProcess::NormalExit || code != 0) {
            return;
        }
        const auto apps = parseNvidiaApps(m_smiApps.readAllStandardOutput());
        for (qsizetype i = 0; i < std::min<qsizetype>(apps.size(), 4); ++i) {
            m_gpuApps.append(QVariantMap {
                { QStringLiteral("name"), apps.at(i).first },
                { QStringLiteral("text"), tr("%1 MB").arg(qRound(apps.at(i).second)) },
            });
        }
        emit changed();
    });
    connect(&m_smi, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_noSmi = true;
        }
    });
}

void SystemStats::setActive(bool on)
{
    if (on == active()) {
        return;
    }
    if (on) {
        m_last = readCpu();
        m_timer.start();
        sample();
    } else {
        m_timer.stop();
    }
    emit changed();
}

int SystemStats::cpuPercent(const QList<qulonglong>& before, const QList<qulonglong>& after)
{
    if (before.size() < 5 || after.size() < before.size()) {
        return 0;
    }
    qulonglong total = 0;
    for (int i = 0; i < before.size(); ++i) {
        total += after.at(i) - std::min(after.at(i), before.at(i));
    }
    // idle and iowait are the 4th and 5th numbers.
    const qulonglong idle = (after.at(3) - std::min(after.at(3), before.at(3))) + (after.at(4) - std::min(after.at(4), before.at(4)));
    return total == 0 ? 0 : int(std::clamp<qulonglong>((total - std::min(total, idle)) * 100 / total, 0, 100));
}

QPair<qulonglong, qulonglong> SystemStats::memoryKb(const QByteArray& meminfo)
{
    qulonglong total = 0, available = 0;
    for (const QByteArray& line : meminfo.split('\n')) {
        if (line.startsWith("MemTotal:")) {
            total = line.mid(9).simplified().split(' ').value(0).toULongLong();
        } else if (line.startsWith("MemAvailable:")) {
            available = line.mid(13).simplified().split(' ').value(0).toULongLong();
        }
    }
    return { total, available };
}

void SystemStats::sample()
{
    const QList<qulonglong> now = readCpu();
    m_cpu = cpuPercent(m_last, now);
    m_last = now;

    QFile meminfo(QStringLiteral("/proc/meminfo"));
    if (meminfo.open(QIODevice::ReadOnly)) {
        const auto [total, available] = memoryKb(meminfo.readAll());
        if (total > 0) {
            const qulonglong used = total - std::min(total, available);
            m_memory = int(used * 100 / total);
            m_memoryText = tr("%1 of %2 GB").arg(gigabytes(used * 1024.0), gigabytes(total * 1024.0));
        }
    }

    const QStorageInfo root(QStringLiteral("/"));
    if (root.isValid() && root.bytesTotal() > 0) {
        const qint64 used = root.bytesTotal() - root.bytesAvailable();
        m_disk = int(used * 100 / root.bytesTotal());
        m_diskText = tr("%1 of %2 GB").arg(gigabytes(double(used)), gigabytes(double(root.bytesTotal())));
    }
    sampleGpu();
    emit changed();
}

std::optional<SystemStats::Gpu> SystemStats::parseNvidiaSmi(const QByteArray& line)
{
    const QList<QByteArray> parts = line.split(',');
    if (parts.size() != 5) {
        return std::nullopt;
    }
    bool ok[4] = {};
    Gpu gpu;
    gpu.name = QString::fromUtf8(parts.at(0).trimmed());
    gpu.load = parts.at(1).trimmed().toInt(&ok[0]);
    gpu.memoryUsedMiB = parts.at(2).trimmed().toDouble(&ok[1]);
    gpu.memoryTotalMiB = parts.at(3).trimmed().toDouble(&ok[2]);
    gpu.temperature = parts.at(4).trimmed().toInt(&ok[3]);
    if (gpu.name.isEmpty() || !ok[0] || !ok[1] || !ok[2]) {
        return std::nullopt;
    }
    if (!ok[3]) {
        gpu.temperature = -1; // "[N/A]"
    }
    return gpu;
}

QList<QPair<QString, double>> SystemStats::parseNvidiaApps(const QByteArray& text)
{
    QList<QPair<QString, double>> apps;
    QString name;
    for (const QByteArray& raw : text.split('\n')) {
        const qsizetype colon = raw.indexOf(':');
        if (colon < 0) {
            continue;
        }
        const QByteArray key = raw.left(colon).trimmed();
        const QString value = QString::fromUtf8(raw.mid(colon + 1).trimmed());
        if (key == "Name") {
            name = value.mid(std::max(value.lastIndexOf(u'/'), value.lastIndexOf(u'\\')) + 1);
        } else if (key == "Used GPU Memory" && !name.isEmpty()) {
            bool ok = false;
            const double mib = value.section(u' ', 0, 0).toDouble(&ok);
            if (ok) {
                auto it = std::find_if(apps.begin(), apps.end(), [&](const auto& app) { return app.first == name; });
                if (it == apps.end()) {
                    apps.append({ name, mib });
                } else {
                    it->second += mib;
                }
            }
            name.clear();
        }
    }
    std::stable_sort(apps.begin(), apps.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    return apps;
}

void SystemStats::sampleGpu()
{
    if (!m_noSmi) {
        if (m_smi.state() == QProcess::NotRunning) {
            m_smi.start(QStringLiteral("nvidia-smi"),
                { QStringLiteral("--query-gpu=name,utilization.gpu,memory.used,memory.total,temperature.gpu"),
                    QStringLiteral("--format=csv,noheader,nounits") });
        }
        if (m_smiApps.state() == QProcess::NotRunning) {
            m_smiApps.start(QStringLiteral("nvidia-smi"), { QStringLiteral("-q"), QStringLiteral("-d"), QStringLiteral("PIDS") });
        }
        return;
    }
    // shortcut: no per-app list on AMD; read drm-memory-vram from /proc/*/fdinfo when someone needs it.
    // amdgpu: the first card that reports a busy percentage.
    const QDir drm(QStringLiteral("/sys/class/drm"));
    for (const QString& card : drm.entryList({ QStringLiteral("card?") }, QDir::Dirs | QDir::System)) {
        const QString device = drm.filePath(card + QStringLiteral("/device/"));
        bool ok = false;
        const int load = readSys(device + QStringLiteral("gpu_busy_percent")).toInt(&ok);
        if (!ok) {
            continue;
        }
        Gpu gpu;
        gpu.name = QString::fromUtf8(readSys(device + QStringLiteral("product_name")));
        if (gpu.name.isEmpty()) {
            gpu.name = tr("Graphics card");
        }
        gpu.load = load;
        gpu.memoryUsedMiB = readSys(device + QStringLiteral("mem_info_vram_used")).toDouble() / (1024 * 1024);
        gpu.memoryTotalMiB = readSys(device + QStringLiteral("mem_info_vram_total")).toDouble() / (1024 * 1024);
        const QStringList hwmon = QDir(device + QStringLiteral("hwmon")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        const QByteArray milli = hwmon.isEmpty() ? QByteArray() : readSys(device + QStringLiteral("hwmon/") + hwmon.first() + QStringLiteral("/temp1_input"));
        gpu.temperature = milli.isEmpty() ? -1 : milli.toInt() / 1000;
        setGpu(gpu);
        return;
    }
}

void SystemStats::setGpu(const Gpu& gpu)
{
    m_gpuName = gpu.name;
    m_gpuLoad = std::clamp(gpu.load, 0, 100);
    m_gpuMemory = gpu.memoryTotalMiB > 0 ? int(gpu.memoryUsedMiB * 100 / gpu.memoryTotalMiB) : 0;
    m_gpuMemoryText = tr("%1 of %2 GB").arg(gigabytes(gpu.memoryUsedMiB * 1024 * 1024), gigabytes(gpu.memoryTotalMiB * 1024 * 1024));
    m_gpuTemperature = gpu.temperature;
    emit changed();
}
