// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "clockmath.h"

#include <QTemporaryFile>
#include <QtTest>

using namespace vela::clock;

namespace {
QDateTime at(const char* zone, int y, int mo, int d, int h, int mi, int s = 0)
{
    return QDateTime(QDate(y, mo, d), QTime(h, mi, s), QTimeZone(zone));
}
constexpr int Mon = 1, Tue = 2, Fri = 16, Sun = 64;
}

class ClockMathTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        if (!QTimeZone("Europe/Berlin").isValid() || !QTimeZone("America/New_York").isValid()) {
            QSKIP("no tz database");
        }
    }

    void onceTodayOrTomorrow()
    {
        const QDateTime now = at("Europe/Berlin", 2026, 6, 10, 7, 0);
        QCOMPARE(nextFire(QTime(7, 30), 0, now), at("Europe/Berlin", 2026, 6, 10, 7, 30));
        QCOMPARE(nextFire(QTime(6, 30), 0, now), at("Europe/Berlin", 2026, 6, 11, 6, 30));
        // The very minute that is already running has passed.
        QCOMPARE(nextFire(QTime(7, 0), 0, now), at("Europe/Berlin", 2026, 6, 11, 7, 0));
    }

    void acrossMidnight()
    {
        const QDateTime now = at("Europe/Berlin", 2026, 12, 31, 23, 59, 30);
        QCOMPARE(nextFire(QTime(0, 5), 0, now), at("Europe/Berlin", 2027, 1, 1, 0, 5));
    }

    void repeatDays()
    {
        // 2026-06-13 is a Saturday.
        const QDateTime sat = at("Europe/Berlin", 2026, 6, 13, 12, 0);
        QCOMPARE(nextFire(QTime(8, 0), Mon | Fri, sat), at("Europe/Berlin", 2026, 6, 15, 8, 0));
        QCOMPARE(nextFire(QTime(8, 0), Sun, sat), at("Europe/Berlin", 2026, 6, 14, 8, 0));
        // Monday's alarm that has passed waits a week.
        const QDateTime mon = at("Europe/Berlin", 2026, 6, 15, 9, 0);
        QCOMPARE(nextFire(QTime(8, 0), Mon, mon), at("Europe/Berlin", 2026, 6, 22, 8, 0));
        QCOMPARE(nextFire(QTime(8, 0), Mon | Tue, mon), at("Europe/Berlin", 2026, 6, 16, 8, 0));
    }

    void dstSpringForward()
    {
        // Berlin jumps 02:00 -> 03:00 on 2026-03-29: 07:00 is 22 hours away.
        const QDateTime now = at("Europe/Berlin", 2026, 3, 28, 8, 0);
        const QDateTime next = nextFire(QTime(7, 0), 0, now);
        QCOMPARE(next, at("Europe/Berlin", 2026, 3, 29, 7, 0));
        QCOMPARE(now.secsTo(next), qint64(22 * 3600));
        // 02:30 does not exist that day: it still rings, after the gap, once.
        const QDateTime gap = nextFire(QTime(2, 30), 0, at("Europe/Berlin", 2026, 3, 29, 1, 0));
        QVERIFY(gap.isValid());
        QVERIFY(gap > at("Europe/Berlin", 2026, 3, 29, 1, 0));
        QCOMPARE(gap.date(), QDate(2026, 3, 29));
    }

    void dstFallBack()
    {
        const QDateTime now = at("Europe/Berlin", 2026, 10, 24, 8, 0);
        const QDateTime next = nextFire(QTime(7, 0), 0, now);
        QCOMPARE(now.secsTo(next), qint64(24 * 3600));
    }

    void timerDeadlines()
    {
        Countdown t;
        t.durationMs = 60000;
        t.left = 60000;
        QVERIFY(!t.paused());
        t.start(1000);
        QCOMPARE(t.remaining(1000), qint64(60000));
        QCOMPARE(t.remaining(31000), qint64(30000));
        QVERIFY(!t.expired(60999));
        QVERIFY(t.expired(61000));
        QCOMPARE(t.remaining(99999), qint64(0));
        t.pause(31000);
        QVERIFY(t.paused());
        QCOMPARE(t.remaining(500000), qint64(30000)); // paused time does not count
        t.start(100000);
        QCOMPARE(t.deadline, qint64(130000));
        t.reset();
        QVERIFY(!t.running);
        QCOMPARE(t.remaining(0), qint64(60000));
    }

    void stopwatchLaps()
    {
        Stopwatch s;
        s.start(1000);
        s.lap(3500);
        s.pause(4000);
        QCOMPARE(s.elapsed(9000), qint64(3000));
        s.start(10000);
        s.lap(10500);
        QCOMPARE(s.laps.size(), 2);
        QCOMPARE(s.laps.first().number, 2);
        QCOMPARE(s.laps.first().totalMs, qint64(3500));
        QCOMPARE(s.laps.first().splitMs, qint64(1000));
        QCOMPARE(s.laps.last().splitMs, qint64(2500));
        s.reset();
        QVERIFY(s.laps.isEmpty());
        QCOMPARE(s.elapsed(20000), qint64(0));
    }

    void worldClock()
    {
        const QTimeZone berlin("Europe/Berlin");
        const QDateTime utc(QDate(2026, 3, 10), QTime(23, 30), QTimeZone::UTC);
        const ZoneInfo ny = zoneInfo("America/New_York", utc, berlin);
        QCOMPARE(ny.city, QString("New York"));
        QCOMPARE(ny.time, QTime(19, 30)); // EDT since March 8
        QCOMPARE(ny.offsetSeconds, -5 * 3600);
        QCOMPARE(ny.dayDelta, -1);
        QVERIFY(ny.night); // 19:30
        const QDateTime evening(QDate(2026, 3, 10), QTime(20, 0), QTimeZone::UTC);
        const ZoneInfo tokyo = zoneInfo("Asia/Tokyo", evening, berlin); // 05:00 tomorrow
        QCOMPARE(tokyo.dayDelta, 1);
        QVERIFY(tokyo.night);
        QCOMPARE(tokyo.offsetSeconds, 8 * 3600);
    }

    void persistence()
    {
        QList<Alarm> in{ { 3, QTime(6, 45), "Gym", Mon | Fri, false, 5 }, { 4, QTime(23, 0), "", 0, true, 15 } };
        const QList<Alarm> out = alarmsFromJson(alarmsToJson(in));
        QCOMPARE(out.size(), 2);
        QCOMPARE(out[0].id, 3);
        QCOMPARE(out[0].time, QTime(6, 45));
        QCOMPARE(out[0].name, QString("Gym"));
        QCOMPARE(out[0].days, Mon | Fri);
        QVERIFY(!out[0].enabled);
        QCOMPARE(out[1].snoozeMinutes, 15);
        QVERIFY(alarmsFromJson("not json").isEmpty());

        const QStringList zones{ "Asia/Tokyo", "America/New_York" };
        QCOMPARE(zonesFromJson(zonesToJson(zones)), zones);
        QCOMPARE(zonesFromJson(zonesToJson({ "Nowhere/Land", "Asia/Tokyo", "Asia/Tokyo" })), QStringList{ "Asia/Tokyo" });
    }
};

QTEST_GUILESS_MAIN(ClockMathTest)
#include "clockmath.moc"
