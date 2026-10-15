// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// What the Task Manager reads in /proc, taken apart: text in, numbers out,
// so that it can be tested without a running system.

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>

#include <optional>

namespace procfs {

// /proc/<pid>/stat
struct Stat {
    QString name;
    char state = '?';
    int parent = 0;
    quint64 ticks = 0; // user + system time
    int threads = 0;
    quint64 start = 0; // since boot: tells a reused pid from the old one
};
std::optional<Stat> parseStat(const QByteArray& text);

// /proc/stat: the whole machine first, then each processor.
struct Cpu {
    quint64 busy = 0;
    quint64 total = 0;
};
QList<Cpu> parseCpu(const QByteArray& text);

// /proc/meminfo, in bytes: "MemTotal", "MemAvailable", ...
QHash<QByteArray, quint64> parseMeminfo(const QByteArray& text);

// /proc/diskstats, every line (the caller keeps the whole disks).
struct Disk {
    QString name;
    quint64 read = 0; // bytes
    quint64 written = 0;
    quint64 busy = 0; // milliseconds doing I/O
};
QList<Disk> parseDiskstats(const QByteArray& text);

// /proc/net/dev
struct Net {
    QString name;
    quint64 received = 0; // bytes
    quint64 sent = 0;
};
QList<Net> parseNetDev(const QByteArray& text);

} // namespace procfs
