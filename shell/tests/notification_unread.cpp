// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// The taskbar button's count from the notifications of an app: matched by the
// desktop-entry hint or the name, in either spelling of the desktop id, and
// reset by markSeen().

#include "notifications.h"

#include <QStandardPaths>
#include <QTest>

class NotificationUnreadTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void countsByEntryOrName()
    {
        NotificationServer s;
        s.Notify("Discord", 0, "", "hi", "", {}, { { "desktop-entry", "discord" } }, 5000);
        s.Notify("Discord", 0, "", "hi again", "", {}, { { "desktop-entry", "discord" } }, 5000);
        s.Notify("discord", 0, "", "by name", "", {}, {}, 5000);
        s.Notify("other", 0, "", "no", "", {}, {}, 5000);
        QCOMPARE(s.unreadFor("discord.desktop"), 3);
        QCOMPARE(s.unreadFor("com.discordapp.Discord.desktop"), 3);
        QCOMPARE(s.unreadFor("firefox.desktop"), 0);
        QCOMPARE(s.unreadFor(""), 0);
    }
    void seenResetsOnlyThatApp()
    {
        NotificationServer s;
        s.Notify("a", 0, "", "one", "", {}, {}, 5000);
        s.Notify("b", 0, "", "one", "", {}, {}, 5000);
        const int before = s.unreadRevision();
        s.markSeen("a.desktop");
        QVERIFY(s.unreadRevision() > before);
        QCOMPARE(s.unreadFor("a.desktop"), 0);
        QCOMPARE(s.unreadFor("b.desktop"), 1);
        // New ones after it was seen count again.
        QTest::qWait(5);
        s.Notify("a", 0, "", "two", "", {}, {}, 5000);
        QCOMPARE(s.unreadFor("a.desktop"), 1);
    }
    void replacementsCountAsMessages()
    {
        NotificationServer s;
        const uint id = s.Notify("Discord", 0, "", "one", "", {}, { { "desktop-entry", "discord" } }, 5000);
        s.Notify("Discord", id, "", "two", "", {}, { { "desktop-entry", "discord" } }, 5000);
        s.Notify("Discord", id, "", "three", "", {}, { { "desktop-entry", "discord" } }, 5000);
        QCOMPARE(s.unreadFor("discord.desktop"), 3);
        s.markSeen("discord.desktop");
        QCOMPARE(s.unreadFor("discord.desktop"), 0);
    }
    void dismissedOnesDoNotCount()
    {
        NotificationServer s;
        s.setDoNotDisturb(true); // straight into the center
        s.Notify("a", 0, "", "one", "", {}, {}, 5000);
        QCOMPARE(s.unreadFor("a"), 1);
        s.clearApp("a");
        QCOMPARE(s.unreadFor("a"), 0);
    }
};

QTEST_MAIN(NotificationUnreadTest)
#include "notification_unread.moc"
