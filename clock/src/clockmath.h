// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The Clock's arithmetic and stores, apart from any window: when an alarm
// rings next, how long a timer has left, the stopwatch's laps, how a city's
// time sits against ours. Times that count down use a monotonic clock in
// milliseconds that the caller passes in; wall times are QDateTime.

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QTimeZone>

namespace vela::clock {

// --- alarms ---

struct Alarm {
    int id = 0;
    QTime time;
    QString name;
    int days = 0; // bit 0 Monday ... bit 6 Sunday; 0: once
    bool enabled = true;
    int snoozeMinutes = 10;
};

// The first moment after `now` that is `time` on one of the `days` (any day
// when 0), in now's zone; invalid if there is none. A time that a DST jump
// skips rings at the moment Qt moves it to.
QDateTime nextFire(QTime time, int days, const QDateTime& now);

QByteArray alarmsToJson(const QList<Alarm>& alarms);
QList<Alarm> alarmsFromJson(const QByteArray& json);

// --- timers ---

// A countdown on a monotonic clock: running keeps a deadline, so late ticks
// don't make it drift.
struct Countdown {
    int id = 0;
    QString name;
    qint64 durationMs = 0;
    bool running = false;
    qint64 deadline = 0; // while running
    qint64 left = 0;     // while not

    qint64 remaining(qint64 now) const;
    bool expired(qint64 now) const { return running && now >= deadline; }
    void start(qint64 now);  // from the start, or on from a pause
    void pause(qint64 now);
    void reset();
    bool paused() const { return !running && left < durationMs; }
};

// --- stopwatch ---

struct Lap {
    int number = 0;
    qint64 splitMs = 0; // since the lap before
    qint64 totalMs = 0;
};

struct Stopwatch {
    bool running = false;
    qint64 since = 0;  // the clock when it last started
    qint64 before = 0; // what came before that
    QList<Lap> laps;   // newest first

    qint64 elapsed(qint64 now) const { return before + (running ? now - since : 0); }
    void start(qint64 now);
    void pause(qint64 now);
    void lap(qint64 now);
    void reset();
};

// --- world clock ---

struct ZoneInfo {
    QString id;
    QString city;   // "New York"
    QString region; // "America"
    QTime time;
    int dayDelta = 0;        // against the local date: -1, 0, +1
    int offsetSeconds = 0;   // against the local zone
    bool night = false;
};

ZoneInfo zoneInfo(const QString& id, const QDateTime& utcNow, const QTimeZone& local);

QByteArray zonesToJson(const QStringList& ids);
QStringList zonesFromJson(const QByteArray& json); // valid zones only

} // namespace vela::clock
