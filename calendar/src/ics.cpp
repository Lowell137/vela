// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ics.h"

#include <QMap>
#include <QRegularExpression>
#include <QTimeZone>
#include <QUuid>
#include <algorithm>

namespace cal::ics {

namespace {

const char* const kDays[] = {"MO", "TU", "WE", "TH", "FR", "SA", "SU"};
const int kReminders[] = {0, 5, 15, 30, 60, 1440}; // what the editor offers

struct Line {
    QString name; // upper case
    QMap<QString, QString> params; // upper case keys
    QString value; // raw, still escaped
};

// Folded lines joined: a line starting with a space or tab continues the one before.
QStringList unfold(const QByteArray& text)
{
    QStringList lines;
    for (const QString& raw : QString::fromUtf8(text).split('\n')) {
        QString line = raw;
        if (line.endsWith('\r')) line.chop(1);
        if (!lines.isEmpty() && (line.startsWith(' ') || line.startsWith('\t'))) {
            lines.last() += line.mid(1);
        } else if (!line.isEmpty()) {
            lines.append(line);
        }
    }
    return lines;
}

Line splitLine(const QString& text)
{
    // The first colon outside double quotes ends the name and parameters.
    bool quoted = false;
    int colon = -1;
    for (int i = 0; i < text.size(); ++i) {
        if (text[i] == '"') quoted = !quoted;
        else if (text[i] == ':' && !quoted) {
            colon = i;
            break;
        }
    }
    Line line;
    if (colon < 0) return line;
    line.value = text.mid(colon + 1);
    QStringList head;
    QString part;
    quoted = false;
    for (QChar c : text.left(colon)) {
        if (c == '"') quoted = !quoted;
        if (c == ';' && !quoted) {
            head.append(part);
            part.clear();
        } else {
            part += c;
        }
    }
    head.append(part);
    line.name = head.takeFirst().toUpper();
    for (const QString& p : head) {
        const int eq = p.indexOf('=');
        if (eq > 0) line.params[p.left(eq).toUpper()] = QString(p.mid(eq + 1)).remove('"');
    }
    return line;
}

QString unescape(const QString& s)
{
    QString out;
    for (int i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            const QChar n = s[++i];
            out += (n == 'n' || n == 'N') ? QChar('\n') : n;
        } else {
            out += s[i];
        }
    }
    return out;
}

QString escape(const QString& s)
{
    QString out;
    for (QChar c : s) {
        if (c == '\\' || c == ';' || c == ',') out += '\\';
        if (c == '\n') out += QStringLiteral("\\n");
        else if (c != '\r') out += c;
    }
    return out;
}

// A DATE or DATE-TIME value as local time; `date` tells which it was.
QDateTime parseTime(const Line& line, bool* date)
{
    const QString v = line.value.trimmed();
    *date = line.params.value("VALUE").toUpper() == "DATE" || v.size() == 8;
    if (*date) {
        const QDate d = QDate::fromString(v.left(8), "yyyyMMdd");
        return d.isValid() ? QDateTime(d, QTime(0, 0)) : QDateTime();
    }
    const bool utc = v.endsWith('Z', Qt::CaseInsensitive);
    QDateTime t = QDateTime::fromString(utc ? v.chopped(1) : v, "yyyyMMdd'T'HHmmss");
    if (!t.isValid()) return QDateTime();
    if (utc) {
        t.setTimeZone(QTimeZone::utc());
        return t.toLocalTime();
    }
    const QString tzid = line.params.value("TZID");
    if (!tzid.isEmpty()) {
        const QTimeZone zone(tzid.toUtf8());
        if (zone.isValid()) {
            t.setTimeZone(zone);
            return t.toLocalTime();
        }
    }
    return QDateTime(t.date(), t.time()); // floating
}

// "-PT15M", "P1D", "PT1H30M", "P1W": minutes, magnitude only.
qint64 durationMinutes(const QString& text, bool* negative = nullptr)
{
    static const QRegularExpression re(QStringLiteral("^([+-])?P(?:(\\d+)W)?(?:(\\d+)D)?(?:T(?:(\\d+)H)?(?:(\\d+)M)?(?:(\\d+)S)?)?$"));
    const QRegularExpressionMatch m = re.match(text.trimmed().toUpper());
    if (!m.hasMatch()) return -1;
    if (negative) *negative = m.captured(1) == "-";
    return m.captured(2).toLongLong() * 10080 + m.captured(3).toLongLong() * 1440
        + m.captured(4).toLongLong() * 60 + m.captured(5).toLongLong();
}

int nearestReminder(qint64 minutes)
{
    int best = kReminders[0];
    for (int r : kReminders) {
        if (qAbs(r - minutes) < qAbs(best - minutes)) best = r;
    }
    return best;
}

void applyRule(const QString& value, Event& e)
{
    Rule& rule = e.rule;
    for (const QString& part : value.split(';')) {
        const int eq = part.indexOf('=');
        if (eq < 0) continue;
        const QString key = part.left(eq).toUpper();
        const QString v = part.mid(eq + 1);
        if (key == "FREQ") {
            const QStringList names = {"", "DAILY", "WEEKLY", "MONTHLY", "YEARLY"};
            rule.freq = std::max<int>(0, names.indexOf(v.toUpper()));
        } else if (key == "INTERVAL") {
            rule.interval = std::max(1, v.toInt());
        } else if (key == "COUNT") {
            rule.count = std::max(0, v.toInt());
        } else if (key == "UNTIL") {
            rule.until = QDate::fromString(v.left(8), "yyyyMMdd");
            if (v.endsWith('Z', Qt::CaseInsensitive) && v.size() > 8) { // UTC: its local day
                Line l;
                l.value = v;
                bool date;
                const QDateTime t = parseTime(l, &date);
                if (t.isValid()) rule.until = t.date();
            }
        } else if (key == "BYDAY") {
            for (const QString& d : v.split(',')) {
                // shortcut: "2MO" style ordinals are read as plain weekdays, add BYSETPOS-like monthly rules when asked
                const int i = QStringList(std::begin(kDays), std::end(kDays)).indexOf(d.right(2).toUpper());
                if (i >= 0) rule.weekdays |= 1 << i;
            }
        }
    }
}

QString dateText(QDate d) { return d.toString("yyyyMMdd"); }
QString timeText(const QDateTime& t) { return t.toString("yyyyMMdd'T'HHmmss"); }

void push(QByteArray& out, const QString& line)
{
    // Folded at 75 octets, never inside a UTF-8 sequence.
    const QByteArray bytes = line.toUtf8();
    int pos = 0;
    int limit = 75;
    while (bytes.size() - pos > limit) {
        int cut = pos + limit;
        while (cut > pos && (bytes[cut] & 0xC0) == 0x80) --cut;
        out += bytes.mid(pos, cut - pos) + "\r\n ";
        pos = cut;
        limit = 74;
    }
    out += bytes.mid(pos) + "\r\n";
}

Event fromComponent(const QList<Line>& lines)
{
    Event e;
    bool startIsDate = false;
    QDateTime end;
    bool endSet = false;
    qint64 duration = -1;
    QList<QDate> exceptions;
    int depth = 0;
    for (const Line& l : lines) {
        if (l.name == "BEGIN") {
            ++depth; // VALARM: its TRIGGER is read below
            continue;
        }
        if (l.name == "END") {
            --depth;
            continue;
        }
        if (depth > 0) {
            if (l.name == "TRIGGER" && l.params.value("VALUE").toUpper() != "DATE-TIME" && e.reminder < 0) {
                bool before = false;
                const qint64 minutes = durationMinutes(l.value, &before);
                if (minutes >= 0) e.reminder = nearestReminder(before ? minutes : 0);
            }
            continue;
        }
        if (l.name == "UID") e.id = unescape(l.value);
        else if (l.name == "SUMMARY") e.title = unescape(l.value);
        else if (l.name == "LOCATION") e.location = unescape(l.value);
        else if (l.name == "DESCRIPTION") e.notes = unescape(l.value);
        else if (l.name == "DTSTART") e.start = parseTime(l, &startIsDate);
        else if (l.name == "DTEND") {
            bool date;
            end = parseTime(l, &date);
            endSet = end.isValid();
        } else if (l.name == "DURATION") duration = durationMinutes(l.value);
        else if (l.name == "RRULE") applyRule(l.value, e);
        else if (l.name == "EXDATE") {
            for (const QString& v : l.value.split(',')) {
                Line one = l;
                one.value = v;
                bool date;
                const QDateTime t = parseTime(one, &date);
                if (t.isValid()) exceptions.append(t.date());
            }
        }
    }
    if (!e.start.isValid()) return Event();
    e.exceptions = exceptions;
    e.allDay = startIsDate;
    if (endSet) {
        e.end = end;
        if (e.allDay) e.end = e.end.addDays(-1); // DTEND is exclusive for dates
    } else if (duration >= 0) {
        e.end = e.allDay ? e.start.addDays(std::max<qint64>(0, (duration + 1439) / 1440 - 1)) : e.start.addSecs(duration * 60);
    } else {
        e.end = e.start;
    }
    if (e.end < e.start) e.end = e.start;
    if (e.rule.freq == Rule::None) e.rule = Rule();
    if (e.id.isEmpty()) e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return e;
}

} // namespace

QList<Event> parse(const QByteArray& text)
{
    QList<Event> events;
    QList<Line> component;
    bool inEvent = false;
    for (const QString& raw : unfold(text)) {
        const Line line = splitLine(raw);
        if (line.name == "BEGIN" && line.value.toUpper() == "VEVENT" && !inEvent) {
            inEvent = true;
            component.clear();
        } else if (line.name == "END" && line.value.toUpper() == "VEVENT" && inEvent) {
            inEvent = false;
            const Event e = fromComponent(component);
            if (e.start.isValid()) events.append(e);
        } else if (inEvent) {
            component.append(line);
        }
    }
    return events;
}

QByteArray write(const QList<Event>& events)
{
    QByteArray out;
    push(out, "BEGIN:VCALENDAR");
    push(out, "VERSION:2.0");
    push(out, "PRODID:-//Vela//Calendar//EN");
    const QString stamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd'T'HHmmss'Z'");
    for (const Event& e : events) {
        push(out, "BEGIN:VEVENT");
        push(out, "UID:" + escape(e.id));
        push(out, "DTSTAMP:" + stamp);
        push(out, "SUMMARY:" + escape(e.title));
        // Timed events are floating (local) times, so a repeating one keeps its hour.
        if (e.allDay) {
            push(out, "DTSTART;VALUE=DATE:" + dateText(e.start.date()));
            push(out, "DTEND;VALUE=DATE:" + dateText(e.end.date().addDays(1)));
        } else {
            push(out, "DTSTART:" + timeText(e.start));
            push(out, "DTEND:" + timeText(e.end));
        }
        if (e.rule.freq != Rule::None) {
            const QStringList names = {"", "DAILY", "WEEKLY", "MONTHLY", "YEARLY"};
            QString rule = "RRULE:FREQ=" + names[e.rule.freq];
            if (e.rule.interval > 1) rule += ";INTERVAL=" + QString::number(e.rule.interval);
            if (e.rule.freq == Rule::Weekly && e.rule.weekdays) {
                QStringList days;
                for (int i = 0; i < 7; ++i) {
                    if (e.rule.weekdays & (1 << i)) days << kDays[i];
                }
                rule += ";BYDAY=" + days.join(',');
            }
            if (e.rule.count > 0) rule += ";COUNT=" + QString::number(e.rule.count);
            else if (e.rule.until.isValid()) {
                rule += ";UNTIL=" + (e.allDay ? dateText(e.rule.until)
                                              : timeText(QDateTime(e.rule.until, QTime(23, 59, 59))));
            }
            push(out, rule);
        }
        for (const QDate& day : e.exceptions) {
            push(out, e.allDay ? "EXDATE;VALUE=DATE:" + dateText(day) : "EXDATE:" + timeText(QDateTime(day, e.start.time())));
        }
        if (!e.location.isEmpty()) push(out, "LOCATION:" + escape(e.location));
        if (!e.notes.isEmpty()) push(out, "DESCRIPTION:" + escape(e.notes));
        if (e.reminder >= 0) {
            push(out, "BEGIN:VALARM");
            push(out, "ACTION:DISPLAY");
            push(out, "DESCRIPTION:" + escape(e.title));
            push(out, e.reminder == 0 ? QStringLiteral("TRIGGER:PT0S") : "TRIGGER:-PT" + QString::number(e.reminder) + "M");
            push(out, "END:VALARM");
        }
        push(out, "END:VEVENT");
    }
    push(out, "END:VCALENDAR");
    return out;
}

} // namespace cal::ics
