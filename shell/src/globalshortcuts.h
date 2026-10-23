// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The backend of xdg-desktop-portal's GlobalShortcuts
// (org.freedesktop.impl.portal.GlobalShortcuts): an app (an AppImage, a
// Flatpak...) asks for a key that works whatever window has the focus, such as
// push-to-talk. The user is asked once; the keys are kept in
// app-shortcuts.conf (appshortcuts.h), changed in Settings, and caught by the
// compositor, which tells us when they go down and up.

#include "appshortcuts.h"

#include <QDBusArgument>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QFileSystemWatcher>
#include <QHash>
#include <QList>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

namespace vela {
// a(sa{sv}): a shortcut's id and its properties (description, trigger...).
struct PortalShortcut {
    QString id;
    QVariantMap properties;
};
QDBusArgument& operator<<(QDBusArgument& argument, const PortalShortcut& shortcut);
const QDBusArgument& operator>>(const QDBusArgument& argument, PortalShortcut& shortcut);

// "CTRL+ALT+a", "LOGO+space" (the portal's spelling) to Vela's: "Ctrl+Alt+a",
// "Super+space". Empty for nothing usable.
QString triggerFromPortal(const QString& trigger);
}
Q_DECLARE_METATYPE(vela::PortalShortcut)
Q_DECLARE_METATYPE(QList<vela::PortalShortcut>)

class GlobalShortcuts;

// One app's session (org.freedesktop.impl.portal.Session), at the path the
// portal chose.
class ShortcutSession : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Session")
    Q_PROPERTY(uint version READ version CONSTANT)

public:
    ShortcutSession(GlobalShortcuts* owner, const QString& path, const QString& appId);
    uint version() const { return 1; }
    QString path() const { return m_path; }
    QString appId() const { return m_appId; }

public Q_SLOTS:
    void Close();

Q_SIGNALS:
    void Closed();

private:
    GlobalShortcuts* m_owner;
    QString m_path;
    QString m_appId;
};

class GlobalShortcuts : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.GlobalShortcuts")
    Q_PROPERTY(uint version READ version CONSTANT)

public:
    explicit GlobalShortcuts(QObject* parent = nullptr);
    // Takes the bus name and the object; false if the name is taken.
    bool registerService();
    uint version() const { return 1; }

    // From the compositor: a key went down or up.
    void keyEvent(int number, bool pressed);

    // The user's answer to the question of QML's dialog.
    Q_INVOKABLE void answer(int request, bool allowed);

    // For the FileChooser adaptor, on this object: the reply of the call in
    // progress will come later; that call.
    QDBusMessage delayReply()
    {
        setDelayedReply(true);
        return message();
    }

    // For the tests: the file in `path`.
    void setPath(const QString& path);
    QList<vela::AppShortcut> shortcuts() const { return m_shortcuts; }

public Q_SLOTS:
    // org.freedesktop.impl.portal.GlobalShortcuts
    uint CreateSession(const QDBusObjectPath& handle, const QDBusObjectPath& sessionHandle, const QString& appId,
        const QVariantMap& options, QVariantMap& results);
    uint BindShortcuts(const QDBusObjectPath& handle, const QDBusObjectPath& sessionHandle,
        const QList<vela::PortalShortcut>& shortcuts, const QString& parentWindow, const QVariantMap& options,
        QVariantMap& results);
    uint ListShortcuts(const QDBusObjectPath& handle, const QDBusObjectPath& sessionHandle, QVariantMap& results);

Q_SIGNALS:
    void Activated(const QDBusObjectPath& sessionHandle, const QString& shortcutId, qulonglong timestamp,
        const QVariantMap& options);
    void Deactivated(const QDBusObjectPath& sessionHandle, const QString& shortcutId, qulonglong timestamp,
        const QVariantMap& options);
    void ShortcutsChanged(const QDBusObjectPath& sessionHandle, const QList<vela::PortalShortcut>& shortcuts);

    void saved(); // app-shortcuts.conf was written: the compositor rereads it
    // To QML: an app asks for these new shortcuts; answer(request, ...) replies.
    void bindRequested(int request, const QString& app, const QString& text);

private:
    friend class ShortcutSession;
    struct Pending {
        QDBusMessage message;
        QString appId;
        QList<vela::AppShortcut> added; // new, to keep if allowed
        QList<vela::PortalShortcut> requested; // all of them, for the reply
    };

    void closeSession(const QString& path);
    QList<vela::PortalShortcut> listFor(const QString& appId) const;
    void reply(const QDBusMessage& message, uint response, const QVariantMap& results);
    void save();
    void reloadFromFile();
    void watch();
    int nextNumber() const;

    QList<vela::AppShortcut> m_shortcuts;
    QHash<QString, ShortcutSession*> m_sessions; // by path
    QHash<int, Pending> m_pending;
    int m_nextRequest = 1;
    QString m_path;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
