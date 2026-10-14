// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// Do not disturb: the user's own choice OR the screen being shared. Popups
// are held back (the notification center keeps them) and the user's choice
// is never overwritten.

#include "notifications.h"

#include <QSettings>
#include <QStandardPaths>
#include <QTest>

class NotificationSilenceTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void sharingSilencesAndRestores()
    {
        NotificationServer s;
        s.Notify("a", 0, "", "one", "", {}, {}, 5000);
        QCOMPARE(s.count(), 1); // a popup

        s.setCaptureSilenced(true);
        QVERIFY(s.silenced());
        QVERIFY(!s.doNotDisturb());
        s.Notify("a", 0, "", "two", "", {}, {}, 5000);
        QCOMPARE(s.count(), 1); // held back, in the center
        QCOMPARE(s.history()->property("count").toInt(), 1);

        s.setCaptureSilenced(false);
        QVERIFY(!s.silenced());
        s.Notify("a", 0, "", "three", "", {}, {}, 5000);
        QCOMPARE(s.count(), 2);
    }
    void manualChoiceSurvivesSharing()
    {
        NotificationServer s;
        s.setDoNotDisturb(true);
        s.setCaptureSilenced(true);
        s.setCaptureSilenced(false);
        QVERIFY(s.doNotDisturb());
        QVERIFY(s.silenced());
        s.setDoNotDisturb(false);
        QVERIFY(!s.silenced());
    }
    // Notifications of one app are one group (contiguous, the newest group on
    // top), counted per app and cleared per app.
    void groupsByApp()
    {
        NotificationServer s;
        s.setDoNotDisturb(true); // straight to the center
        s.Notify("a", 0, "", "a1", "", {}, {}, 5000);
        s.Notify("b", 0, "", "b1", "", {}, {}, 5000);
        s.Notify("a", 0, "", "a2", "", {}, {}, 5000);
        auto* h = s.history();
        auto app = [&](int row) { return h->property("count").toInt() > row ? qobject_cast<QAbstractItemModel*>(h)->index(row, 0).data(Qt::UserRole + 2).toString() : QString(); };
        QCOMPARE(app(0), QString("a"));
        QCOMPARE(app(1), QString("a"));
        QCOMPARE(app(2), QString("b"));
        QVariantMap counts;
        QMetaObject::invokeMethod(h, "appCounts", Q_RETURN_ARG(QVariantMap, counts));
        QCOMPARE(counts.value("a").toInt(), 2);
        QCOMPARE(counts.value("b").toInt(), 1);

        s.clearApp("a");
        QCOMPARE(h->property("count").toInt(), 1);
        QCOMPARE(app(0), QString("b"));
        s.dismissFromHistory(qobject_cast<QAbstractItemModel*>(h)->index(0, 0).data(Qt::UserRole + 1).toUInt());
        QCOMPARE(h->property("count").toInt(), 0);
    }
};

QTEST_GUILESS_MAIN(NotificationSilenceTest)
#include "notification_silence.moc"
