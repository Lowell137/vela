// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Calendar events and the expansion of repeating ones into occurrences.
// Times are wall-clock local times: a repeating event keeps its hour across
// a daylight saving change, as a person means it.

#pragma once

#include <QDate>
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace cal {

struct Rule {
    enum Freq { None, Daily, Weekly, Monthly, Yearly };
    int freq = None;
    int interval = 1;
    int weekdays = 0; // weekly: bit 0 Monday .. bit 6 Sunday; none set: the start's day
    int count = 0; // total occurrences, 0: no limit
    QDate until; // last day, invalid: no limit
    bool operator==(const Rule&) const = default;
};

struct Event {
    QString id;
    QString title;
    QString location;
    QString notes;
    bool allDay = false;
    QDateTime start; // all-day: midnight of the first day
    QDateTime end; // all-day: midnight of the LAST day (inclusive)
    int color = 0; // index of the 8 calendar colors
    Rule rule;
    int reminder = -1; // minutes before, -1: none
    QList<QDate> exceptions; // days of deleted single occurrences

    QJsonObject toJson() const;
    static Event fromJson(const QJsonObject& object);
};

struct Occurrence {
    QString id; // of the event
    QDateTime start;
    QDateTime end;
    bool allDay = false;
    QDate day; // the day the series put it on, what an exception names
};

// The occurrences touching [from, to] (days, inclusive), in start order,
// without the deleted ones.
QList<Occurrence> expand(const Event& event, QDate from, QDate to);

struct Reminder {
    QString id;
    QString title;
    QString location;
    QDateTime start;
    QDateTime due;
};

// Reminders that fall due in (after, upTo].
QList<Reminder> remindersBetween(const QList<Event>& events, const QDateTime& after, const QDateTime& upTo);
// The first one due after now within the given days, invalid if none.
QDateTime nextReminder(const QList<Event>& events, const QDateTime& now, int days);

} // namespace cal
