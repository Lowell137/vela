// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// The GlobalShortcuts portal backend, over a private session bus: a session,
// a new shortcut that waits for the user, the answer, the saved file, the
// press and release signals, and what an already-known shortcut does.

#include "globalshortcuts.h"

#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using vela::PortalShortcut;

class GlobalShortcutsTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void triggers()
    {
        QCOMPARE(vela::triggerFromPortal("CTRL+ALT+a"), QString("CTRL+ALT+a"));
        QCOMPARE(vela::triggerFromPortal("LOGO+space"), QString("Super+space"));
        QCOMPARE(vela::triggerFromPortal(""), QString());
    }
    void fileRoundTrip()
    {
        const QList<vela::AppShortcut> in { { 3, "Super+M", "org.app", "mute", "Mute | all" }, { 4, "", "org.app", "talk", "Talk" } };
        const QList<vela::AppShortcut> out = vela::parseAppShortcuts(vela::serializeAppShortcuts(in));
        QCOMPARE(out.size(), 2);
        QCOMPARE(out.at(0).number, 3);
        QCOMPARE(out.at(0).combo, QString("Super+M"));
        QCOMPARE(out.at(0).description, QString("Mute _ all")); // the bar can't be written
        QCOMPARE(out.at(1).combo, QString());
        QVERIFY(vela::parseAppShortcuts("junk\n0|x|a|b|c\n").isEmpty());
    }
    void bindAnswerPressRelease()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("app-shortcuts.conf");
        GlobalShortcuts gs;
        gs.setPath(file);
        if (!gs.registerService()) {
            QSKIP("no private session bus");
        }
        // The app: another connection, or Qt would call the object directly.
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "app");
        // No QDBusInterface: its introspection would block this very event loop.
        struct {
            QDBusConnection& bus;
            QDBusPendingCall asyncCallWithArgumentList(const QString& method, const QVariantList& args)
            {
                QDBusMessage m = QDBusMessage::createMethodCall("org.freedesktop.impl.portal.desktop.vela",
                    "/org/freedesktop/portal/desktop", "org.freedesktop.impl.portal.GlobalShortcuts", method);
                m.setArguments(args);
                return bus.asyncCall(m);
            }
        } portal { bus };
        const QDBusObjectPath handle("/req/1"), session("/session/1");

        auto call = [&](const QString& method, const QVariantList& args) {
            auto* watcher = new QDBusPendingCallWatcher(portal.asyncCallWithArgumentList(method, args));
            QSignalSpy done(watcher, &QDBusPendingCallWatcher::finished);
            [&] { QVERIFY(done.wait(3000)); }();
            return QDBusMessage(watcher->reply());
        };

        QDBusMessage reply = call("CreateSession", { QVariant::fromValue(handle), QVariant::fromValue(session), "org.app", QVariantMap() });
        QCOMPARE(reply.type(), QDBusMessage::ReplyMessage);
        QCOMPARE(reply.arguments().at(0).toUInt(), 0u);

        // A new shortcut: the user is asked first.
        QSignalSpy asked(&gs, &GlobalShortcuts::bindRequested);
        QList<PortalShortcut> wanted { { "talk", { { "description", "Push to talk" }, { "preferred_trigger", "CTRL+ALT+t" } } } };
        auto* bind = new QDBusPendingCallWatcher(portal.asyncCallWithArgumentList("BindShortcuts",
            { QVariant::fromValue(handle), QVariant::fromValue(session), QVariant::fromValue(wanted), "", QVariantMap() }));
        QSignalSpy bound(bind, &QDBusPendingCallWatcher::finished);
        QVERIFY(asked.wait(3000));
        QVERIFY(bound.isEmpty()); // still waiting
        gs.answer(asked.first().first().toInt(), true);
        QVERIFY(bound.wait(3000));
        QDBusMessage bindReply = bind->reply();
        QCOMPARE(bindReply.type(), QDBusMessage::ReplyMessage);
        QCOMPARE(bindReply.arguments().at(0).toUInt(), 0u);
        QCOMPARE(vela::loadAppShortcuts(file).value(0).combo, QString("CTRL+ALT+t"));

        // The compositor says it went down and up.
        QSignalSpy activated(&gs, &GlobalShortcuts::Activated), deactivated(&gs, &GlobalShortcuts::Deactivated);
        const int number = vela::loadAppShortcuts(file).value(0).number;
        gs.keyEvent(number, true);
        gs.keyEvent(number, false);
        QCOMPARE(activated.size(), 1);
        QCOMPARE(activated.first().at(1).toString(), QString("talk"));
        QCOMPARE(deactivated.size(), 1);

        // Asked again (next run of the app): known, so no question.
        asked.clear();
        reply = call("BindShortcuts", { QVariant::fromValue(handle), QVariant::fromValue(session), QVariant::fromValue(wanted), "", QVariantMap() });
        QCOMPARE(reply.arguments().at(0).toUInt(), 0u);
        QVERIFY(asked.isEmpty());

        // A refusal keeps nothing.
        const QDBusObjectPath other("/session/2");
        call("CreateSession", { QVariant::fromValue(handle), QVariant::fromValue(other), "org.other", QVariantMap() });
        QList<PortalShortcut> more { { "x", { { "description", "X" } } } };
        auto* refused = new QDBusPendingCallWatcher(portal.asyncCallWithArgumentList("BindShortcuts",
            { QVariant::fromValue(handle), QVariant::fromValue(other), QVariant::fromValue(more), "", QVariantMap() }));
        QSignalSpy refusedDone(refused, &QDBusPendingCallWatcher::finished);
        QVERIFY(asked.wait(3000));
        gs.answer(asked.last().first().toInt(), false);
        QVERIFY(refusedDone.wait(3000));
        QCOMPARE(refused->reply().arguments().at(0).toUInt(), 1u);
        QCOMPARE(vela::loadAppShortcuts(file).size(), 1);
    }
};

QTEST_MAIN(GlobalShortcutsTest)
#include "globalshortcuts.moc"
