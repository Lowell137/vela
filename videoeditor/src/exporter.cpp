// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "exporter.h"

#include "timeline.h"

#include <QFile>

double Exporter::remaining() const
{
    if (m_progress < 0.02 || !m_clock.isValid()) {
        return -1;
    }
    return m_clock.elapsed() / 1000.0 * (1 - m_progress) / m_progress;
}

void Exporter::start(QObject* project, const QVariantMap& settings, const QString& output)
{
    auto* timeline = qobject_cast<Timeline*>(project);
    if (running() || !timeline) {
        return;
    }
    m_tracks = timeline->tracks();
    m_length = timeline->videoLength();
    m_settings = {};
    m_settings.format = settings.value("format", m_settings.format).toString();
    m_settings.size = QSize(settings.value("width").toInt(), settings.value("height").toInt());
    m_settings.fps = settings.value("fps").toDouble();
    m_settings.quality = settings.value("quality", m_settings.quality).toString();
    m_settings.hardware = settings.value("hardware").toBool() && m_settings.format == QLatin1String("mp4");
    m_settings.font = defaultFont();
    m_output = output;
    launch();
}

void Exporter::launch()
{
    if (m_process) {
        m_process->disconnect(this);
        m_process->deleteLater(); // launch() may run from its finished()
    }
    m_process = new QProcess(this);
    m_progress = 0;
    m_cancelled = false;
    m_error.clear();
    m_stderr.clear();
    m_clock.start();
    connect(m_process, &QProcess::readyReadStandardOutput, this, &Exporter::read);
    connect(m_process, &QProcess::readyReadStandardError, this, [this] {
        m_stderr = (m_stderr + m_process->readAllStandardError()).right(4000);
    });
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        ended(status == QProcess::NormalExit ? code : -1);
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_error = tr("ffmpeg can't be started: is it installed?");
            emit stateChanged();
            emit finished(false);
        }
    });
    m_process->start(QStringLiteral("ffmpeg"), exportArguments(m_tracks, m_settings, m_output));
    emit stateChanged();
    emit progressChanged();
}

void Exporter::read()
{
    while (m_process->canReadLine()) {
        const QByteArray line = m_process->readLine().trimmed();
        if (line.startsWith("out_time_us=") && m_length > 0) {
            bool ok = false;
            const double done = line.mid(12).toLongLong(&ok) / 1e6;
            if (ok) {
                m_progress = std::clamp(done / m_length, 0.0, 1.0);
                emit progressChanged();
            }
        }
    }
}

void Exporter::ended(int code)
{
    if (code == 0) {
        m_progress = 1;
        emit progressChanged();
        emit stateChanged();
        emit finished(true);
        return;
    }
    QFile::remove(m_output);
    if (m_cancelled) {
        emit stateChanged();
        return;
    }
    if (m_settings.hardware) {
        m_settings.hardware = false; // no NVENC after all: the processor
        launch();
        return;
    }
    // The last lines are the ones that tell.
    QStringList lines = QString::fromUtf8(m_stderr).trimmed().split(u'\n');
    m_error = lines.mid(std::max<qsizetype>(0, lines.size() - 4)).join(u'\n');
    if (m_error.isEmpty()) {
        m_error = tr("ffmpeg stopped (code %1).").arg(code);
    }
    emit stateChanged();
    emit finished(false);
}

void Exporter::cancel()
{
    if (!running()) {
        return;
    }
    m_cancelled = true;
    m_process->kill();
    m_process->waitForFinished(3000);
}
