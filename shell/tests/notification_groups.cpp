// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// The notification center groups by app: one app's notifications are
// contiguous (the newest group on top), counted per app and cleared per app.

#include "notifications.h"

#include <QAbstractItemModel>
#include <QStandardPaths>
#include <QTest>

class NotificationGroupsTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void groupsByApp()
    {
        NotificationServer s;
        s.setDoNotDisturb(true); // straight to the center
        s.Notify("a", 0, "", "a1", "", {}, {}, 5000);
        s.Notify("b", 0, "", "b1", "", {}, {}, 5000);
        s.Notify("a", 0, "", "a2", "", {}, {}, 5000);
        auto* h = s.history();
        auto* model = qobject_cast<QAbstractItemModel*>(h);
        auto app = [&](int row) {
            return h->property("count").toInt() > row ? model->index(row, 0).data(Qt::UserRole + 2).toString()
                                                      : QString();
        };
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
        s.dismissFromHistory(model->index(0, 0).data(Qt::UserRole + 1).toUInt());
        QCOMPARE(h->property("count").toInt(), 0);
    }
};

QTEST_GUILESS_MAIN(NotificationGroupsTest)
#include "notification_groups.moc"
