// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QVariant>

// The file dialogs of the Qt Quick apps (FileDialog in QML) through
// xdg-desktop-portal, that is Vela's own Explorer as a file chooser in Vela
// (shell/src/filechooser.h; KDE's after it, vela-portals.conf). KDE's platform theme opens its own dialog only in
// QApplication (widgets) apps: a QGuiApplication got Qt Quick's bare fallback.
// Qt's "xdgdesktopportal" theme keeps KDE's theme underneath (it picks it from
// XDG_CURRENT_DESKTOP) for everything else. Only over KDE's theme or none: a
// theme the user chose (qt6ct) stays.
//
// Construct before QGuiApplication, restore() right after: Qt reads the
// variable only then, and the apps started from here must not inherit it.
namespace vela {
class PortalFileDialogs {
public:
    PortalFileDialogs()
        : m_old(qgetenv("QT_QPA_PLATFORMTHEME"))
        , m_wasSet(qEnvironmentVariableIsSet("QT_QPA_PLATFORMTHEME"))
    {
        m_changed = m_old.isEmpty() || m_old == "kde";
        if (m_changed) {
            qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
        }
    }

    void restore()
    {
        if (!m_changed) {
            return;
        }
        if (m_wasSet) {
            qputenv("QT_QPA_PLATFORMTHEME", m_old);
        } else {
            qunsetenv("QT_QPA_PLATFORMTHEME");
        }
        m_changed = false;

        // Qt Quick's own dialog until the portal says it has a file chooser:
        // without one (no portal, no backend, no answer yet) the portal theme
        // hands the dialog to KDE's theme, whose widget dialog aborts a
        // QGuiApplication. Qt's theme asked the same question while
        // QGuiApplication was built, on the same connection: the answers
        // arrive in order, so when this one comes Qt knows it too.
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"),
            QStringLiteral("/org/freedesktop/portal/desktop"), QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("Get"));
        message << QStringLiteral("org.freedesktop.portal.FileChooser") << QStringLiteral("version");
        auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message),
                                                    QCoreApplication::instance());
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher, [](QDBusPendingCallWatcher* call) {
            const QDBusPendingReply<QVariant> reply = *call;
            if (reply.isValid() && reply.value().toUInt() > 0) {
                QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, false);
            }
            call->deleteLater();
        });
    }

private:
    QByteArray m_old;
    bool m_wasSet;
    bool m_changed;
};
} // namespace vela
