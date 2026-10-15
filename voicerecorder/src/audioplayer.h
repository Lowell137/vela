// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Playback of one recording: libmpv without a picture (the Media Player's
// MpvPlayer draws video, this only needs the sound).

#include <QObject>
#include <QString>

struct mpv_handle;

class AudioPlayer : public QObject {
    Q_OBJECT
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged) // 0..100

public:
    explicit AudioPlayer(QObject* parent = nullptr);
    ~AudioPlayer() override;

    double position() const { return m_position; }
    double duration() const { return m_duration; }
    bool paused() const { return m_paused; }
    bool loaded() const { return m_loaded; }
    int volume() const { return m_volume; }
    void setVolume(int volume);

    Q_INVOKABLE void load(const QString& path); // paused, at the start
    Q_INVOKABLE void unload();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void seek(double seconds);

signals:
    void positionChanged();
    void durationChanged();
    void pausedChanged();
    void loadedChanged();
    void volumeChanged();

private:
    void drain();

    mpv_handle* m_mpv = nullptr;
    double m_position = 0;
    double m_duration = 0;
    bool m_paused = true;
    bool m_loaded = false;
    int m_volume = 100;
};
