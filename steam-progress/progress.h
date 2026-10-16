// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// Steam's download progress, from its appmanifest files and the files it has
// staged. Shared by the steam-progress program and its test.
#pragma once

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace steamprogress {
constexpr int Updating = 1024; // StateFlags bit: "update running"

inline QString defaultDir()
{
    const QString env = qEnvironmentVariable("STEAM_DIR");
    return env.isEmpty() ? QDir::homePath() + QStringLiteral("/.local/share/Steam") : env;
}

inline QStringList libraries(const QString& steam)
{
    QSet<QString> paths { steam };
    QFile f(steam + QStringLiteral("/steamapps/libraryfolders.vdf"));
    if (f.open(QIODevice::ReadOnly)) {
        static const QRegularExpression path(QStringLiteral("\"path\"\\s+\"([^\"]+)\""));
        for (auto it = path.globalMatch(QString::fromUtf8(f.readAll())); it.hasNext();) {
            paths.insert(it.next().captured(1));
        }
    }
    QStringList libs;
    for (const QString& p : paths) {
        libs << p + QStringLiteral("/steamapps");
    }
    return libs;
}

// Unlike the manifest, which Steam only flushes now and then, this is current.
inline qint64 stagedBytes(const QString& lib, const QString& appid)
{
    qint64 total = 0;
    for (QDirIterator it(lib + QStringLiteral("/downloading/") + appid, QDir::Files, QDirIterator::Subdirectories); it.hasNext();) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

inline qint64 field(const QString& text, const char* name)
{
    const auto m = QRegularExpression(QStringLiteral("\"%1\"\\s+\"(\\d+)\"").arg(QLatin1String(name))).match(text);
    return m.hasMatch() ? m.captured(1).toLongLong() : -1;
}

// 0..1 over every running download, -1 when nothing downloads.
inline double progress(const QString& steam)
{
    qint64 total = 0;
    qint64 done = 0;
    for (const QString& lib : libraries(steam)) {
        for (const QFileInfo& manifest : QDir(lib).entryInfoList({ QStringLiteral("appmanifest_*.acf") }, QDir::Files)) {
            QFile f(manifest.filePath());
            if (!f.open(QIODevice::ReadOnly)) {
                continue;
            }
            const QString text = QString::fromUtf8(f.readAll());
            const qint64 flags = field(text, "StateFlags");
            const qint64 toStage = field(text, "BytesToStage");
            const qint64 staged = field(text, "BytesStaged");
            if (flags < 0 || !(flags & Updating) || toStage < 0 || staged < 0) {
                continue;
            }
            const QString appid = manifest.baseName().mid(QStringLiteral("appmanifest_").size());
            total += toStage;
            done += std::max(stagedBytes(lib, appid), staged);
        }
    }
    return total > 0 ? std::min(double(done) / double(total), 1.0) : -1;
}
}
