// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "globalshortcuts.h"

#include <QDateTime>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QFileInfo>

#include <algorithm>

namespace {
const char service[] = "org.freedesktop.impl.portal.desktop.vela";
const char objectPath[] = "/org/freedesktop/portal/desktop";
}

namespace vela {

QDBusArgument& operator<<(QDBusArgument& argument, const PortalShortcut& shortcut)
{
    argument.beginStructure();
    argument << shortcut.id << shortcut.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, PortalShortcut& shortcut)
{
    argument.beginStructure();
    argument >> shortcut.id >> shortcut.properties;
    argument.endStructure();
    return argument;
}

QString triggerFromPortal(const QString& trigger)
{
    QStringList out;
    for (QString part : trigger.split(u'+', Qt::SkipEmptyParts)) {
        part = part.trimmed();
        if (part.isEmpty()) {
            continue;
        }
        if (part.compare(QLatin1String("LOGO"), Qt::CaseInsensitive) == 0) {
            part = QStringLiteral("Super");
        }
        out << part;
    }
    return out.join(u'+');
}

} // namespace vela

using vela::AppShortcut;
using vela::PortalShortcut;

// ------------------------------------------------------------- session --

ShortcutSession::ShortcutSession(GlobalShortcuts* owner, const QString& path, const QString& appId)
    : QObject(owner)
    , m_owner(owner)
    , m_path(path)
    , m_appId(appId)
{
}

void ShortcutSession::Close()
{
    m_owner->closeSession(m_path);
}

// ------------------------------------------------------------- service --

GlobalShortcuts::GlobalShortcuts(QObject* parent)
    : QObject(parent)
    , m_path(vela::appShortcutsPath())
{
    qDBusRegisterMetaType<PortalShortcut>();
    qDBusRegisterMetaType<QList<PortalShortcut>>();
    m_shortcuts = vela::loadAppShortcuts(m_path);
    // Settings changes the file too.
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this, [this] {
        watch();
        reloadFromFile();
    });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    watch();
}

void GlobalShortcuts::setPath(const QString& path)
{
    m_path = path;
    m_shortcuts = vela::loadAppShortcuts(m_path);
    watch();
}

bool GlobalShortcuts::registerService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QString::fromLatin1(service))) {
        qWarning("vela-shell: can't take %s: global shortcuts for apps are off", service);
        return false;
    }
    return bus.registerObject(QString::fromLatin1(objectPath), this,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllProperties
            | QDBusConnection::ExportAdaptors); // the FileChooser's, a child (filechooser.h)
}

void GlobalShortcuts::watch()
{
    const QFileInfo info(m_path);
    const QString dir = info.absolutePath();
    if (!m_watcher.directories().contains(dir) && QFileInfo::exists(dir)) {
        m_watcher.addPath(dir);
    }
    if (info.exists() && !m_watcher.files().contains(m_path)) {
        m_watcher.addPath(m_path);
    }
}

int GlobalShortcuts::nextNumber() const
{
    int top = 0;
    for (const AppShortcut& s : m_shortcuts) {
        top = std::max(top, s.number);
    }
    return top + 1;
}

void GlobalShortcuts::save()
{
    if (vela::saveAppShortcuts(m_shortcuts, m_path)) {
        emit saved();
    }
}

QList<PortalShortcut> GlobalShortcuts::listFor(const QString& appId) const
{
    QList<PortalShortcut> out;
    for (const AppShortcut& s : m_shortcuts) {
        if (s.appId != vela::appShortcutField(appId)) {
            continue;
        }
        QVariantMap props { { QStringLiteral("description"), s.description } };
        props.insert(QStringLiteral("trigger_description"), s.combo);
        out.append({ s.id, props });
    }
    return out;
}

// The file changed under us (Settings): the apps hear about it.
void GlobalShortcuts::reloadFromFile()
{
    const QList<AppShortcut> fresh = vela::loadAppShortcuts(m_path);
    auto same = [](const QList<AppShortcut>& a, const QList<AppShortcut>& b) {
        return vela::serializeAppShortcuts(a) == vela::serializeAppShortcuts(b);
    };
    if (same(fresh, m_shortcuts)) {
        return;
    }
    m_shortcuts = fresh;
    for (ShortcutSession* session : std::as_const(m_sessions)) {
        emit ShortcutsChanged(QDBusObjectPath(session->path()), listFor(session->appId()));
    }
}

void GlobalShortcuts::closeSession(const QString& path)
{
    ShortcutSession* session = m_sessions.take(path);
    if (!session) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(path);
    emit session->Closed();
    session->deleteLater();
}

void GlobalShortcuts::reply(const QDBusMessage& message, uint response, const QVariantMap& results)
{
    QDBusConnection::sessionBus().send(message.createReply(QVariantList { response, results }));
}

uint GlobalShortcuts::CreateSession(const QDBusObjectPath&, const QDBusObjectPath& sessionHandle, const QString& appId,
    const QVariantMap&, QVariantMap& results)
{
    const QString path = sessionHandle.path();
    if (m_sessions.contains(path)) {
        return 2;
    }
    auto* session = new ShortcutSession(this, path, vela::appShortcutField(appId));
    if (!QDBusConnection::sessionBus().registerObject(path, session,
            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllProperties)) {
        delete session;
        return 2;
    }
    m_sessions.insert(path, session);
    results.insert(QStringLiteral("session_id"), path);
    return 0;
}

uint GlobalShortcuts::ListShortcuts(const QDBusObjectPath&, const QDBusObjectPath& sessionHandle, QVariantMap& results)
{
    ShortcutSession* session = m_sessions.value(sessionHandle.path());
    if (!session) {
        return 2;
    }
    results.insert(QStringLiteral("shortcuts"), QVariant::fromValue(listFor(session->appId())));
    return 0;
}

uint GlobalShortcuts::BindShortcuts(const QDBusObjectPath&, const QDBusObjectPath& sessionHandle,
    const QList<PortalShortcut>& shortcuts, const QString&, const QVariantMap&, QVariantMap& results)
{
    ShortcutSession* session = m_sessions.value(sessionHandle.path());
    if (!session) {
        return 2;
    }
    const QString appId = session->appId();
    Pending pending;
    pending.appId = appId;
    int number = nextNumber();
    QStringList lines;
    for (const PortalShortcut& wanted : shortcuts) {
        const QString id = vela::appShortcutField(wanted.id);
        const QString description = vela::appShortcutField(wanted.properties.value(QStringLiteral("description")).toString());
        if (id.isEmpty()) {
            continue;
        }
        auto known = std::find_if(m_shortcuts.begin(), m_shortcuts.end(),
            [&](const AppShortcut& s) { return s.appId == appId && s.id == id; });
        if (known != m_shortcuts.end()) {
            known->description = description.isEmpty() ? known->description : description;
            continue; // the user has already answered for this one
        }
        QString combo = vela::triggerFromPortal(wanted.properties.value(QStringLiteral("preferred_trigger")).toString());
        // A key someone else has stays theirs: this one waits for Settings.
        auto taken = [&](const QString& c) {
            return std::any_of(m_shortcuts.cbegin(), m_shortcuts.cend(), [&](const AppShortcut& s) { return s.combo.compare(c, Qt::CaseInsensitive) == 0; })
                || std::any_of(pending.added.cbegin(), pending.added.cend(), [&](const AppShortcut& s) { return s.combo.compare(c, Qt::CaseInsensitive) == 0; });
        };
        if (!combo.isEmpty() && taken(combo)) {
            combo.clear();
        }
        pending.added.append({ number++, combo, appId, id, description });
        lines << (description.isEmpty() ? id : description) + QStringLiteral(": ")
                + (combo.isEmpty() ? tr("no key yet, choose it in Settings") : combo);
    }
    pending.requested = shortcuts;

    if (pending.added.isEmpty()) {
        save();
        results.insert(QStringLiteral("shortcuts"), QVariant::fromValue(listFor(appId)));
        return 0;
    }
    // New ones: the user decides.
    setDelayedReply(true);
    pending.message = message();
    const int request = m_nextRequest++;
    m_pending.insert(request, pending);
    emit bindRequested(request, appId.isEmpty() ? tr("An app") : appId, lines.join(u'\n'));
    Q_UNUSED(results)
    return 0;
}

void GlobalShortcuts::answer(int request, bool allowed)
{
    if (!m_pending.contains(request)) {
        return;
    }
    const Pending pending = m_pending.take(request);
    QVariantMap results;
    if (!allowed) {
        reply(pending.message, 1, results);
        return;
    }
    m_shortcuts += pending.added;
    save();
    results.insert(QStringLiteral("shortcuts"), QVariant::fromValue(listFor(pending.appId)));
    reply(pending.message, 0, results);
}

void GlobalShortcuts::keyEvent(int number, bool pressed)
{
    const auto shortcut = std::find_if(m_shortcuts.cbegin(), m_shortcuts.cend(),
        [&](const AppShortcut& s) { return s.number == number; });
    if (shortcut == m_shortcuts.cend()) {
        return;
    }
    const qulonglong time = qulonglong(QDateTime::currentMSecsSinceEpoch());
    for (ShortcutSession* session : std::as_const(m_sessions)) {
        if (session->appId() != shortcut->appId) {
            continue;
        }
        if (pressed) {
            emit Activated(QDBusObjectPath(session->path()), shortcut->id, time, {});
        } else {
            emit Deactivated(QDBusObjectPath(session->path()), shortcut->id, time, {});
        }
    }
}
