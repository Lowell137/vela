// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "launcherbadges.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QtGlobal>

LauncherBadges::LauncherBadges(QObject* parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    // Any sender, any path: the apps send it from their own.
    bus.connect(QString(), QString(), QStringLiteral("com.canonical.Unity.LauncherEntry"), QStringLiteral("Update"),
        this, SLOT(onUpdate(QString, QVariantMap)));
    bus.connect(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("NameOwnerChanged"),
        this, SLOT(onNameOwnerChanged(QString, QString, QString)));
}

void LauncherBadges::onUpdate(const QString& appUri, const QVariantMap& props)
{
    update(appUri, props, message().service());
}

void LauncherBadges::update(const QString& appUri, const QVariantMap& props, const QString& owner)
{
    const QString key = vela::badgeKey(appUri);
    if (key.isEmpty()) {
        return;
    }
    // The properties not sent keep their value.
    Badge badge = m_badges.value(key);
    badge.owner = owner;
    if (props.contains(QStringLiteral("count")) || props.contains(QStringLiteral("count-visible"))) {
        const qlonglong count = props.value(QStringLiteral("count"), badge.count).toLongLong();
        badge.count = props.value(QStringLiteral("count-visible"), true).toBool() ? qMax<qlonglong>(0, count) : 0;
    }
    if (props.contains(QStringLiteral("progress")) || props.contains(QStringLiteral("progress-visible"))) {
        const double progress = qBound(0.0, props.value(QStringLiteral("progress")).toDouble(), 1.0);
        // Full means done, like the bar of the Windows taskbar.
        const bool visible = props.value(QStringLiteral("progress-visible"), true).toBool();
        badge.progress = visible && progress < 1.0 ? progress : -1;
    }
    if (props.contains(QStringLiteral("urgent"))) {
        badge.urgent = props.value(QStringLiteral("urgent")).toBool();
    }
    if (badge.count == 0 && badge.progress < 0 && !badge.urgent) {
        if (!m_badges.remove(key)) {
            return;
        }
    } else {
        m_badges.insert(key, badge);
    }
    ++m_revision;
    emit changed();
}

void LauncherBadges::onNameOwnerChanged(const QString& name, const QString&, const QString& newOwner)
{
    // The app left the bus: its badges go with it.
    if (!newOwner.isEmpty() || !name.startsWith(QLatin1Char(':'))) {
        return;
    }
    bool any = false;
    for (auto it = m_badges.begin(); it != m_badges.end();) {
        if (it->owner == name) {
            it = m_badges.erase(it);
            any = true;
        } else {
            ++it;
        }
    }
    if (any) {
        ++m_revision;
        emit changed();
    }
}

QVariantMap LauncherBadges::entry(const QString& desktopId) const
{
    const QString key = vela::badgeKey(desktopId);
    auto it = m_badges.constFind(key);
    if (it == m_badges.constEnd()) {
        // The other spelling: "discord" against "com.discordapp.discord".
        const QString shortKey = vela::badgeShortKey(key);
        for (auto i = m_badges.constBegin(); i != m_badges.constEnd(); ++i) {
            if (vela::badgeShortKey(i.key()) == shortKey) {
                it = i;
                break;
            }
        }
    }
    if (it == m_badges.constEnd()) {
        return { { QStringLiteral("count"), 0 }, { QStringLiteral("progress"), -1.0 }, { QStringLiteral("urgent"), false } };
    }
    return { { QStringLiteral("count"), it->count }, { QStringLiteral("progress"), it->progress }, { QStringLiteral("urgent"), it->urgent } };
}
