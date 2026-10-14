// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusContext>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace vela {
// "application://Discord.desktop", "com.discordapp.Discord.desktop" -> the
// bare lower-case name, to compare the two spellings of an app.
inline QString badgeKey(QString id)
{
    id = id.trimmed().toLower();
    if (id.startsWith(QLatin1String("application://"))) {
        id.remove(0, 14);
    }
    if (id.endsWith(QLatin1String(".desktop"))) {
        id.chop(8);
    }
    return id;
}
// The last dot-separated part: "com.discordapp.discord" -> "discord".
inline QString badgeShortKey(const QString& key)
{
    return key.mid(key.lastIndexOf(QLatin1Char('.')) + 1);
}
}

// The apps' badges (Unity LauncherEntry.Update on the session bus: unread
// count, progress, urgent), for the taskbar's buttons.
class LauncherBadges : public QObject, protected QDBusContext {
    Q_OBJECT
    // Bumped on every change, for the bindings of entry().
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    explicit LauncherBadges(QObject* parent = nullptr);

    int revision() const { return m_revision; }
    // {count, progress, urgent} of the app (any spelling of its desktop id),
    // with count = 0 and progress = -1 for nothing.
    Q_INVOKABLE QVariantMap entry(const QString& desktopId) const;

    // For the tests.
    void update(const QString& appUri, const QVariantMap& props, const QString& owner);

Q_SIGNALS:
    void changed();

private Q_SLOTS:
    void onUpdate(const QString& appUri, const QVariantMap& props);
    void onNameOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner);

private:
    struct Badge {
        QString owner;
        qlonglong count = 0;
        double progress = -1;
        bool urgent = false;
    };
    QHash<QString, Badge> m_badges; // by badgeKey()
    int m_revision = 0;
};
