// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// The widgets board's system card: the processor percentage and the memory
// figures from /proc's text; the GPU card's nvidia-smi line.
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
    void nvidiaSmi()
    {
        const auto gpu = SystemStats::parseNvidiaSmi("NVIDIA GeForce RTX 4060, 19, 426, 8188, 42");
        QVERIFY(gpu);
        QCOMPARE(gpu->name, QStringLiteral("NVIDIA GeForce RTX 4060"));
        QCOMPARE(gpu->load, 19);
        QCOMPARE(gpu->memoryUsedMiB, 426.0);
        QCOMPARE(gpu->memoryTotalMiB, 8188.0);
        QCOMPARE(gpu->temperature, 42);
        QCOMPARE(SystemStats::parseNvidiaSmi("Tesla, 5, 10, 100, [N/A]")->temperature, -1);
        QVERIFY(!SystemStats::parseNvidiaSmi("No devices were found"));
        QVERIFY(!SystemStats::parseNvidiaSmi(""));
    }
    void nvidiaApps()
    {
        const auto apps = SystemStats::parseNvidiaApps(
            "    Processes\n"
            "        Process ID                        : 1\n"
            "            Name                          : /usr/bin/vela-shell\n"
            "            Used GPU Memory               : 173 MiB\n"
            "        Process ID                        : 2\n"
            "            Name                          : Z:\\Games\\game.exe\n"
            "            Used GPU Memory               : 900 MiB\n"
            "        Process ID                        : 3\n"
            "            Name                          : /usr/bin/vela-shell\n"
            "            Used GPU Memory               : 7 MiB\n"
            "        Process ID                        : 4\n"
            "            Name                          : Xwayland\n"
            "            Used GPU Memory               : Not available\n");
        QCOMPARE(apps.size(), 2);
        QCOMPARE(apps.at(0).first, QStringLiteral("game.exe"));
        QCOMPARE(apps.at(0).second, 900.0);
        QCOMPARE(apps.at(1).first, QStringLiteral("vela-shell"));
        QCOMPARE(apps.at(1).second, 180.0);
        QVERIFY(SystemStats::parseNvidiaApps("No running processes found").isEmpty());
    }
};

QTEST_GUILESS_MAIN(SystemStatsTest)
#include "systemstats.moc"
