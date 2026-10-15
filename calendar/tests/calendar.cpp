// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Repeats expanded into occurrences, .ics read and written, the store's file.
// Runs in TZ=Europe/Rome (CMakeLists.txt): daylight saving ends 2026-10-25.

#include "event.h"
#include "ics.h"
#include "store.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace cal;

namespace {

Event timed(QDate day, QTime time, int minutes = 60)
{
    Event e;
    e.id = "e";
    e.title = "t";
    e.start = QDateTime(day, time);
    e.end = e.start.addSecs(minutes * 60);
    return e;
}

QList<QDate> days(const QList<Occurrence>& list)
{
    QList<QDate> out;
    for (const Occurrence& o : list) out.append(o.start.date());
    return out;
}

} // namespace

class CalendarTest : public QObject {
    Q_OBJECT

private slots:
    void zoneHasDaylightSaving()
    {
        QVERIFY2(QDateTime(QDate(2026, 7, 1), QTime(12, 0)).offsetFromUtc() == 7200, "TZ not Europe/Rome");
    }

    void singleEvent()
    {
        const Event e = timed(QDate(2026, 5, 10), QTime(9, 0));
        QCOMPARE(expand(e, QDate(2026, 5, 1), QDate(2026, 5, 31)).size(), 1);
        QCOMPARE(expand(e, QDate(2026, 5, 11), QDate(2026, 5, 31)).size(), 0);
        QCOMPARE(expand(e, QDate(2026, 5, 10), QDate(2026, 5, 10)).size(), 1);
    }

    void dailyWithIntervalAndCount()
    {
        Event e = timed(QDate(2026, 1, 1), QTime(8, 0));
        e.rule.freq = Rule::Daily;
        e.rule.interval = 3;
        e.rule.count = 4;
        const QList<QDate> d = days(expand(e, QDate(2026, 1, 1), QDate(2026, 12, 31)));
        QCOMPARE(d, (QList<QDate>{QDate(2026, 1, 1), QDate(2026, 1, 4), QDate(2026, 1, 7), QDate(2026, 1, 10)}));
    }

    void countIgnoresTheWindow()
    {
        // The window starts late: occurrences before it still use up the count.
        Event e = timed(QDate(2026, 1, 1), QTime(8, 0));
        e.rule.freq = Rule::Daily;
        e.rule.count = 5;
        QCOMPARE(days(expand(e, QDate(2026, 1, 4), QDate(2026, 2, 1))).size(), 2);
    }

    void untilIsInclusive()
    {
        Event e = timed(QDate(2026, 3, 1), QTime(8, 0));
        e.rule.freq = Rule::Daily;
        e.rule.until = QDate(2026, 3, 3);
        QCOMPARE(days(expand(e, QDate(2026, 1, 1), QDate(2026, 12, 31))).size(), 3);
    }

    void weeklyByDay()
    {
        // Wednesday start, Monday and Friday set: the Monday before the start is not an occurrence.
        Event e = timed(QDate(2026, 6, 3), QTime(18, 0));
        e.rule.freq = Rule::Weekly;
        e.rule.weekdays = (1 << 0) | (1 << 4);
        const QList<QDate> d = days(expand(e, QDate(2026, 6, 1), QDate(2026, 6, 14)));
        QCOMPARE(d, (QList<QDate>{QDate(2026, 6, 5), QDate(2026, 6, 8), QDate(2026, 6, 12)}));
    }

    void weeklyEveryOtherWeekAndNoDaySet()
    {
        Event e = timed(QDate(2026, 6, 3), QTime(18, 0)); // a Wednesday
        e.rule.freq = Rule::Weekly;
        e.rule.interval = 2;
        const QList<QDate> d = days(expand(e, QDate(2026, 6, 1), QDate(2026, 7, 1)));
        QCOMPARE(d, (QList<QDate>{QDate(2026, 6, 3), QDate(2026, 6, 17), QDate(2026, 7, 1)}));
    }

    void weeklySundayBelongsToTheWeekOfItsMonday()
    {
        Event e = timed(QDate(2026, 6, 7), QTime(10, 0)); // Sunday
        e.rule.freq = Rule::Weekly;
        e.rule.weekdays = (1 << 0) | (1 << 6);
        e.rule.interval = 2;
        const QList<QDate> d = days(expand(e, QDate(2026, 6, 1), QDate(2026, 6, 30)));
        // week of Jun 1: Sunday the 7th only (the Monday is before the start);
        // two weeks on: Monday the 15th and Sunday the 21st.
        QCOMPARE(d, (QList<QDate>{QDate(2026, 6, 7), QDate(2026, 6, 15), QDate(2026, 6, 21), QDate(2026, 6, 29)}));
    }

    void monthlyOnThe31stSkipsShortMonths()
    {
        Event e = timed(QDate(2026, 1, 31), QTime(9, 0));
        e.rule.freq = Rule::Monthly;
        const QList<QDate> d = days(expand(e, QDate(2026, 1, 1), QDate(2026, 12, 31)));
        QCOMPARE(d, (QList<QDate>{QDate(2026, 1, 31), QDate(2026, 3, 31), QDate(2026, 5, 31), QDate(2026, 7, 31),
                                  QDate(2026, 8, 31), QDate(2026, 10, 31), QDate(2026, 12, 31)}));
    }

    void monthlyCountSkipsAreNotCounted()
    {
        Event e = timed(QDate(2026, 1, 31), QTime(9, 0));
        e.rule.freq = Rule::Monthly;
        e.rule.count = 3;
        QCOMPARE(days(expand(e, QDate(2026, 1, 1), QDate(2027, 12, 31))),
                 (QList<QDate>{QDate(2026, 1, 31), QDate(2026, 3, 31), QDate(2026, 5, 31)}));
    }

    void monthlyEveryThirdMonth()
    {
        Event e = timed(QDate(2026, 11, 15), QTime(9, 0));
        e.rule.freq = Rule::Monthly;
        e.rule.interval = 3;
        QCOMPARE(days(expand(e, QDate(2026, 11, 1), QDate(2027, 6, 30))),
                 (QList<QDate>{QDate(2026, 11, 15), QDate(2027, 2, 15), QDate(2027, 5, 15)}));
    }

    void yearlyLeapDay()
    {
        Event e = timed(QDate(2024, 2, 29), QTime(9, 0));
        e.rule.freq = Rule::Yearly;
        QCOMPARE(days(expand(e, QDate(2024, 1, 1), QDate(2033, 1, 1))),
                 (QList<QDate>{QDate(2024, 2, 29), QDate(2028, 2, 29), QDate(2032, 2, 29)}));
    }

    void yearlyOrdinary()
    {
        Event e = timed(QDate(2020, 6, 1), QTime(9, 0));
        e.rule.freq = Rule::Yearly;
        e.rule.interval = 2;
        QCOMPARE(days(expand(e, QDate(2026, 1, 1), QDate(2029, 12, 31))),
                 (QList<QDate>{QDate(2026, 6, 1), QDate(2028, 6, 1)}));
    }

    void daylightSavingKeepsTheWallClock()
    {
        // 2026-10-25 is 25 hours long in Rome: the 09:00 stays 09:00.
        Event e = timed(QDate(2026, 10, 24), QTime(9, 0));
        e.rule.freq = Rule::Daily;
        const QList<Occurrence> o = expand(e, QDate(2026, 10, 24), QDate(2026, 10, 27));
        QCOMPARE(o.size(), 4);
        for (const Occurrence& x : o) QCOMPARE(x.start.time(), QTime(9, 0));
        QCOMPARE(o[1].start.offsetFromUtc(), 3600); // the 25th, after the change
        QCOMPARE(o[0].start.offsetFromUtc(), 7200);
        QCOMPARE(o[1].end.time(), QTime(10, 0));
    }

    void springForwardGapEvent()
    {
        // 02:30 doesn't exist on 2026-03-29: the event still shows up that day.
        Event e = timed(QDate(2026, 3, 28), QTime(2, 30));
        e.rule.freq = Rule::Daily;
        const QList<Occurrence> o = expand(e, QDate(2026, 3, 28), QDate(2026, 3, 30));
        QCOMPARE(o.size(), 3);
        QCOMPARE(o[1].start.date(), QDate(2026, 3, 29));
    }

    void allDayMultiDaySpan()
    {
        Event e;
        e.id = "trip";
        e.allDay = true;
        e.start = QDateTime(QDate(2026, 5, 29), QTime(0, 0));
        e.end = QDateTime(QDate(2026, 6, 2), QTime(0, 0)); // last day, inclusive
        // Seen from June only: it began in May.
        const QList<Occurrence> o = expand(e, QDate(2026, 6, 1), QDate(2026, 6, 30));
        QCOMPARE(o.size(), 1);
        QCOMPARE(o[0].start.date(), QDate(2026, 5, 29));
        QCOMPARE(o[0].end.date(), QDate(2026, 6, 2));
        QCOMPARE(expand(e, QDate(2026, 6, 3), QDate(2026, 6, 30)).size(), 0);
    }

    void repeatingMultiDayCatchesTheTail()
    {
        Event e = timed(QDate(2026, 6, 1), QTime(22, 0), 60 * 30); // 30 h: ends two days later, 04:00
        e.rule.freq = Rule::Weekly;
        const QList<Occurrence> o = expand(e, QDate(2026, 6, 9), QDate(2026, 6, 9));
        QCOMPARE(o.size(), 1);
        QCOMPARE(o[0].start.date(), QDate(2026, 6, 8));
    }

    void exceptionRemovesOne()
    {
        Event e = timed(QDate(2026, 6, 1), QTime(9, 0));
        e.rule.freq = Rule::Daily;
        e.rule.count = 3;
        e.exceptions = {QDate(2026, 6, 2)};
        QCOMPARE(days(expand(e, QDate(2026, 6, 1), QDate(2026, 6, 30))), (QList<QDate>{QDate(2026, 6, 1), QDate(2026, 6, 3)}));
    }

    void remindersFireOnceInTheirWindow()
    {
        Event e = timed(QDate(2026, 6, 2), QTime(9, 0));
        e.reminder = 15;
        e.rule.freq = Rule::Daily;
        const QDateTime a(QDate(2026, 6, 2), QTime(8, 30));
        const QDateTime b(QDate(2026, 6, 2), QTime(8, 45));
        QCOMPARE(remindersBetween({e}, a, b).size(), 1);
        QCOMPARE(remindersBetween({e}, b, b.addSecs(60)).size(), 0); // (after, upTo]
        QCOMPARE(nextReminder({e}, b, 31), QDateTime(QDate(2026, 6, 3), QTime(8, 45)));
        e.rule = Rule();
        QVERIFY(!nextReminder({e}, b, 31).isValid());
        e.reminder = 1440; // a day before: due yesterday 09:00, across midnight
        QCOMPARE(remindersBetween({e}, QDateTime(QDate(2026, 6, 1), QTime(8, 0)),
                                  QDateTime(QDate(2026, 6, 1), QTime(10, 0))).size(), 1);
    }

    void icsReadsTheCommonForms()
    {
        const QByteArray text =
            "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VTIMEZONE\r\nTZID:X\r\nEND:VTIMEZONE\r\n"
            "BEGIN:VEVENT\r\nUID:a@x\r\nSUMMARY:Lunch\\, with\\nSam\r\n"
            "DTSTART:20260610T100000Z\r\nDTEND:20260610T110000Z\r\n"
            "DESCRIPTION:Long \r\n notes folded\r\nLOCATION:Caf\xc3\xa9\r\n"
            "RRULE:FREQ=WEEKLY;INTERVAL=2;BYDAY=MO,WE;COUNT=10\r\n"
            "BEGIN:VALARM\r\nTRIGGER:-PT15M\r\nACTION:DISPLAY\r\nEND:VALARM\r\nEND:VEVENT\r\n"
            "BEGIN:VEVENT\r\nUID:b@x\r\nSUMMARY:Holiday\r\nDTSTART;VALUE=DATE:20260801\r\nDTEND;VALUE=DATE:20260804\r\n"
            "RRULE:FREQ=YEARLY;UNTIL=20300801\r\nEND:VEVENT\r\n"
            "BEGIN:VEVENT\r\nUID:c@x\r\nSUMMARY:Rome\r\nDTSTART;TZID=Europe/Rome:20260610T180000\r\n"
            "DURATION:PT90M\r\nEND:VEVENT\r\n"
            "BEGIN:VEVENT\r\nUID:d@x\r\nSUMMARY:Floating\r\nDTSTART:20260610T180000\r\nEND:VEVENT\r\n"
            "BEGIN:VTODO\r\nSUMMARY:no\r\nEND:VTODO\r\nEND:VCALENDAR\r\n";
        const QList<Event> events = ics::parse(text);
        QCOMPARE(events.size(), 4);
        const Event& a = events[0];
        QCOMPARE(a.title, QString("Lunch, with\nSam"));
        QCOMPARE(a.notes, QString("Long notes folded"));
        QCOMPARE(a.location, QString::fromUtf8("Caf\xc3\xa9"));
        QCOMPARE(a.start, QDateTime(QDate(2026, 6, 10), QTime(12, 0))); // 10:00 UTC, +2 in June
        QCOMPARE(a.end, a.start.addSecs(3600));
        QCOMPARE(a.rule.freq, int(Rule::Weekly));
        QCOMPARE(a.rule.interval, 2);
        QCOMPARE(a.rule.weekdays, (1 << 0) | (1 << 2));
        QCOMPARE(a.rule.count, 10);
        QCOMPARE(a.reminder, 15);
        const Event& b = events[1];
        QVERIFY(b.allDay);
        QCOMPARE(b.start.date(), QDate(2026, 8, 1));
        QCOMPARE(b.end.date(), QDate(2026, 8, 3)); // DTEND is exclusive
        QCOMPARE(b.rule.until, QDate(2030, 8, 1));
        QCOMPARE(events[2].start, QDateTime(QDate(2026, 6, 10), QTime(18, 0)));
        QCOMPARE(events[2].end, events[2].start.addSecs(5400));
        QCOMPARE(events[3].start.time(), QTime(18, 0));
    }

    void icsTzidIsConverted()
    {
        const QList<Event> e = ics::parse("BEGIN:VEVENT\r\nUID:z\r\nDTSTART;TZID=America/New_York:20260610T120000\r\nEND:VEVENT\r\n");
        QCOMPARE(e.size(), 1);
        QCOMPARE(e[0].start, QDateTime(QDate(2026, 6, 10), QTime(18, 0))); // 12:00 EDT is 18:00 CEST
    }

    void icsRoundTrip()
    {
        Event a = timed(QDate(2026, 6, 10), QTime(9, 30), 45);
        a.id = "one";
        a.title = "A; B, C\\D\nline two";
        a.location = "Room \xc3\xa9";
        a.notes = QString(300, QChar(0x00e9)) + "tail"; // folds, in the middle of 2-byte letters
        a.reminder = 60;
        a.rule.freq = Rule::Weekly;
        a.rule.weekdays = (1 << 1) | (1 << 3);
        a.rule.interval = 2;
        a.rule.until = QDate(2026, 12, 1);
        a.exceptions = {QDate(2026, 6, 16)};
        Event b;
        b.id = "two";
        b.title = "Trip";
        b.allDay = true;
        b.start = QDateTime(QDate(2026, 7, 1), QTime(0, 0));
        b.end = QDateTime(QDate(2026, 7, 4), QTime(0, 0));
        b.rule.freq = Rule::Monthly;
        b.rule.count = 6;
        const QByteArray text = ics::write({a, b});
        for (const QByteArray& line : text.split('\n')) QVERIFY(line.size() <= 76); // 75 + CR
        QVERIFY(text.contains("DTEND;VALUE=DATE:20260705")); // exclusive
        const QList<Event> back = ics::parse(text);
        QCOMPARE(back.size(), 2);
        for (int i = 0; i < 2; ++i) {
            const Event& x = i == 0 ? a : b;
            QCOMPARE(back[i].id, x.id);
            QCOMPARE(back[i].title, x.title);
            QCOMPARE(back[i].location, x.location);
            QCOMPARE(back[i].notes, x.notes);
            QCOMPARE(back[i].allDay, x.allDay);
            QCOMPARE(back[i].start, x.start);
            QCOMPARE(back[i].end, x.end);
            QVERIFY(back[i].rule == x.rule);
            QCOMPARE(back[i].reminder, x.reminder);
            QCOMPARE(back[i].exceptions, x.exceptions);
        }
    }

    void jsonRoundTrip()
    {
        Event a = timed(QDate(2026, 6, 10), QTime(9, 30), 45);
        a.rule.freq = Rule::Yearly;
        a.rule.until = QDate(2030, 1, 1);
        a.exceptions = {QDate(2027, 6, 10)};
        a.reminder = 5;
        a.color = 3;
        const Event b = Event::fromJson(a.toJson());
        QCOMPARE(b.start, a.start);
        QCOMPARE(b.end, a.end);
        QVERIFY(b.rule == a.rule);
        QCOMPARE(b.exceptions, a.exceptions);
        QCOMPARE(b.color, 3);
    }

    void storeSavesAtomicallyAndReloads()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("sub/events.json");
        QString id;
        {
            Store store(path);
            QCOMPARE(store.revision(), 0);
            const qint64 ms = QDateTime(QDate(2026, 6, 10), QTime(9, 0)).toMSecsSinceEpoch();
            id = store.save({{"title", "Dentist"}, {"startMs", ms}, {"endMs", ms + 3600000}, {"freq", 1}, {"count", 3}});
            QVERIFY(!id.isEmpty());
            QCOMPARE(store.revision(), 1);
            store.removeOccurrence(id, "2026-06-11");
            QCOMPARE(store.occurrences(2026, 6, 1, 2026, 6, 30).size(), 2);
        }
        QVERIFY(!QFile::exists(path + ".tmp"));
        Store again(path);
        QCOMPARE(again.event(id).value("title").toString(), QString("Dentist"));
        QCOMPARE(again.occurrences(2026, 6, 1, 2026, 6, 30).size(), 2);
        again.remove(id);
        QCOMPARE(again.occurrences(2026, 6, 1, 2026, 6, 30).size(), 0);
    }

    void storeImportsTwiceWithoutDoubling()
    {
        QTemporaryDir dir;
        QFile f(dir.filePath("x.ics"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("BEGIN:VEVENT\r\nUID:q\r\nSUMMARY:S\r\nDTSTART:20260610T100000\r\nEND:VEVENT\r\n");
        f.close();
        Store store(dir.filePath("e.json"));
        QCOMPARE(store.icsCount(f.fileName()), 1);
        QCOMPARE(store.importIcs(f.fileName()), 1);
        QCOMPARE(store.importIcs(f.fileName()), 1);
        QCOMPARE(store.events().size(), 1);
        QCOMPARE(store.importIcs(dir.filePath("missing.ics")), -1);
        QVERIFY(store.exportIcs(dir.filePath("out.ics")));
    }
};

QTEST_MAIN(CalendarTest)
#include "calendar.moc"
