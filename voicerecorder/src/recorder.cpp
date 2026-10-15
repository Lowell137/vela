// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "recorder.h"
#include "wav.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <sys/prctl.h>

#include <csignal>

Recorder::Recorder(const QDir& folder, QObject* parent)
    : QObject(parent)
    , m_folder(folder)
{
    m_timer.setInterval(50);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        // 50 ms is also the meter's window; a paused clock shows no level.
        const qreal peak = m_state == 1 ? wav::tailPeak(m_part, 50) : 0;
        m_level = std::max(peak, m_level * 0.8);
        emit tick();
    });
    connect(&m_process, &QProcess::finished, this, &Recorder::finish);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_keep = false;
            emit failed(tr("pw-record could not be started"));
            finish();
        }
    });
}

Recorder::~Recorder()
{
    discard();
    m_process.waitForFinished(1000);
}

qint64 Recorder::elapsed() const
{
    return m_before + (m_state == 1 ? m_clock.elapsed() : 0);
}

void Recorder::setState(int state)
{
    if (state != m_state) {
        m_state = state;
        emit stateChanged();
    }
}

void Recorder::start()
{
    if (m_state != 0 || m_process.state() != QProcess::NotRunning) {
        return;
    }
    m_folder.mkpath(QStringLiteral("."));
    // Hidden until it is done: the list does not show a half-written file.
    // shortcut: a take lost to a kill -9 stays behind as a hidden file, no sweep yet.
    m_part = m_folder.filePath(QStringLiteral(".recording-%1.wav").arg(QDateTime::currentMSecsSinceEpoch()));
    QStringList arguments { QStringLiteral("--rate"), QStringLiteral("48000"), QStringLiteral("--channels"),
        m_stereo ? QStringLiteral("2") : QStringLiteral("1"), QStringLiteral("--format"), QStringLiteral("s16") };
    if (!m_device.isEmpty()) {
        arguments << QStringLiteral("--target") << m_device;
    }
    arguments << m_part;
    m_stopping = false;
    m_keep = true;
    m_before = 0;
    m_level = 0;
    m_clock.start();
    // If the app dies without cleaning up, pw-record still ends its file.
    m_process.setChildProcessModifier([] { prctl(PR_SET_PDEATHSIG, SIGINT); });
    m_process.start(QStringLiteral("pw-record"), arguments);
    setState(1);
    m_timer.start();
}

void Recorder::pause()
{
    if (m_state == 1 && m_process.state() == QProcess::Running) {
        m_before += m_clock.elapsed();
        ::kill(pid_t(m_process.processId()), SIGSTOP);
        setState(2);
    }
}

void Recorder::resume()
{
    if (m_state == 2 && m_process.state() == QProcess::Running) {
        ::kill(pid_t(m_process.processId()), SIGCONT);
        m_clock.start();
        setState(1);
    }
}

void Recorder::stop()
{
    if (m_state == 0 || m_stopping) {
        return;
    }
    m_stopping = true;
    if (m_process.state() == QProcess::Running) {
        // A stopped process takes no signal: continue it first. SIGINT lets
        // pw-record write the header's final sizes.
        ::kill(pid_t(m_process.processId()), SIGCONT);
        ::kill(pid_t(m_process.processId()), SIGINT);
        QTimer::singleShot(3000, &m_process, [this] {
            if (m_process.state() == QProcess::Running) {
                m_process.kill();
            }
        });
    }
}

void Recorder::discard()
{
    m_keep = false;
    stop();
}

void Recorder::finish()
{
    m_timer.stop();
    const qint64 total = elapsed();
    const bool crashed = !m_stopping && m_keep;
    setState(0);
    m_level = 0;
    m_before = 0;
    emit tick();
    const QString part = std::exchange(m_part, QString());
    if (part.isEmpty()) {
        return;
    }
    const bool usable = QFile::exists(part) && wav::readHeader(part).valid && total > 0;
    if (!m_keep || !usable) {
        QFile::remove(part);
        if (m_keep || crashed) {
            emit failed(tr("Nothing was recorded"));
        }
        return;
    }
    const QString name = names::unique(m_folder, names::stamp(QDateTime::currentDateTime()), QStringLiteral(".wav"));
    if (!QFile::rename(part, m_folder.filePath(name))) {
        emit failed(tr("The recording could not be saved"));
        return;
    }
    emit saved(m_folder.filePath(name));
}

void Recorder::refreshDevices()
{
    auto* dump = new QProcess(this);
    connect(dump, &QProcess::finished, this, [this, dump] {
        parseDevices(dump->readAllStandardOutput());
        dump->deleteLater();
    });
    connect(dump, &QProcess::errorOccurred, dump, &QObject::deleteLater);
    dump->start(QStringLiteral("pw-dump"), {});
}

void Recorder::parseDevices(const QByteArray& json)
{
    QVariantList found;
    for (const QJsonValue& object : QJsonDocument::fromJson(json).array()) {
        const QJsonObject props = object.toObject().value(QStringLiteral("info")).toObject().value(QStringLiteral("props")).toObject();
        if (props.value(QStringLiteral("media.class")).toString() != QLatin1String("Audio/Source")) {
            continue;
        }
        const QString name = props.value(QStringLiteral("node.name")).toString();
        const QString label = props.value(QStringLiteral("node.description")).toString();
        found.append(QVariantMap { { QStringLiteral("name"), name }, { QStringLiteral("label"), label.isEmpty() ? name : label } });
    }
    m_devices = found;
    emit devicesChanged();
}
