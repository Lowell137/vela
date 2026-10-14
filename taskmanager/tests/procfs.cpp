// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// What the Task Manager makes of /proc's text, and how its lists follow.

#include "procfs.h"
#include "rowmodel.h"

#include <QSignalSpy>
#include <QtTest>

class ProcfsTest : public QObject {
    Q_OBJECT

private slots:
    void statWithAStrangeName()
    {
        // The name holds a space and a parenthesis: it ends at the last one.
        const auto stat = procfs::parseStat("4242 (Web (Content) x) S 100 4242 4242 0 -1 4194560 1 2 3 4 "
                                            "70 30 0 0 20 0 9 0 123456 1000000 2000 18446744073709551615 0 0");
        QVERIFY(stat.has_value());
        QCOMPARE(stat->name, "Web (Content) x");
        QCOMPARE(stat->state, 'S');
        QCOMPARE(stat->parent, 100);
        QCOMPARE(stat->ticks, 100u); // 70 user + 30 system
        QCOMPARE(stat->threads, 9);
        QCOMPARE(stat->start, 123456u);
        QVERIFY(!procfs::parseStat("garbage").has_value());
    }

    void cpuCountsIdleAndWaitingAsFree()
    {
        const QList<procfs::Cpu> cpus = procfs::parseCpu("cpu  100 0 50 800 50 0 0 0 0 0\ncpu0 60 0 20 400 20 0 0 0 0 0\nintr 1 2 3\n");
        QCOMPARE(cpus.size(), 2);
        QCOMPARE(cpus[0].total, 1000u);
        QCOMPARE(cpus[0].busy, 150u);
        QCOMPARE(cpus[1].busy, 80u);
    }

    void meminfoInBytes()
    {
        const auto mem = procfs::parseMeminfo("MemTotal:       16000 kB\nMemAvailable:    4000 kB\nHugePages_Total:       0\n");
        QCOMPARE(mem.value("MemTotal"), 16000u * 1024);
        QCOMPARE(mem.value("MemAvailable"), 4000u * 1024);
    }

    void disksAndNetwork()
    {
        const QList<procfs::Disk> disks = procfs::parseDiskstats(" 259       0 nvme0n1 100 0 2000 50 200 0 4000 80 0 130 130 0 0 0 0\n");
        QCOMPARE(disks.size(), 1);
        QCOMPARE(disks[0].name, "nvme0n1");
        QCOMPARE(disks[0].read, 2000u * 512);
        QCOMPARE(disks[0].written, 4000u * 512);
        QCOMPARE(disks[0].busy, 130u);

        const QList<procfs::Net> nets = procfs::parseNetDev("Inter-|   Receive  |  Transmit\n face |bytes ...\n"
                                                            "  eth0: 5000 10 0 0 0 0 0 0 7000 12 0 0 0 0 0 0\n");
        QCOMPARE(nets.size(), 1);
        QCOMPARE(nets[0].name, "eth0");
        QCOMPARE(nets[0].received, 5000u);
        QCOMPARE(nets[0].sent, 7000u);
    }

    void rowsMoveInsteadOfStartingOver()
    {
        RowModel model;
        QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
        model.set({ { "a", { { "n", 1 } } }, { "b", { { "n", 2 } } }, { "c", { { "n", 3 } } } });
        QCOMPARE(model.rowCount(), 3);
        model.set({ { "c", { { "n", 30 } } }, { "a", { { "n", 10 } } }, { "d", { { "n", 4 } } } }); // b gone, d new, c first
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.rows().at(0).key, "c");
        QCOMPARE(model.data(model.index(0), Qt::UserRole).toMap().value("n").toInt(), 30);
        QCOMPARE(model.rows().at(2).key, "d");
        QCOMPARE(removed.size(), 1);
        QCOMPARE(resets.size(), 0); // the view keeps its place
    }
};

QTEST_GUILESS_MAIN(ProcfsTest)
#include "procfs.moc"
