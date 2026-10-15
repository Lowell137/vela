// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// An export (`Exporter` in QML): ffmpeg with exportArguments(), its
// -progress lines turned into a fraction and the time left. Cancelling kills
// it and removes the half-written file; a failure keeps ffmpeg's last words.

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QVariantMap>

#include "exportcommand.h"

class Exporter : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged) // 0..1
    Q_PROPERTY(double remaining READ remaining NOTIFY progressChanged) // seconds, -1 unknown
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString output READ output NOTIFY stateChanged)

public:
    using QObject::QObject;
    ~Exporter() override { cancel(); }

    bool running() const { return m_process && m_process->state() != QProcess::NotRunning; }
    double progress() const { return m_progress; }
    double remaining() const;
    QString error() const { return m_error; }
    QString output() const { return m_output; }

    // `project`: the Timeline. `settings`: format, width, height, fps, quality, hardware.
    Q_INVOKABLE void start(QObject* project, const QVariantMap& settings, const QString& output);
    Q_INVOKABLE void cancel();

signals:
    void stateChanged();
    void progressChanged();
    void finished(bool ok);

private:
    void launch();
    void read();
    void ended(int code);

    QProcess* m_process = nullptr;
    Tracks m_tracks;
    ExportSettings m_settings;
    double m_length = 0;
    double m_progress = 0;
    bool m_cancelled = false;
    QElapsedTimer m_clock;
    QString m_output;
    QString m_error;
    QByteArray m_stderr;
};
