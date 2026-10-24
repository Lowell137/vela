// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// The widgets board's system card: the processor percentage and the memory
// figures from /proc's text.
//
//   ctest --test-dir build -R systemstats

#include "systemstats.h"

#include <QTest>

class SystemStatsTest : public QObject {
    Q_OBJECT

private slots:
    void cpuPercent()
    {
        //                          user nice sys idle iowait
        const QList<qulonglong> a { 100, 0, 100, 800, 0 };
        QCOMPARE(SystemStats::cpuPercent(a, { 150, 0, 150, 900, 0 }), 50); // 100 busy of 200
        QCOMPARE(SystemStats::cpuPercent(a, { 100, 0, 100, 1000, 0 }), 0); // all idle
        QCOMPARE(SystemStats::cpuPercent(a, { 200, 0, 200, 800, 0 }), 100);
        QCOMPARE(SystemStats::cpuPercent(a, a), 0); // no time passed
        QCOMPARE(SystemStats::cpuPercent({}, a), 0); // no earlier reading
    }
    void memory()
    {
        const auto [total, available] = SystemStats::memoryKb("MemTotal:       16000000 kB\nMemFree:  1 kB\nMemAvailable:    4000000 kB\n");
        QCOMPARE(total, 16000000ull);
        QCOMPARE(available, 4000000ull);
        QCOMPARE(SystemStats::memoryKb("nothing").first, 0ull);
    }
};

QTEST_GUILESS_MAIN(SystemStatsTest)
#include "systemstats.moc"
