// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// What the Event Viewer makes of journalctl's lines and how its list holds them.

#include "eventmodel.h"
#include "journal.h"

#include <QSignalSpy>
#include <QtTest>

using namespace journal;

static Entry entry(const QString& cursor, qint64 usec)
{
    Entry e;
    e.cursor = cursor;
    e.usec = usec;
    return e;
}

class JournalTest : public QObject {
    Q_OBJECT

private slots:
    void aNormalLine()
    {
        const auto e = parseLine("{\"__CURSOR\":\"s=1\",\"__REALTIME_TIMESTAMP\":\"1760000000123456\",\"PRIORITY\":\"3\","
                                 "\"MESSAGE\":\"disk full\\nsecond line\",\"_SYSTEMD_UNIT\":\"a.service\",\"SYSLOG_IDENTIFIER\":\"a\","
                                 "\"_PID\":\"42\",\"SYSLOG_FACILITY\":\"3\",\"_BOOT_ID\":\"b1\"}");
        QVERIFY(e.has_value());
        QCOMPARE(e->priority, 3);
        QCOMPARE(e->usec, 1760000000123456);
        QCOMPARE(e->pid, 42);
        QCOMPARE(e->source(), "a.service");
        QCOMPARE(e->summary(), "disk full");
        QCOMPARE(e->message, "disk full\nsecond line");
        QCOMPARE(e->facility, 3);
        QCOMPARE(e->bootId, "b1");
    }

    void missingFields()
    {
        const auto e = parseLine("{\"__CURSOR\":\"s=2\"}");
        QVERIFY(e.has_value());
        QCOMPARE(e->priority, 6); // no PRIORITY: informational
        QCOMPARE(e->usec, 0);
        QVERIFY(e->message.isEmpty());
        QCOMPARE(e->uid, -1);
        QVERIFY(!parseLine("{\"MESSAGE\":\"no cursor\"}").has_value());
        QVERIFY(!parseLine("not json").has_value());
        QVERIFY(!parseLine("").has_value());
        QCOMPARE(parseLine("{\"__CURSOR\":\"c\",\"PRIORITY\":\"99\"}")->priority, 7);
    }

    void identifierWhenNoUnit()
    {
        const auto e = parseLine("{\"__CURSOR\":\"c\",\"SYSLOG_IDENTIFIER\":\"kernel\",\"_COMM\":\"x\"}");
        QCOMPARE(e->source(), "kernel");
        QCOMPARE(parseLine("{\"__CURSOR\":\"c\",\"_COMM\":\"sudo\"}")->source(), "sudo");
    }

    void messageAsByteArray()
    {
        // "hé" in UTF-8 as numbers, journalctl's way of printing what isn't text.
        const auto e = parseLine("{\"__CURSOR\":\"c\",\"MESSAGE\":[104,195,169]}");
        QCOMPARE(e->message, QString::fromUtf8("h\xc3\xa9"));
        // Not UTF-8 at all: replacement characters, no failure.
        const auto bad = parseLine("{\"__CURSOR\":\"c\",\"MESSAGE\":[104,255,105]}");
        QVERIFY(bad.has_value());
        QVERIFY(bad->message.startsWith('h') && bad->message.endsWith('i'));
        QVERIFY(bad->message.contains(QChar::ReplacementCharacter));
        // The field repeated: strings in an array, and null for a field too large.
        QCOMPARE(parseLine("{\"__CURSOR\":\"c\",\"MESSAGE\":[\"a\",\"b\"]}")->message, "a\nb");
        QVERIFY(parseLine("{\"__CURSOR\":\"c\",\"MESSAGE\":null}")->message.isEmpty());
    }

    void rawBadUtf8InTheLine()
    {
        QByteArray line = "{\"__CURSOR\":\"c\",\"MESSAGE\":\"a";
        line += char(0xff);
        line += "b\"}";
        // Whatever Qt makes of it, it mustn't crash; a line that parses keeps its cursor.
        const auto e = parseLine(line);
        QVERIFY(!e.has_value() || e->cursor == "c");
    }

    void hugeLinesAreSkipped()
    {
        QByteArray line = "{\"__CURSOR\":\"c\",\"MESSAGE\":\"" + QByteArray(maxLineBytes, 'x') + "\"}";
        QVERIFY(!parseLine(line).has_value());
        QByteArray fits = "{\"__CURSOR\":\"c\",\"MESSAGE\":\"" + QByteArray(100000, 'x') + "\"}";
        QCOMPARE(parseLine(fits)->message.size(), 100000);
    }

    void argumentsForTheViews()
    {
        Filter f;
        QCOMPARE(arguments(f, false), QStringList({ "--no-pager", "-o", "json", "-r" }));
        QCOMPARE(arguments(f, true), QStringList({ "--no-pager", "-o", "json", "-f", "-n", "0" }));

        f.view = "warnings";
        QVERIFY(arguments(f, false).join(' ').contains("-p 4"));
        f.view = "critical";
        f.level = 3; // the stricter wins
        QVERIFY(arguments(f, false).join(' ').contains("-p 2"));
        f.level = 1;
        QVERIFY(arguments(f, false).join(' ').contains("-p 1"));

        f = {};
        f.view = "previous";
        QVERIFY(arguments(f, false).join(' ').contains("-b -1"));
        f.view = "boot";
        QVERIFY(arguments(f, false).join(' ').contains("-b 0"));
        f.view = "kernel";
        f.range = "boot";
        QVERIFY(arguments(f, false).contains("-k"));
        QVERIFY(arguments(f, false).join(' ').contains("-b 0"));
    }

    void argumentsForRangeUnitAndScope()
    {
        Filter f;
        f.range = "1h";
        f.unit = "sshd";
        f.scope = Scope::User;
        const QStringList a = arguments(f, false);
        QVERIFY(a.contains("--since=-1h"));
        QVERIFY(a.contains("_SYSTEMD_UNIT=sshd.service"));
        QVERIFY(a.contains("--user"));
        f.unit = "x.timer";
        QVERIFY(arguments(f, false).contains("_SYSTEMD_UNIT=x.timer"));
        f.range = "nonsense"; // unknown: no limit rather than a journalctl error
        QVERIFY(!arguments(f, false).join(' ').contains("--since"));
    }

    void securityKeepsTheUnitInBothHalves()
    {
        Filter f;
        f.view = "security";
        f.unit = "sshd.service";
        const QStringList a = arguments(f, false);
        const qsizetype plus = a.indexOf("+");
        QVERIFY(plus > 0);
        QVERIFY(a.mid(0, plus).contains("_SYSTEMD_UNIT=sshd.service"));
        QVERIFY(a.mid(plus).contains("_SYSTEMD_UNIT=sshd.service"));
        QVERIFY(a.mid(plus).contains("SYSLOG_FACILITY=10"));
        QVERIFY(a.contains("_COMM=sudo"));
    }

    void searchText()
    {
        Entry e = entry("c", 1);
        e.message = "Failed password for root";
        e.unit = "sshd.service";
        e.identifier = "sshd-session";
        QVERIFY(matches(e, ""));
        QVERIFY(matches(e, "FAILED"));
        QVERIFY(matches(e, "SSHD.serv"));
        QVERIFY(matches(e, "session"));
        QVERIFY(!matches(e, "kernel"));
    }

    void modelHoldsTheCapNewestFirst()
    {
        EventModel model;
        model.setCap(3);
        QSignalSpy counts(&model, &EventModel::countChanged);
        QVERIFY(!model.appendOlder({ entry("5", 5), entry("4", 4) }));
        QCOMPARE(model.count(), 2);
        // The load goes on: "4" again is dropped, the cap stops the rest.
        QVERIFY(model.appendOlder({ entry("4", 4), entry("3", 3), entry("2", 2) }));
        QCOMPARE(model.count(), 3);
        QCOMPARE(model.entries().last().cursor, "3");
        QCOMPARE(counts.count(), 2);
    }

    void followPutsNewOnTopAndDropsTheOldest()
    {
        EventModel model;
        model.setCap(3);
        model.appendOlder({ entry("3", 3), entry("2", 2), entry("1", 1) });
        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
        // "3" is known (the follow started inside the load): once.
        model.prependNewer({ entry("3", 3), entry("4", 4), entry("5", 5) });
        QCOMPARE(model.count(), 3);
        QCOMPARE(model.entries().at(0).cursor, "5");
        QCOMPARE(model.entries().at(1).cursor, "4");
        QCOMPARE(model.entries().at(2).cursor, "3");
        QCOMPARE(inserted.count(), 1);
        QCOMPARE(removed.count(), 1);
        // A dropped one may come again as new.
        model.prependNewer({ entry("1", 1) });
        QCOMPARE(model.entries().first().cursor, "1");
        QCOMPARE(model.count(), 3);
    }

    void clearForgetsTheCursors()
    {
        EventModel model;
        model.appendOlder({ entry("a", 1) });
        model.clear();
        QCOMPARE(model.count(), 0);
        model.appendOlder({ entry("a", 1) });
        QCOMPARE(model.count(), 1);
    }
};

QTEST_GUILESS_MAIN(JournalTest)
#include "eventviewer.moc"
