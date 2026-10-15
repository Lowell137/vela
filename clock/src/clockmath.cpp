// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "clockmath.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace vela::clock {

QDateTime nextFire(QTime time, int days, const QDateTime& now)
{
    if (!time.isValid() || !now.isValid()) {
        return {};
    }
    const QTimeZone zone = now.timeZone();
    const QDate today = now.date();
    for (int i = 0; i <= 8; ++i) {
        const QDate day = today.addDays(i);
        if (days != 0 && !(days & (1 << (day.dayOfWeek() - 1)))) {
            continue;
        }
        const QDateTime candidate(day, time, zone);
        if (candidate.isValid() && candidate > now) {
            return candidate;
        }
    }
    return {};
}

QByteArray alarmsToJson(const QList<Alarm>& alarms)
{
    QJsonArray array;
    for (const Alarm& a : alarms) {
        array.append(QJsonObject{
            { "id", a.id }, { "hour", a.time.hour() }, { "minute", a.time.minute() },
            { "name", a.name }, { "days", a.days }, { "enabled", a.enabled },
            { "snooze", a.snoozeMinutes } });
    }
    return QJsonDocument(QJsonObject{ { "alarms", array } }).toJson(QJsonDocument::Indented);
}

QList<Alarm> alarmsFromJson(const QByteArray& json)
{
    QList<Alarm> out;
    const QJsonArray array = QJsonDocument::fromJson(json).object().value(QLatin1String("alarms")).toArray();
    for (const QJsonValue& value : array) {
        const QJsonObject o = value.toObject();
        Alarm a;
        a.id = o.value(QLatin1String("id")).toInt();
        a.time = QTime(o.value(QLatin1String("hour")).toInt(-1), o.value(QLatin1String("minute")).toInt(-1));
        a.name = o.value(QLatin1String("name")).toString();
        a.days = o.value(QLatin1String("days")).toInt() & 0x7f;
        a.enabled = o.value(QLatin1String("enabled")).toBool(true);
        a.snoozeMinutes = std::clamp(o.value(QLatin1String("snooze")).toInt(10), 1, 60);
        if (a.id > 0 && a.time.isValid()) {
            out.append(a);
        }
    }
    return out;
}

qint64 Countdown::remaining(qint64 now) const
{
    return running ? std::max<qint64>(0, deadline - now) : left;
}

void Countdown::start(qint64 now)
{
    if (running) {
        return;
    }
    if (left <= 0) {
        left = durationMs;
    }
    deadline = now + left;
    running = true;
}

void Countdown::pause(qint64 now)
{
    if (running) {
        left = remaining(now);
        running = false;
    }
}

void Countdown::reset()
{
    running = false;
    left = durationMs;
}

void Stopwatch::start(qint64 now)
{
    if (!running) {
        since = now;
        running = true;
    }
}

void Stopwatch::pause(qint64 now)
{
    if (running) {
        before = elapsed(now);
        running = false;
    }
}

void Stopwatch::lap(qint64 now)
{
    const qint64 total = elapsed(now);
    const qint64 last = laps.isEmpty() ? 0 : laps.first().totalMs;
    laps.prepend({ int(laps.size()) + 1, total - last, total });
}

void Stopwatch::reset()
{
    running = false;
    before = 0;
    laps.clear();
}

ZoneInfo zoneInfo(const QString& id, const QDateTime& utcNow, const QTimeZone& local)
{
    ZoneInfo info;
    info.id = id;
    const QStringList parts = id.split(u'/');
    info.city = QString(parts.last()).replace(u'_', u' ');
    info.region = parts.size() > 1 ? QString(parts.first()).replace(u'_', u' ') : QString();
    const QTimeZone zone(id.toUtf8());
    if (!zone.isValid()) {
        return info;
    }
    const QDateTime there = utcNow.toTimeZone(zone);
    const QDateTime here = utcNow.toTimeZone(local);
    info.time = there.time();
    info.dayDelta = int(here.date().daysTo(there.date()));
    info.offsetSeconds = zone.offsetFromUtc(utcNow) - local.offsetFromUtc(utcNow);
    info.night = there.time().hour() < 6 || there.time().hour() >= 18;
    return info;
}

QByteArray zonesToJson(const QStringList& ids)
{
    return QJsonDocument(QJsonObject{ { "zones", QJsonArray::fromStringList(ids) } }).toJson(QJsonDocument::Indented);
}

QStringList zonesFromJson(const QByteArray& json)
{
    QStringList out;
    const QJsonArray array = QJsonDocument::fromJson(json).object().value(QLatin1String("zones")).toArray();
    for (const QJsonValue& value : array) {
        const QString id = value.toString();
        if (!id.isEmpty() && QTimeZone(id.toUtf8()).isValid() && !out.contains(id)) {
            out.append(id);
        }
    }
    return out;
}

} // namespace vela::clock
