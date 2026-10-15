// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// What the Clock's window shows and does: the timers, the alarms, the
// stopwatch and the world's cities (clockmath.h), one tick that all of them
// are read at, and the ringing (notification and sound) when something is
// due. It lives on while something is armed, window or not.

#include "clockmath.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QTimer>
#include <QVariantList>

class Clock : public QObject {
    Q_OBJECT
    // The two clocks the QML reads (monotonic ms, ms since the epoch);
    // changing is what makes the countdowns and clocks move.
    Q_PROPERTY(qint64 now READ now NOTIFY ticked)
    Q_PROPERTY(qint64 wall READ wall NOTIFY ticked)
    Q_PROPERTY(bool fastTick READ fastTick WRITE setFastTick NOTIFY fastTickChanged)
    Q_PROPERTY(bool armed READ armed NOTIFY armedChanged)
    Q_PROPERTY(QVariantList timers READ timers NOTIFY timersChanged)
    Q_PROPERTY(QVariantList alarms READ alarms NOTIFY alarmsChanged)
    Q_PROPERTY(QVariantList laps READ laps NOTIFY lapsChanged)
    Q_PROPERTY(bool swRunning READ swRunning NOTIFY lapsChanged)
    Q_PROPERTY(QStringList zones READ zones NOTIFY zonesChanged)
    Q_PROPERTY(QString localZone READ localZone CONSTANT)
    Q_PROPERTY(QVariantList ringing READ ringing NOTIFY ringingChanged)
    Q_PROPERTY(bool use24h READ use24h WRITE setUse24h NOTIFY settingsChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY settingsChanged)
    Q_PROPERTY(int snoozeMinutes READ snoozeMinutes WRITE setSnoozeMinutes NOTIFY settingsChanged)

public:
    explicit Clock(QObject* parent = nullptr);

    qint64 now() const { return m_now; }
    qint64 wall() const { return m_wall; }
    bool fastTick() const { return m_fast; }
    void setFastTick(bool fast);
    bool armed() const;
    QVariantList timers() const;
    QVariantList alarms() const;
    QVariantList laps() const;
    bool swRunning() const { return m_stopwatch.running; }
    QStringList zones() const { return m_zones; }
    QString localZone() const { return QString::fromUtf8(QTimeZone::systemTimeZoneId()); }
    QVariantList ringing() const;
    bool use24h() const { return m_use24h; }
    void setUse24h(bool on);
    int volume() const { return m_volume; }
    void setVolume(int percent);
    int snoozeMinutes() const { return m_snooze; }
    void setSnoozeMinutes(int minutes);

    Q_INVOKABLE qint64 timerRemaining(int id, qint64 now) const;
    Q_INVOKABLE int timerAdd(const QString& name, qint64 durationMs, bool start);
    Q_INVOKABLE void timerEdit(int id, const QString& name, qint64 durationMs);
    Q_INVOKABLE void timerStart(int id);
    Q_INVOKABLE void timerPause(int id);
    Q_INVOKABLE void timerReset(int id);
    Q_INVOKABLE void timerRemove(int id);

    Q_INVOKABLE void alarmSave(int id, int hour, int minute, const QString& name, int days, int snoozeMinutes);
    Q_INVOKABLE void alarmSetEnabled(int id, bool enabled);
    Q_INVOKABLE void alarmRemove(int id);
    // Seconds until it rings, -1 when it won't.
    Q_INVOKABLE qint64 alarmEta(int id, qint64 wall) const;

    Q_INVOKABLE qint64 swElapsed(qint64 now) const { return m_stopwatch.elapsed(now); }
    Q_INVOKABLE void swStart();
    Q_INVOKABLE void swPause();
    Q_INVOKABLE void swLap();
    Q_INVOKABLE void swReset();

    Q_INVOKABLE void zoneAdd(const QString& id);
    Q_INVOKABLE void zoneRemove(const QString& id);
    Q_INVOKABLE QVariantMap zoneInfo(const QString& id, qint64 wall) const;
    Q_INVOKABLE QVariantList zoneSearch(const QString& text) const;

    Q_INVOKABLE void ringSnooze(const QString& key);
    Q_INVOKABLE void ringDismiss(const QString& key);

signals:
    void ticked();
    void fastTickChanged();
    void armedChanged();
    void timersChanged();
    void alarmsChanged();
    void lapsChanged();
    void zonesChanged();
    void ringingChanged();
    void settingsChanged();

private slots:
    void tick();
    void onAction(uint id, const QString& action);
    void onClosed(uint id, uint reason);

private:
    using Countdown = vela::clock::Countdown;
    struct AlarmState {
        QDateTime next;
        bool snoozing = false;
    };
    struct Ring {
        QString key; // "alarm:3", "timer:2"
        QString title;
        QString body;
        int source = 0;
        bool alarm = false;
        uint notification = 0;
    };

    qint64 clockNow() const { return m_clock.elapsed(); }
    Countdown* timerById(int id);
    int alarmIndex(int id) const;
    void fire(const Ring& ring);
    void notify(Ring& ring);
    void closeNotification(const Ring& ring);
    void startSound();
    void stopSound();
    void playOnce();
    void emitArmed();
    void saveAlarms() const;
    void saveZones() const;
    static QString dataPath(const QString& file);

    QElapsedTimer m_clock;
    QTimer m_tick;
    qint64 m_now = 0;
    qint64 m_wall = 0;
    bool m_fast = false;
    bool m_wasArmed = false;
    QList<Countdown> m_timers;
    int m_nextTimer = 1;
    QList<vela::clock::Alarm> m_alarms;
    QHash<int, AlarmState> m_alarmState;
    vela::clock::Stopwatch m_stopwatch;
    QStringList m_zones;
    QList<Ring> m_rings;
    QProcess m_player;
    QTimer m_soundCap;
    bool m_soundOn = false;
    QSettings m_settings;
    bool m_use24h = true;
    int m_volume = 80;
    int m_snooze = 10;
};
