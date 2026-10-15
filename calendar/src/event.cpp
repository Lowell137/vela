// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "event.h"

#include <QJsonArray>
#include <algorithm>

namespace cal {

namespace {

const QString kDateTime = QStringLiteral("yyyy-MM-ddTHH:mm:ss");

QDateTime at(QDate day, const QDateTime& timeOf)
{
    return QDateTime(day, timeOf.time());
}

} // namespace

QList<Occurrence> expand(const Event& event, QDate from, QDate to)
{
    QList<Occurrence> out;
    if (!event.start.isValid() || !event.end.isValid() || !from.isValid() || !to.isValid()) {
        return out;
    }
    const QDate first = event.start.date();
    const int span = std::max<qint64>(0, first.daysTo(event.end.date()));
    const Rule& rule = event.rule;
    const int interval = std::max(1, rule.interval);
    int produced = 0;

    // Returns false when no later candidate can matter.
    auto visit = [&](QDate day) {
        if (day > to || (rule.until.isValid() && day > rule.until)) {
            return false;
        }
        if (rule.count > 0 && produced >= rule.count) {
            return false;
        }
        ++produced; // a deleted occurrence still counts, as in RFC 5545
        if (day.addDays(span) >= from && !event.exceptions.contains(day)) {
            out.append({event.id, at(day, event.start), at(day.addDays(span), event.end), event.allDay, day});
        }
        return true;
    };

    switch (rule.freq) {
    case Rule::Daily:
        for (qint64 k = 0;; ++k) {
            if (!visit(first.addDays(k * interval))) break;
        }
        break;
    case Rule::Weekly: {
        int mask = rule.weekdays & 0x7f;
        if (mask == 0) mask = 1 << (first.dayOfWeek() - 1);
        const QDate monday = first.addDays(-(first.dayOfWeek() - 1));
        bool go = true;
        for (qint64 k = 0; go; ++k) {
            const QDate week = monday.addDays(7 * k * interval);
            for (int d = 0; d < 7 && go; ++d) {
                const QDate day = week.addDays(d);
                if (!(mask & (1 << d)) || day < first) continue;
                go = visit(day);
            }
            if (week > to) break;
        }
        break;
    }
    case Rule::Monthly:
    case Rule::Yearly: {
        const bool monthly = rule.freq == Rule::Monthly;
        for (qint64 k = 0;; ++k) {
            const QDate period = monthly ? first.addMonths(int(k * interval)) : first.addYears(int(k * interval));
            // A 31st, or a leap day, that the period lacks is skipped, not moved.
            const QDate day(period.year(), monthly ? period.month() : first.month(), first.day());
            if (!day.isValid()) {
                if (QDate(period.year(), period.month(), 1) > to) break;
                continue;
            }
            if (!visit(day)) break;
        }
        break;
    }
    default:
        visit(first);
        break;
    }
    std::sort(out.begin(), out.end(), [](const Occurrence& a, const Occurrence& b) { return a.start < b.start; });
    return out;
}

QList<Reminder> remindersBetween(const QList<Event>& events, const QDateTime& after, const QDateTime& upTo)
{
    QList<Reminder> out;
    for (const Event& event : events) {
        if (event.reminder < 0) continue;
        // The longest lead is a day: look a bit further than the window.
        for (const Occurrence& occurrence : expand(event, after.date(), upTo.date().addDays(2))) {
            const QDateTime due = occurrence.start.addSecs(-qint64(event.reminder) * 60);
            if (due > after && due <= upTo) {
                out.append({event.id, event.title, event.location, occurrence.start, due});
            }
        }
    }
    std::sort(out.begin(), out.end(), [](const Reminder& a, const Reminder& b) { return a.due < b.due; });
    return out;
}

QDateTime nextReminder(const QList<Event>& events, const QDateTime& now, int days)
{
    const QList<Reminder> due = remindersBetween(events, now, now.addDays(days));
    return due.isEmpty() ? QDateTime() : due.first().due;
}

QJsonObject Event::toJson() const
{
    QJsonObject o;
    o["id"] = id;
    o["title"] = title;
    o["location"] = location;
    o["notes"] = notes;
    o["allDay"] = allDay;
    o["start"] = start.toString(kDateTime);
    o["end"] = end.toString(kDateTime);
    o["color"] = color;
    o["reminder"] = reminder;
    o["freq"] = rule.freq;
    o["interval"] = rule.interval;
    o["weekdays"] = rule.weekdays;
    o["count"] = rule.count;
    o["until"] = rule.until.isValid() ? rule.until.toString(Qt::ISODate) : QString();
    QJsonArray skipped;
    for (const QDate& day : exceptions) skipped.append(day.toString(Qt::ISODate));
    o["exceptions"] = skipped;
    return o;
}

Event Event::fromJson(const QJsonObject& o)
{
    Event e;
    e.id = o["id"].toString();
    e.title = o["title"].toString();
    e.location = o["location"].toString();
    e.notes = o["notes"].toString();
    e.allDay = o["allDay"].toBool();
    e.start = QDateTime::fromString(o["start"].toString(), kDateTime);
    e.end = QDateTime::fromString(o["end"].toString(), kDateTime);
    if (e.end < e.start) e.end = e.start;
    e.color = std::clamp(o["color"].toInt(), 0, 7);
    e.reminder = o["reminder"].toInt(-1);
    e.rule.freq = std::clamp(o["freq"].toInt(), 0, int(Rule::Yearly));
    e.rule.interval = std::max(1, o["interval"].toInt(1));
    e.rule.weekdays = o["weekdays"].toInt() & 0x7f;
    e.rule.count = std::max(0, o["count"].toInt());
    e.rule.until = QDate::fromString(o["until"].toString(), Qt::ISODate);
    for (const QJsonValue& v : o["exceptions"].toArray()) {
        const QDate day = QDate::fromString(v.toString(), Qt::ISODate);
        if (day.isValid()) e.exceptions.append(day);
    }
    return e;
}

} // namespace cal
