// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "procfs.h"

namespace procfs {

std::optional<Stat> parseStat(const QByteArray& text)
{
    // "pid (name) state ppid ...": the name may hold spaces and parentheses,
    // so it ends at the last one.
    const qsizetype open = text.indexOf('(');
    const qsizetype close = text.lastIndexOf(')');
    if (open < 0 || close < open) {
        return std::nullopt;
    }
    const QList<QByteArray> fields = text.mid(close + 2).simplified().split(' ');
    if (fields.size() < 20) {
        return std::nullopt;
    }
    Stat stat;
    stat.name = QString::fromUtf8(text.mid(open + 1, close - open - 1));
    stat.state = fields[0].isEmpty() ? '?' : fields[0][0];
    stat.parent = fields[1].toInt();
    stat.ticks = fields[11].toULongLong() + fields[12].toULongLong();
    stat.threads = fields[17].toInt();
    stat.start = fields[19].toULongLong();
    return stat;
}

QList<Cpu> parseCpu(const QByteArray& text)
{
    QList<Cpu> list;
    for (const QByteArray& line : text.split('\n')) {
        if (!line.startsWith("cpu")) {
            continue;
        }
        // user nice system idle iowait irq softirq steal
        const QList<QByteArray> fields = line.simplified().split(' ');
        Cpu cpu;
        for (qsizetype i = 1; i < fields.size() && i <= 8; i++) {
            const quint64 value = fields[i].toULongLong();
            cpu.total += value;
            if (i != 4 && i != 5) { // not idle, not waiting for I/O
                cpu.busy += value;
            }
        }
        list.append(cpu);
    }
    return list;
}

QHash<QByteArray, quint64> parseMeminfo(const QByteArray& text)
{
    QHash<QByteArray, quint64> values;
    for (const QByteArray& line : text.split('\n')) {
        const qsizetype colon = line.indexOf(':');
        if (colon > 0) { // "MemTotal:       32768000 kB"
            values.insert(line.left(colon), line.mid(colon + 1).simplified().split(' ').first().toULongLong() * 1024);
        }
    }
    return values;
}

QList<Disk> parseDiskstats(const QByteArray& text)
{
    QList<Disk> list;
    for (const QByteArray& line : text.split('\n')) {
        // major minor name reads merged sectors ms writes merged sectors ms inflight busy-ms ...
        const QList<QByteArray> fields = line.simplified().split(' ');
        if (fields.size() < 13) {
            continue;
        }
        list.append({ QString::fromLatin1(fields[2]), fields[5].toULongLong() * 512, fields[9].toULongLong() * 512,
            fields[12].toULongLong() });
    }
    return list;
}

QList<Net> parseNetDev(const QByteArray& text)
{
    QList<Net> list;
    for (const QByteArray& line : text.split('\n')) {
        const qsizetype colon = line.indexOf(':');
        if (colon < 0) { // the two header lines
            continue;
        }
        const QList<QByteArray> fields = line.mid(colon + 1).simplified().split(' ');
        if (fields.size() >= 9) {
            list.append({ QString::fromLatin1(line.left(colon).trimmed()), fields[0].toULongLong(), fields[8].toULongLong() });
        }
    }
    return list;
}

} // namespace procfs
