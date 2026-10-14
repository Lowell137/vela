// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audioplayer.h"

#include <QMetaObject>

#include <mpv/client.h>

namespace {
enum Property : uint64_t { Position, Duration, Paused, Volume };
}

AudioPlayer::AudioPlayer(QObject* parent)
    : QObject(parent)
{
    m_mpv = mpv_create();
    if (!m_mpv) {
        qFatal("vela-voicerecorder: mpv can't be started");
    }
    mpv_set_option_string(m_mpv, "config", "no");
    mpv_set_option_string(m_mpv, "terminal", "no");
    mpv_set_option_string(m_mpv, "vo", "null");
    mpv_set_option_string(m_mpv, "idle", "yes");
    mpv_set_option_string(m_mpv, "keep-open", "yes"); // stays at the end, paused
    mpv_set_option_string(m_mpv, "audio-display", "no");
    mpv_set_option_string(m_mpv, "input-default-bindings", "no");
    mpv_set_option_string(m_mpv, "audio-client-name", "Sound Recorder");
    if (mpv_initialize(m_mpv) < 0) {
        qFatal("vela-voicerecorder: mpv can't be started");
    }
    mpv_observe_property(m_mpv, Position, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, Duration, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, Paused, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, Volume, "volume", MPV_FORMAT_DOUBLE);
    mpv_set_wakeup_callback(m_mpv, [](void* self) {
        QMetaObject::invokeMethod(static_cast<AudioPlayer*>(self), &AudioPlayer::drain, Qt::QueuedConnection);
    }, this);
}

AudioPlayer::~AudioPlayer()
{
    mpv_terminate_destroy(m_mpv);
}

void AudioPlayer::setVolume(int volume)
{
    mpv_set_property_string(m_mpv, "volume", QByteArray::number(std::clamp(volume, 0, 100)).constData());
}

void AudioPlayer::load(const QString& path)
{
    const QByteArray file = path.toUtf8();
    // The option holds for this file only (mpv 0.38+ for the index argument).
    const char* command[] = { "loadfile", file.constData(), "replace", "-1", "pause=yes", nullptr };
    mpv_command(m_mpv, command);
    m_loaded = true;
    emit loadedChanged();
}

void AudioPlayer::unload()
{
    const char* command[] = { "stop", nullptr };
    mpv_command(m_mpv, command);
    m_loaded = false;
    emit loadedChanged();
}

void AudioPlayer::togglePause()
{
    mpv_set_property_string(m_mpv, "pause", m_paused ? "no" : "yes");
}

void AudioPlayer::pause()
{
    mpv_set_property_string(m_mpv, "pause", "yes");
}

void AudioPlayer::seek(double seconds)
{
    const QByteArray value = QByteArray::number(std::max(0.0, seconds), 'f', 3);
    const char* command[] = { "seek", value.constData(), "absolute", nullptr };
    mpv_command(m_mpv, command);
}

void AudioPlayer::drain()
{
    while (true) {
        const mpv_event* event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) {
            break;
        }
        if (event->event_id != MPV_EVENT_PROPERTY_CHANGE) {
            continue;
        }
        const auto* property = static_cast<mpv_event_property*>(event->data);
        const bool has = property->format != MPV_FORMAT_NONE;
        switch (event->reply_userdata) {
        case Position:
            m_position = has ? *static_cast<double*>(property->data) : 0;
            emit positionChanged();
            break;
        case Duration:
            m_duration = has ? *static_cast<double*>(property->data) : 0;
            emit durationChanged();
            break;
        case Paused:
            m_paused = has ? *static_cast<int*>(property->data) != 0 : true;
            emit pausedChanged();
            break;
        case Volume:
            m_volume = has ? int(*static_cast<double*>(property->data) + 0.5) : 100;
            emit volumeChanged();
            break;
        }
    }
}
