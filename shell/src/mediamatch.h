// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QVariantList>
#include <QVariantMap>

// Which MPRIS player a window belongs to, for the taskbar preview's media
// strip. `players` is [{service, desktopEntry, title, status, ...}] (see
// MediaKeys::players). Empty result: no strip.
namespace MediaMatch {

inline QString normalized(QString id)
{
    id = id.toLower();
    if (id.endsWith(QLatin1String(".desktop"))) {
        id.chop(8);
    }
    return id;
}

inline bool sameApp(const QString& a, const QString& b)
{
    const QString x = normalized(a), y = normalized(b);
    if (x.isEmpty() || y.isEmpty()) {
        return false;
    }
    // "com.spotify.Client" and "spotify" are the same app: last part, too.
    return x == y || x.section(QLatin1Char('.'), -1) == y.section(QLatin1Char('.'), -1);
}

// The service's name without the org.mpris.MediaPlayer2. prefix and the
// browsers' ".instanceNNN" suffix.
inline QString busName(QString service)
{
    service.remove(QStringLiteral("org.mpris.MediaPlayer2."));
    const int i = service.indexOf(QLatin1String(".instance"));
    return i > 0 ? service.left(i) : service;
}

// Shown only for a player that plays, or one paused with a track (else the
// strip would vanish on Pause and there'd be no way back).
inline bool visible(const QVariantMap& p)
{
    const QString status = p.value(QStringLiteral("status")).toString();
    return status == QLatin1String("Playing")
        || (status == QLatin1String("Paused") && !p.value(QStringLiteral("title")).toString().isEmpty());
}

// `appId`/`title`: the window's; `sameAppWindows`: how many windows share the
// appId (browser windows: more than one means only the title can tell).
// Order: DesktopEntry, bus name. (PID is not offered by the compositor's
// toplevel protocol.)
inline QVariantMap pick(const QVariantList& players, const QString& appId, const QString& title, int sameAppWindows)
{
    QVariantList byEntry, byName;
    for (const QVariant& v : players) {
        const QVariantMap p = v.toMap();
        if (!visible(p)) {
            continue;
        }
        if (sameApp(p.value(QStringLiteral("desktopEntry")).toString(), appId)) {
            byEntry << p;
        } else if (sameApp(busName(p.value(QStringLiteral("service")).toString()), appId)) {
            byName << p;
        }
    }
    const QVariantList& candidates = !byEntry.isEmpty() ? byEntry : byName;
    if (candidates.isEmpty()) {
        return {};
    }
    QVariantMap best;
    int titled = 0;
    for (const QVariant& v : candidates) {
        const QVariantMap p = v.toMap();
        const QString track = p.value(QStringLiteral("title")).toString();
        if (!track.isEmpty() && title.contains(track, Qt::CaseInsensitive)) {
            if (titled++ == 0 || p.value(QStringLiteral("status")).toString() == QLatin1String("Playing")) {
                best = p;
            }
        }
    }
    if (titled > 0) {
        return best;
    }
    // No title agrees: one window of one player is that player's.
    if (sameAppWindows <= 1 && candidates.size() == 1) {
        return candidates.first().toMap();
    }
    return {};
}

} // namespace MediaMatch
