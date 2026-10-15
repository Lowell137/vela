// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "store.h"

#include "ics.h"

#include <QCoreApplication>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDir>
#include <QJsonArray>
#include <QLocale>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>

namespace {

QString localPath(const QString& fileOrUrl)
{
    const QUrl url(fileOrUrl);
    return url.isLocalFile() ? url.toLocalFile() : fileOrUrl;
}

QDateTime fromMs(const QVariant& v)
{
    return QDateTime::fromMSecsSinceEpoch(v.toLongLong());
}

} // namespace

Store::Store(const QString& path, QObject* parent)
    : QObject(parent)
    , m_path(path)
{
    if (m_path.isEmpty()) {
        m_path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Vela/calendar/events.json";
    }
    load();
    m_checked = QDateTime::currentDateTime();
    m_timer.setInterval(20000);
    connect(&m_timer, &QTimer::timeout, this, [this] { check(QDateTime::currentDateTime()); });
    m_timer.start();
}

void Store::load()
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) return;
    for (const QJsonValue& v : QJsonDocument::fromJson(file.readAll()).object()["events"].toArray()) {
        const cal::Event e = cal::Event::fromJson(v.toObject());
        if (!e.id.isEmpty() && e.start.isValid()) m_events.append(e);
    }
}

void Store::write()
{
    QJsonArray array;
    for (const cal::Event& e : std::as_const(m_events)) array.append(e.toJson());
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path); // written aside, then renamed over
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("calendar: cannot write %s", qPrintable(m_path));
        return;
    }
    file.write(QJsonDocument(QJsonObject{{"version", 1}, {"events", array}}).toJson());
    file.commit();
}

void Store::touch()
{
    write();
    ++m_revision;
    emit changed();
}

QVariantList Store::occurrences(int fy, int fm, int fd, int ty, int tm, int td) const
{
    QList<QPair<cal::Occurrence, const cal::Event*>> all;
    for (const cal::Event& e : m_events) {
        for (const cal::Occurrence& o : cal::expand(e, QDate(fy, fm, fd), QDate(ty, tm, td))) all.append({o, &e});
    }
    std::sort(all.begin(), all.end(), [](const auto& a, const auto& b) {
        return a.first.start != b.first.start ? a.first.start < b.first.start : a.first.end > b.first.end;
    });
    QVariantList out;
    for (const auto& [o, e] : std::as_const(all)) {
        out.append(QVariantMap{
            {"id", o.id}, {"title", e->title}, {"location", e->location}, {"color", e->color},
            {"allDay", o.allDay}, {"startMs", o.start.toMSecsSinceEpoch()}, {"endMs", o.end.toMSecsSinceEpoch()},
            {"day", o.day.toString(Qt::ISODate)}, {"repeats", e->rule.freq != cal::Rule::None}});
    }
    return out;
}

QVariantMap Store::event(const QString& id) const
{
    for (const cal::Event& e : m_events) {
        if (e.id != id) continue;
        return {{"id", e.id}, {"title", e.title}, {"location", e.location}, {"notes", e.notes}, {"allDay", e.allDay},
                {"startMs", e.start.toMSecsSinceEpoch()}, {"endMs", e.end.toMSecsSinceEpoch()}, {"color", e.color},
                {"freq", e.rule.freq}, {"interval", e.rule.interval}, {"weekdays", e.rule.weekdays}, {"count", e.rule.count},
                {"untilMs", e.rule.until.isValid() ? QDateTime(e.rule.until, QTime(0, 0)).toMSecsSinceEpoch() : 0},
                {"reminder", e.reminder}};
    }
    return {};
}

QString Store::save(const QVariantMap& v)
{
    cal::Event e;
    e.id = v.value("id").toString();
    const bool isNew = e.id.isEmpty();
    if (isNew) e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    e.title = v.value("title").toString();
    e.location = v.value("location").toString();
    e.notes = v.value("notes").toString();
    e.allDay = v.value("allDay").toBool();
    e.start = fromMs(v.value("startMs"));
    e.end = fromMs(v.value("endMs"));
    if (e.allDay) { // whole days: midnight to midnight, the last day inclusive
        e.start.setTime(QTime(0, 0));
        e.end.setTime(QTime(0, 0));
    }
    if (e.end < e.start) e.end = e.start;
    e.color = qBound(0, v.value("color").toInt(), 7);
    e.reminder = v.value("reminder", -1).toInt();
    e.rule.freq = qBound(0, v.value("freq").toInt(), int(cal::Rule::Yearly));
    e.rule.interval = qMax(1, v.value("interval", 1).toInt());
    e.rule.weekdays = v.value("weekdays").toInt() & 0x7f;
    e.rule.count = qMax(0, v.value("count").toInt());
    if (v.value("untilMs").toLongLong() > 0) e.rule.until = fromMs(v.value("untilMs")).date();
    if (e.rule.freq == cal::Rule::None) e.rule = cal::Rule();
    for (cal::Event& old : m_events) {
        if (old.id == e.id) {
            // Moving the series makes the old exceptions meaningless.
            if (old.start == e.start && old.rule == e.rule) e.exceptions = old.exceptions;
            old = e;
            touch();
            return e.id;
        }
    }
    m_events.append(e);
    touch();
    return e.id;
}

void Store::remove(const QString& id)
{
    const qsizetype before = m_events.size();
    m_events.removeIf([&](const cal::Event& e) { return e.id == id; });
    if (m_events.size() != before) touch();
}

void Store::removeOccurrence(const QString& id, const QString& day)
{
    const QDate d = QDate::fromString(day, Qt::ISODate);
    for (cal::Event& e : m_events) {
        if (e.id == id && d.isValid() && !e.exceptions.contains(d)) {
            e.exceptions.append(d);
            touch();
            return;
        }
    }
}

int Store::icsCount(const QString& fileOrUrl) const
{
    QFile file(localPath(fileOrUrl));
    return file.open(QIODevice::ReadOnly) ? int(cal::ics::parse(file.readAll()).size()) : -1;
}

int Store::importIcs(const QString& fileOrUrl)
{
    QFile file(localPath(fileOrUrl));
    if (!file.open(QIODevice::ReadOnly)) return -1;
    const QList<cal::Event> imported = cal::ics::parse(file.readAll());
    for (const cal::Event& e : imported) {
        // The same UID again replaces: importing a file twice doesn't double it.
        m_events.removeIf([&](const cal::Event& old) { return old.id == e.id; });
        m_events.append(e);
    }
    if (!imported.isEmpty()) touch();
    return int(imported.size());
}

bool Store::exportIcs(const QString& fileOrUrl) const
{
    QSaveFile file(localPath(fileOrUrl));
    return file.open(QIODevice::WriteOnly) && file.write(cal::ics::write(m_events)) >= 0 && file.commit();
}

bool Store::remindersPending() const
{
    // shortcut: looks 31 days ahead, so a window-less process ends for a reminder further out; widen if that matters
    return cal::nextReminder(m_events, QDateTime::currentDateTime(), 31).isValid();
}

void Store::setWindowVisible(bool visible)
{
    m_windowVisible = visible;
    if (!visible && !remindersPending()) QCoreApplication::quit();
}

void Store::check(const QDateTime& now)
{
    const QList<cal::Reminder> due = cal::remindersBetween(m_events, m_checked, now);
    m_checked = now;
    for (const cal::Reminder& r : due) notify(r);
    if (!m_windowVisible && !remindersPending()) QCoreApplication::quit();
}

void Store::notify(const cal::Reminder& r)
{
    QDBusInterface bus(QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"),
                       QStringLiteral("org.freedesktop.Notifications"));
    if (!bus.isValid()) return;
    const QString when = QLocale().toString(r.start, QLocale::ShortFormat);
    const QString body = r.location.isEmpty() ? when : when + "\n" + r.location;
    bus.asyncCall(QStringLiteral("Notify"), QStringLiteral("Calendar"), 0u, QStringLiteral("calendar"),
                  r.title.isEmpty() ? tr("(No title)") : r.title, body, QStringList(), QVariantMap(), 10000);
}
