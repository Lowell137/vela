// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "journal.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace journal {

namespace {

// A field is a string, a byte array (not UTF-8 or with control characters),
// an array of values (the field repeated) or null (too large).
QString field(const QJsonObject& object, const char* name)
{
    const QJsonValue value = object.value(QLatin1String(name));
    if (value.isString()) {
        return value.toString();
    }
    if (!value.isArray()) {
        return {};
    }
    const QJsonArray array = value.toArray();
    if (!array.isEmpty() && array.first().isDouble()) {
        QByteArray bytes;
        for (const QJsonValue& byte : array) {
            bytes.append(char(byte.toInt()));
        }
        return QString::fromUtf8(bytes); // bad bytes become U+FFFD
    }
    QStringList parts;
    for (const QJsonValue& part : array) {
        parts << part.toString();
    }
    return parts.join(QLatin1Char('\n'));
}

QString levelName(int priority)
{
    static const char* const names[] = { "emerg", "alert", "crit", "err", "warning", "notice", "info", "debug" };
    return QLatin1String(names[qBound(0, priority, 7)]);
}

} // namespace

QString Entry::source() const
{
    return !unit.isEmpty() ? unit : !identifier.isEmpty() ? identifier : comm;
}

QString Entry::summary() const
{
    const qsizetype end = message.indexOf(QLatin1Char('\n'));
    return end < 0 ? message : message.left(end);
}

QVariantMap Entry::toMap() const
{
    return {
        { QStringLiteral("key"), cursor },
        { QStringLiteral("time"), double(usec / 1000) },
        { QStringLiteral("priority"), priority },
        { QStringLiteral("syslog"), levelName(priority) },
        { QStringLiteral("source"), source() },
        { QStringLiteral("summary"), summary() },
        { QStringLiteral("message"), message },
        { QStringLiteral("unit"), unit },
        { QStringLiteral("identifier"), identifier },
        { QStringLiteral("pid"), pid },
        { QStringLiteral("uid"), uid },
        { QStringLiteral("comm"), comm },
        { QStringLiteral("exe"), exe },
        { QStringLiteral("cmdline"), cmdline },
        { QStringLiteral("hostname"), hostname },
        { QStringLiteral("transport"), transport },
        { QStringLiteral("boot"), bootId },
        { QStringLiteral("cursor"), cursor },
    };
}

QString Entry::toLine() const
{
    return QStringLiteral("%1  %2  %3  %4")
        .arg(QDateTime::fromMSecsSinceEpoch(usec / 1000).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
             levelName(priority), source(), summary());
}

QString Entry::toText() const
{
    QString text;
    const auto add = [&text](const char* name, const QString& value) {
        if (!value.isEmpty()) {
            text += QLatin1String(name) + QLatin1String(": ") + value + QLatin1Char('\n');
        }
    };
    add("Time", QDateTime::fromMSecsSinceEpoch(usec / 1000).toString(Qt::ISODate));
    add("Level", levelName(priority));
    add("Unit", unit);
    add("Identifier", identifier);
    add("PID", pid > 0 ? QString::number(pid) : QString());
    add("UID", uid >= 0 ? QString::number(uid) : QString());
    add("Command", comm);
    add("Executable", exe);
    add("Command line", cmdline);
    add("Host", hostname);
    add("Transport", transport);
    add("Boot ID", bootId);
    add("Cursor", cursor);
    add("Message", message);
    return text;
}

std::optional<Entry> parseLine(const QByteArray& line)
{
    if (line.size() > maxLineBytes) {
        return std::nullopt;
    }
    const QJsonDocument document = QJsonDocument::fromJson(line);
    if (!document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = document.object();
    Entry entry;
    entry.cursor = field(object, "__CURSOR");
    if (entry.cursor.isEmpty()) {
        return std::nullopt;
    }
    entry.usec = field(object, "__REALTIME_TIMESTAMP").toLongLong();
    bool ok = false;
    const int priority = field(object, "PRIORITY").toInt(&ok);
    entry.priority = ok ? qBound(0, priority, 7) : 6;
    entry.facility = field(object, "SYSLOG_FACILITY").toInt(&ok);
    if (!ok) {
        entry.facility = -1;
    }
    entry.pid = field(object, "_PID").toLongLong();
    const QString uid = field(object, "_UID");
    entry.uid = uid.isEmpty() ? -1 : uid.toLongLong();
    entry.message = field(object, "MESSAGE");
    entry.unit = field(object, "_SYSTEMD_UNIT");
    if (entry.unit.isEmpty()) {
        entry.unit = field(object, "_SYSTEMD_USER_UNIT");
    }
    entry.identifier = field(object, "SYSLOG_IDENTIFIER");
    entry.comm = field(object, "_COMM");
    entry.exe = field(object, "_EXE");
    entry.cmdline = field(object, "_CMDLINE");
    entry.hostname = field(object, "_HOSTNAME");
    entry.transport = field(object, "_TRANSPORT");
    entry.bootId = field(object, "_BOOT_ID");
    return entry;
}

QStringList arguments(const Filter& filter, bool follow)
{
    QStringList args = { QStringLiteral("--no-pager"), QStringLiteral("-o"), QStringLiteral("json") };
    if (follow) {
        args << QStringLiteral("-f") << QStringLiteral("-n") << QStringLiteral("0");
    } else {
        args << QStringLiteral("-r");
    }
    if (filter.scope == Scope::System) {
        args << QStringLiteral("--system");
    } else if (filter.scope == Scope::User) {
        args << QStringLiteral("--user");
    }

    int level = qBound(0, filter.level, 7);
    if (filter.view == QLatin1String("warnings")) {
        level = qMin(level, 4);
    } else if (filter.view == QLatin1String("critical")) {
        level = qMin(level, 2);
    }
    if (level < 7) {
        args << QStringLiteral("-p") << QString::number(level);
    }

    if (filter.view == QLatin1String("previous")) {
        args << QStringLiteral("-b") << QStringLiteral("-1");
    } else if (filter.view == QLatin1String("boot") || filter.range == QLatin1String("boot")) {
        args << QStringLiteral("-b") << QStringLiteral("0");
    }
    if (filter.view == QLatin1String("kernel")) {
        args << QStringLiteral("-k");
    }
    static const QHash<QString, QString> since = {
        { QStringLiteral("15m"), QStringLiteral("-15min") }, { QStringLiteral("1h"), QStringLiteral("-1h") },
        { QStringLiteral("1d"), QStringLiteral("-1d") }, { QStringLiteral("1w"), QStringLiteral("-1w") },
    };
    if (since.contains(filter.range)) {
        args << QStringLiteral("--since=") + since.value(filter.range);
    }

    // The unit becomes a plain match so it can sit in every half of an OR.
    const QString unit = filter.unit.trimmed();
    const QString unitMatch = unit.isEmpty() ? QString()
        : QStringLiteral("_SYSTEMD_UNIT=") + (unit.contains(QLatin1Char('.')) ? unit : unit + QStringLiteral(".service"));
    if (filter.view == QLatin1String("apps")) {
        args << QStringLiteral("_TRANSPORT=journal") << QStringLiteral("_TRANSPORT=stdout") << QStringLiteral("_TRANSPORT=syslog");
        if (!unitMatch.isEmpty()) {
            args << unitMatch;
        }
    } else if (filter.view == QLatin1String("security")) {
        const QStringList comms = { QStringLiteral("sudo"), QStringLiteral("su"), QStringLiteral("sshd"), QStringLiteral("login"),
                                    QStringLiteral("polkitd"), QStringLiteral("systemd-logind") };
        for (const QString& comm : comms) {
            args << QStringLiteral("_COMM=") + comm;
        }
        if (!unitMatch.isEmpty()) {
            args << unitMatch;
        }
        args << QStringLiteral("+") << QStringLiteral("SYSLOG_FACILITY=4") << QStringLiteral("SYSLOG_FACILITY=10");
        if (!unitMatch.isEmpty()) {
            args << unitMatch;
        }
    } else if (!unitMatch.isEmpty()) {
        args << unitMatch;
    }
    return args;
}

bool matches(const Entry& entry, const QString& text)
{
    return text.isEmpty() || entry.message.contains(text, Qt::CaseInsensitive)
        || entry.unit.contains(text, Qt::CaseInsensitive) || entry.identifier.contains(text, Qt::CaseInsensitive);
}

} // namespace journal
