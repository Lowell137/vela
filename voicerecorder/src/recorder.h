// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Recording: pw-record writes a WAV from a PipeWire source, this follows its
// growing file for the level and the clock.

#include <QDir>
#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>

class Recorder : public QObject {
    Q_OBJECT
    Q_PROPERTY(int state READ state NOTIFY stateChanged) // 0 idle, 1 recording, 2 paused
    Q_PROPERTY(qint64 elapsed READ elapsed NOTIFY tick) // ms, without the pauses
    Q_PROPERTY(qreal level READ level NOTIFY tick) // 0..1, with a short fall-off
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged) // [{name, label}]
    Q_PROPERTY(QString device MEMBER m_device NOTIFY deviceChanged) // node name, "" is the default source
    Q_PROPERTY(bool stereo MEMBER m_stereo NOTIFY stereoChanged)

public:
    explicit Recorder(const QDir& folder, QObject* parent = nullptr);
    ~Recorder() override;

    int state() const { return m_state; }
    qint64 elapsed() const;
    qreal level() const { return m_level; }
    QVariantList devices() const { return m_devices; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void stop(); // keeps the recording
    Q_INVOKABLE void discard();
    Q_INVOKABLE void refreshDevices();

signals:
    void stateChanged();
    void tick();
    void devicesChanged();
    void deviceChanged();
    void stereoChanged();
    void saved(const QString& path);
    void failed(const QString& message);

private:
    void setState(int state);
    void finish(); // the process is gone
    void parseDevices(const QByteArray& json);

    QDir m_folder;
    QProcess m_process;
    QTimer m_timer;
    QElapsedTimer m_clock; // monotonic, running while recording
    qint64 m_before = 0; // ms recorded before the last resume
    int m_state = 0;
    qreal m_level = 0;
    bool m_stopping = false;
    bool m_keep = true;
    QString m_part; // the file pw-record writes, renamed when it is done
    QString m_device;
    bool m_stereo = false;
    QVariantList m_devices;
};
