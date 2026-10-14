// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "clock.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

using namespace vela::clock;

namespace {
constexpr auto soundFile = "/usr/share/sounds/freedesktop/stereo/alarm-clock-elapsed.oga";
constexpr auto service = "org.freedesktop.Notifications";
constexpr auto path = "/org/freedesktop/Notifications";
constexpr int soundCapMs = 60000;

QByteArray readFile(const QString& file)
{
    QFile f(file);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void writeFile(const QString& file, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(file).absolutePath());
    QSaveFile f(file);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(data);
        f.commit();
    }
}
}

QString Clock::dataPath(const QString& file)
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Vela/clock/") + file;
}

Clock::Clock(QObject* parent)
    : QObject(parent)
    , m_settings(QStringLiteral("Vela"), QStringLiteral("vela-clock"))
{
    m_clock.start();
    m_use24h = m_settings.value(QStringLiteral("use24h"), true).toBool();
    m_volume = std::clamp(m_settings.value(QStringLiteral("volume"), 80).toInt(), 0, 100);
    m_snooze = std::clamp(m_settings.value(QStringLiteral("snooze"), 10).toInt(), 1, 60);

    m_alarms = alarmsFromJson(readFile(dataPath(QStringLiteral("alarms.json"))));
    m_zones = zonesFromJson(readFile(dataPath(QStringLiteral("world.json"))));
    const QDateTime current = QDateTime::currentDateTime();
    for (Alarm& a : m_alarms) {
        m_alarmState[a.id].next = a.enabled ? nextFire(a.time, a.days, current) : QDateTime();
    }

    m_tick.setInterval(250);
    connect(&m_tick, &QTimer::timeout, this, &Clock::tick);
    m_tick.start();
    m_now = clockNow();
    m_wall = QDateTime::currentMSecsSinceEpoch();

    m_player.setProgram(QStringLiteral("paplay"));
    m_soundCap.setSingleShot(true);
    m_soundCap.setInterval(soundCapMs);
    connect(&m_soundCap, &QTimer::timeout, this, &Clock::stopSound);
    // Repeats until dismissed or capped; a player that fails is not retried.
    connect(&m_player, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (m_soundOn && code == 0 && status == QProcess::NormalExit) {
            playOnce();
        }
    });

    auto bus = QDBusConnection::sessionBus();
    bus.connect(service, path, service, QStringLiteral("ActionInvoked"), this, SLOT(onAction(uint, QString)));
    bus.connect(service, path, service, QStringLiteral("NotificationClosed"), this, SLOT(onClosed(uint, uint)));
    m_wasArmed = armed();
}

void Clock::setFastTick(bool fast)
{
    if (m_fast != fast) {
        m_fast = fast;
        m_tick.setInterval(fast ? 40 : 250); // shortcut: polls, a timer per deadline would wake less
        emit fastTickChanged();
    }
}

bool Clock::armed() const
{
    if (!m_rings.isEmpty()) {
        return true;
    }
    if (std::any_of(m_timers.begin(), m_timers.end(), [](const Countdown& t) { return t.running; })) {
        return true;
    }
    return std::any_of(m_alarms.begin(), m_alarms.end(), [this](const Alarm& a) {
        return a.enabled || m_alarmState.value(a.id).snoozing;
    });
}

void Clock::emitArmed()
{
    const bool now = armed();
    if (now != m_wasArmed) {
        m_wasArmed = now;
        emit armedChanged();
    }
}

// --- the tick ---

void Clock::tick()
{
    m_now = clockNow();
    m_wall = QDateTime::currentMSecsSinceEpoch();
    const QDateTime wall = QDateTime::fromMSecsSinceEpoch(m_wall);

    QList<Ring> due;
    bool timersMoved = false;
    for (Countdown& t : m_timers) {
        if (t.expired(m_now)) {
            t.reset();
            timersMoved = true;
            due.append({ QStringLiteral("timer:%1").arg(t.id), t.name.isEmpty() ? tr("Timer") : t.name,
                tr("Time's up"), t.id, false, 0 });
        }
    }
    bool alarmsMoved = false;
    for (Alarm& a : m_alarms) {
        AlarmState& state = m_alarmState[a.id];
        if ((a.enabled || state.snoozing) && state.next.isValid() && wall >= state.next) {
            const bool snoozed = state.snoozing;
            state.snoozing = false;
            if (a.days == 0) {
                a.enabled = false; // a one-off is done
            }
            state.next = a.enabled ? nextFire(a.time, a.days, wall) : QDateTime();
            alarmsMoved = true;
            due.append({ QStringLiteral("alarm:%1").arg(a.id), a.name.isEmpty() ? tr("Alarm") : a.name,
                snoozed ? tr("Snoozed alarm") : QLocale().toString(a.time, QLocale::ShortFormat), a.id, true, 0 });
        }
    }
    if (timersMoved) {
        emit timersChanged();
    }
    if (alarmsMoved) {
        saveAlarms();
        emit alarmsChanged();
    }
    for (const Ring& r : due) {
        fire(r);
    }
    emit ticked();
    emitArmed();
}

// --- ringing ---

void Clock::fire(const Ring& ring)
{
    Ring r = ring;
    m_rings.removeIf([&](const Ring& o) { return o.key == r.key; });
    m_rings.append(r);
    notify(m_rings.last());
    startSound();
    emit ringingChanged();
}

void Clock::notify(Ring& ring)
{
    QDBusMessage message = QDBusMessage::createMethodCall(service, path, service, QStringLiteral("Notify"));
    QStringList actions;
    if (ring.alarm) {
        actions << QStringLiteral("snooze") << tr("Snooze");
    }
    actions << QStringLiteral("dismiss") << tr("Dismiss");
    message << QStringLiteral("Clock") << uint(0) << QStringLiteral("preferences-system-time") << ring.title << ring.body
            << actions << QVariantMap{ { QStringLiteral("urgency"), QVariant::fromValue<uchar>(2) } } << 0;
    auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 2000), this);
    const QString key = ring.key;
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, key](QDBusPendingCallWatcher* w) {
        const QDBusPendingReply<uint> reply = *w;
        w->deleteLater();
        for (Ring& r : m_rings) {
            if (r.key == key && reply.isValid()) {
                r.notification = reply.value();
            }
        }
    });
}

void Clock::closeNotification(const Ring& ring)
{
    if (ring.notification == 0) {
        return;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(service, path, service, QStringLiteral("CloseNotification"));
    message << ring.notification;
    QDBusConnection::sessionBus().asyncCall(message, 1000);
}

void Clock::startSound()
{
    if (m_soundOn || !QFile::exists(QString::fromLatin1(soundFile))) {
        return;
    }
    m_soundOn = true;
    m_soundCap.start();
    playOnce();
}

void Clock::playOnce()
{
    // paplay's --volume is 0..65536, 65536 being 100%.
    m_player.setArguments({ QStringLiteral("--volume=%1").arg(m_volume * 65536 / 100), QString::fromLatin1(soundFile) });
    m_player.start();
}

void Clock::stopSound()
{
    m_soundOn = false;
    m_soundCap.stop();
    if (m_player.state() != QProcess::NotRunning) {
        m_player.kill();
    }
}

QVariantList Clock::ringing() const
{
    QVariantList out;
    for (const Ring& r : m_rings) {
        out.append(QVariantMap{ { "key", r.key }, { "title", r.title }, { "body", r.body }, { "snoozable", r.alarm } });
    }
    return out;
}

void Clock::ringSnooze(const QString& key)
{
    const auto it = std::find_if(m_rings.begin(), m_rings.end(), [&](const Ring& r) { return r.key == key; });
    if (it == m_rings.end() || !it->alarm) {
        return;
    }
    const int index = alarmIndex(it->source);
    if (index >= 0) {
        AlarmState& state = m_alarmState[it->source];
        state.snoozing = true;
        state.next = QDateTime::currentDateTime().addSecs(m_alarms[index].snoozeMinutes * 60);
    }
    ringDismiss(key);
    emit alarmsChanged();
}

void Clock::ringDismiss(const QString& key)
{
    const auto it = std::find_if(m_rings.begin(), m_rings.end(), [&](const Ring& r) { return r.key == key; });
    if (it == m_rings.end()) {
        return;
    }
    closeNotification(*it);
    m_rings.erase(it);
    if (m_rings.isEmpty()) {
        stopSound();
    }
    emit ringingChanged();
    emitArmed();
}

void Clock::onAction(uint id, const QString& action)
{
    const auto it = std::find_if(m_rings.begin(), m_rings.end(), [id](const Ring& r) { return r.notification == id; });
    if (it == m_rings.end()) {
        return;
    }
    const QString key = it->key;
    action == QLatin1String("snooze") ? ringSnooze(key) : ringDismiss(key);
}

// The notification was swiped away or expired: that is a dismissal.
void Clock::onClosed(uint id, uint)
{
    const auto it = std::find_if(m_rings.begin(), m_rings.end(), [id](const Ring& r) { return r.notification == id; });
    if (it != m_rings.end()) {
        it->notification = 0;
        ringDismiss(it->key);
    }
}

// --- timers ---

Countdown* Clock::timerById(int id)
{
    const auto it = std::find_if(m_timers.begin(), m_timers.end(), [id](const Countdown& t) { return t.id == id; });
    return it == m_timers.end() ? nullptr : &*it;
}

QVariantList Clock::timers() const
{
    QVariantList out;
    for (const Countdown& t : m_timers) {
        out.append(QVariantMap{ { "id", t.id }, { "name", t.name }, { "durationMs", t.durationMs },
            { "running", t.running }, { "paused", t.paused() } });
    }
    return out;
}

qint64 Clock::timerRemaining(int id, qint64 now) const
{
    const auto it = std::find_if(m_timers.begin(), m_timers.end(), [id](const Countdown& t) { return t.id == id; });
    return it == m_timers.end() ? 0 : it->remaining(now);
}

int Clock::timerAdd(const QString& name, qint64 durationMs, bool start)
{
    Countdown t;
    t.id = m_nextTimer++;
    t.name = name;
    t.durationMs = std::max<qint64>(1000, durationMs);
    t.left = t.durationMs;
    if (start) {
        t.start(clockNow());
    }
    m_timers.append(t);
    emit timersChanged();
    emitArmed();
    return t.id;
}

void Clock::timerEdit(int id, const QString& name, qint64 durationMs)
{
    if (Countdown* t = timerById(id)) {
        t->name = name;
        t->durationMs = std::max<qint64>(1000, durationMs);
        t->reset();
        emit timersChanged();
        emitArmed();
    }
}

void Clock::timerStart(int id)
{
    if (Countdown* t = timerById(id)) {
        t->start(clockNow());
        emit timersChanged();
        emitArmed();
    }
}

void Clock::timerPause(int id)
{
    if (Countdown* t = timerById(id)) {
        t->pause(clockNow());
        emit timersChanged();
        emitArmed();
    }
}

void Clock::timerReset(int id)
{
    if (Countdown* t = timerById(id)) {
        t->reset();
        emit timersChanged();
        emitArmed();
    }
}

void Clock::timerRemove(int id)
{
    m_timers.removeIf([id](const Countdown& t) { return t.id == id; });
    emit timersChanged();
    emitArmed();
}

// --- alarms ---

int Clock::alarmIndex(int id) const
{
    for (int i = 0; i < m_alarms.size(); ++i) {
        if (m_alarms[i].id == id) {
            return i;
        }
    }
    return -1;
}

QVariantList Clock::alarms() const
{
    QVariantList out;
    for (const Alarm& a : m_alarms) {
        out.append(QVariantMap{ { "id", a.id }, { "hour", a.time.hour() }, { "minute", a.time.minute() },
            { "name", a.name }, { "days", a.days }, { "enabled", a.enabled }, { "snooze", a.snoozeMinutes } });
    }
    return out;
}

void Clock::alarmSave(int id, int hour, int minute, const QString& name, int days, int snoozeMinutes)
{
    int index = alarmIndex(id);
    if (index < 0) {
        int top = 0;
        for (const Alarm& a : std::as_const(m_alarms)) {
            top = std::max(top, a.id);
        }
        m_alarms.append({ top + 1, {}, {}, 0, true, 10 });
        index = int(m_alarms.size()) - 1;
    }
    Alarm& a = m_alarms[index];
    a.time = QTime(std::clamp(hour, 0, 23), std::clamp(minute, 0, 59));
    a.name = name;
    a.days = days & 0x7f;
    a.enabled = true; // saving an alarm turns it on, like Windows
    a.snoozeMinutes = std::clamp(snoozeMinutes, 1, 60);
    AlarmState& state = m_alarmState[a.id];
    state.snoozing = false;
    state.next = nextFire(a.time, a.days, QDateTime::currentDateTime());
    saveAlarms();
    emit alarmsChanged();
    emitArmed();
}

void Clock::alarmSetEnabled(int id, bool enabled)
{
    const int index = alarmIndex(id);
    if (index < 0) {
        return;
    }
    Alarm& a = m_alarms[index];
    a.enabled = enabled;
    AlarmState& state = m_alarmState[id];
    state.snoozing = false;
    state.next = enabled ? nextFire(a.time, a.days, QDateTime::currentDateTime()) : QDateTime();
    saveAlarms();
    emit alarmsChanged();
    emitArmed();
}

void Clock::alarmRemove(int id)
{
    m_alarms.removeIf([id](const Alarm& a) { return a.id == id; });
    m_alarmState.remove(id);
    saveAlarms();
    emit alarmsChanged();
    emitArmed();
}

qint64 Clock::alarmEta(int id, qint64 wall) const
{
    const AlarmState state = m_alarmState.value(id);
    const int index = alarmIndex(id);
    if (index < 0 || !state.next.isValid() || !(m_alarms[index].enabled || state.snoozing)) {
        return -1;
    }
    return std::max<qint64>(0, (state.next.toMSecsSinceEpoch() - wall) / 1000);
}

void Clock::saveAlarms() const
{
    writeFile(dataPath(QStringLiteral("alarms.json")), alarmsToJson(m_alarms));
}

// --- stopwatch ---

void Clock::swStart() { m_stopwatch.start(clockNow()); emit lapsChanged(); }
void Clock::swPause() { m_stopwatch.pause(clockNow()); emit lapsChanged(); }
void Clock::swLap() { m_stopwatch.lap(clockNow()); emit lapsChanged(); }
void Clock::swReset() { m_stopwatch.reset(); emit lapsChanged(); }

QVariantList Clock::laps() const
{
    QVariantList out;
    for (const Lap& l : m_stopwatch.laps) {
        out.append(QVariantMap{ { "number", l.number }, { "split", l.splitMs }, { "total", l.totalMs } });
    }
    return out;
}

// --- world clock ---

void Clock::zoneAdd(const QString& id)
{
    if (!m_zones.contains(id) && QTimeZone(id.toUtf8()).isValid()) {
        m_zones.append(id);
        saveZones();
        emit zonesChanged();
    }
}

void Clock::zoneRemove(const QString& id)
{
    if (m_zones.removeAll(id) > 0) {
        saveZones();
        emit zonesChanged();
    }
}

void Clock::saveZones() const
{
    writeFile(dataPath(QStringLiteral("world.json")), zonesToJson(m_zones));
}

QVariantMap Clock::zoneInfo(const QString& id, qint64 wall) const
{
    const QDateTime utc = QDateTime::fromMSecsSinceEpoch(wall, QTimeZone::UTC);
    const QTimeZone local = QTimeZone::systemTimeZone();
    const ZoneInfo z = vela::clock::zoneInfo(id, utc, local);
    return { { "id", z.id }, { "city", z.city }, { "region", z.region }, { "hour", z.time.hour() },
        { "minute", z.time.minute() }, { "dayDelta", z.dayDelta }, { "offset", z.offsetSeconds },
        { "night", z.night } };
}

QVariantList Clock::zoneSearch(const QString& text) const
{
    QVariantList out;
    const QString needle = text.trimmed().replace(u' ', u'_');
    if (needle.size() < 2) {
        return out;
    }
    QList<QByteArray> ids = QTimeZone::availableTimeZoneIds();
    std::sort(ids.begin(), ids.end());
    for (const QByteArray& raw : ids) {
        const QString id = QString::fromLatin1(raw);
        // The area/city ones only: the abbreviations and Etc/ are noise.
        if (!id.contains(u'/') || id.startsWith(QLatin1String("Etc/")) || m_zones.contains(id)
            || !id.contains(needle, Qt::CaseInsensitive)) {
            continue;
        }
        const QStringList parts = id.split(u'/');
        out.append(QVariantMap{ { "id", id }, { "city", QString(parts.last()).replace(u'_', u' ') },
            { "region", QString(parts.first()).replace(u'_', u' ') } });
        if (out.size() >= 40) {
            break;
        }
    }
    return out;
}

// --- settings ---

void Clock::setUse24h(bool on)
{
    m_use24h = on;
    m_settings.setValue(QStringLiteral("use24h"), on);
    emit settingsChanged();
}

void Clock::setVolume(int percent)
{
    m_volume = std::clamp(percent, 0, 100);
    m_settings.setValue(QStringLiteral("volume"), m_volume);
    emit settingsChanged();
}

void Clock::setSnoozeMinutes(int minutes)
{
    m_snooze = std::clamp(minutes, 1, 60);
    m_settings.setValue(QStringLiteral("snooze"), m_snooze);
    emit settingsChanged();
}
