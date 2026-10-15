// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// What `journalctl -o json` says, taken apart: one line is one entry, and a
// Filter turns into the arguments that make journalctl do the filtering.

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>

namespace journal {

struct Entry {
    QString cursor; // the key: unique in the journal
    qint64 usec = 0; // __REALTIME_TIMESTAMP, microseconds since the epoch
    int priority = 6; // syslog: 0 emergency .. 7 debug
    int facility = -1;
    qint64 pid = 0;
    qint64 uid = -1;
    QString message;
    QString unit; // _SYSTEMD_UNIT, else _SYSTEMD_USER_UNIT
    QString identifier; // SYSLOG_IDENTIFIER
    QString comm;
    QString exe;
    QString cmdline;
    QString hostname;
    QString transport;
    QString bootId;

    // Who wrote it: the unit, else the program's identifier.
    QString source() const;
    // The first line, what the table shows.
    QString summary() const;
    QVariantMap toMap() const;
    // The whole entry as `Field: value` lines, for the clipboard.
    QString toText() const;
    // One line for a saved file.
    QString toLine() const;
};

// Lines longer than this are skipped: journalctl prints binary blobs as
// number arrays and a few MB of those only costs memory.
constexpr int maxLineBytes = 1 << 20;

// nullopt: not JSON, no cursor, or too long.
std::optional<Entry> parseLine(const QByteArray& line);

enum class Scope { All, System, User };

struct Filter {
    // all, warnings, critical, boot, previous, kernel, apps, security
    QString view = QStringLiteral("all");
    int level = 7; // shown from this priority up (0..7), 7 is everything
    QString range = QStringLiteral("all"); // 15m, 1h, 1d, 1w, boot, all
    QString unit;
    QString text; // only matches() uses it, journalctl can't search more than MESSAGE
    Scope scope = Scope::All;
};

// The arguments for journalctl; `follow` is the live part (`-f -n 0`, new
// entries only), otherwise newest first.
//
// The views are made of journalctl's own matches:
//   warnings  -p 4         critical  -p 2        boot  -b 0 / -b -1
//   kernel    -k           apps      _TRANSPORT of the programs (journal,
//   stdout, syslog), which leaves out the kernel and the audit log
//   security  sudo, su, sshd, login, polkit, logind by _COMM, or the facility
//   auth (4) and authpriv (10); `+` is journalctl's OR, so the unit is put
//   in both halves
// A match field repeated is an OR, different fields are an AND.
QStringList arguments(const Filter& filter, bool follow);

// The search box: case-insensitive over message, unit and identifier.
bool matches(const Entry& entry, const QString& text);

} // namespace journal
