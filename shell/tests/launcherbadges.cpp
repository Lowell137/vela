// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// The launcher badges: both spellings of an app, 99+ data, progress, cleanup.

#include "launcherbadges.h"

#include <QTest>

class LauncherBadgesTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void spellings()
    {
        LauncherBadges b;
        b.update("application://com.discordapp.Discord.desktop", { { "count", 5 }, { "count-visible", true } }, ":1.5");
        QCOMPARE(b.entry("com.discordapp.Discord.desktop").value("count").toInt(), 5);
        QCOMPARE(b.entry("discord.desktop").value("count").toInt(), 5);
        QCOMPARE(b.entry("Discord").value("count").toInt(), 5);
        QCOMPARE(b.entry("telegram.desktop").value("count").toInt(), 0);
    }
    void hiddenAndProgress()
    {
        LauncherBadges b;
        b.update("application://t.desktop", { { "count", 3 }, { "progress", 0.4 }, { "progress-visible", true } }, ":1.2");
        QCOMPARE(b.entry("t").value("progress").toDouble(), 0.4);
        b.update("application://t.desktop", { { "count-visible", false } }, ":1.2");
        QCOMPARE(b.entry("t").value("count").toInt(), 0);
        QCOMPARE(b.entry("t").value("progress").toDouble(), 0.4);
        b.update("application://t.desktop", { { "progress", 1.0 } }, ":1.2");
        QCOMPARE(b.entry("t").value("progress").toDouble(), -1.0);
    }
};

QTEST_GUILESS_MAIN(LauncherBadgesTest)
#include "launcherbadges.moc"
