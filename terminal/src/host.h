// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QObject>
#include <QProcess>
#include <QVariantList>

// What the window asks the machine: the shells there are, the fonts a
// terminal can use, and what the command line asked for.
class Host : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString command MEMBER m_command CONSTANT) // after -e: instead of the shell
    Q_PROPERTY(QString directory MEMBER m_directory CONSTANT)
    Q_PROPERTY(QString defaultFont READ defaultFont CONSTANT)

public:
    Host(const QString& command, const QString& directory)
        : m_command(command)
        , m_directory(directory)
    {
    }

    QString defaultFont() const { return QFontDatabase::systemFont(QFontDatabase::FixedFont).family(); }

    // The shells of /etc/shells that are installed, the user's first:
    // [{ name, command, directory }].
    Q_INVOKABLE QVariantList shells() const
    {
        QStringList paths = { qEnvironmentVariable("SHELL", QStringLiteral("/bin/sh")) };
        QFile file(QStringLiteral("/etc/shells"));
        if (file.open(QIODevice::ReadOnly)) {
            const QList<QByteArray> lines = file.readAll().split('\n');
            for (const QByteArray& line : lines) {
                paths.append(QString::fromUtf8(line.trimmed()));
            }
        }
        QVariantList out;
        // Not for typing in: the ones that refuse, restrict or only stand in.
        QStringList seen = { QStringLiteral("nologin"), QStringLiteral("false"), QStringLiteral("git-shell"), QStringLiteral("rbash"),
            QStringLiteral("systemd-home-fallback-shell") };
        for (const QString& path : std::as_const(paths)) {
            const QFileInfo info(path);
            if (!path.startsWith(QLatin1Char('/')) || !info.isExecutable() || seen.contains(info.fileName())) {
                continue;
            }
            seen.append(info.fileName());
            out.append(QVariantMap { { QStringLiteral("name"), info.fileName() }, { QStringLiteral("command"), path }, { QStringLiteral("directory"), QString() } });
        }
        return out;
    }

    Q_INVOKABLE QStringList fonts() const
    {
        QStringList out;
        const QStringList families = QFontDatabase::families();
        for (const QString& family : families) {
            if (QFontDatabase::isFixedPitch(family) && !QFontDatabase::isPrivateFamily(family)) {
                out.append(family);
            }
        }
        return out;
    }

    Q_INVOKABLE void newWindow() const { QProcess::startDetached(QCoreApplication::applicationFilePath(), {}); }

private:
    QString m_command;
    QString m_directory;
};
