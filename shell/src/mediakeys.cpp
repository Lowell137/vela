// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mediakeys.h"
#include "mediamatch.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QUrl>

#include <algorithm>

namespace {

const QString mprisPrefix = QStringLiteral("org.mpris.MediaPlayer2.");
const QString mprisPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString playerInterface = QStringLiteral("org.mpris.MediaPlayer2.Player");
const QString propertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");
const QString rootInterface = QStringLiteral("org.mpris.MediaPlayer2");

QString method(const QString& key)
{
    static const QHash<QString, QString> methods {
        { QStringLiteral("play-pause"), QStringLiteral("PlayPause") },
        { QStringLiteral("pause"), QStringLiteral("Pause") },
        { QStringLiteral("stop"), QStringLiteral("Stop") },
        { QStringLiteral("next"), QStringLiteral("Next") },
        { QStringLiteral("previous"), QStringLiteral("Previous") },
    };
    return methods.value(key);
}

// Values nested in a variant (Metadata, xesam:artist) come still marshalled.
QVariantMap toMap(const QVariant& v)
{
    return v.canConvert<QDBusArgument>() ? qdbus_cast<QVariantMap>(v.value<QDBusArgument>()) : v.toMap();
}
QStringList toStringList(const QVariant& v)
{
    return v.canConvert<QDBusArgument>() ? qdbus_cast<QStringList>(v.value<QDBusArgument>()) : v.toStringList();
}

// "Playing", "Paused" or "Stopped"; empty if the player doesn't answer in time
// (a hung player mustn't freeze the shell).
QString playbackStatus(const QString& service)
{
    QDBusMessage get = QDBusMessage::createMethodCall(service, mprisPath, propertiesInterface, QStringLiteral("Get"));
    get << playerInterface << QStringLiteral("PlaybackStatus");
    const QDBusMessage reply = QDBusConnection::sessionBus().call(get, QDBus::Block, 300);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return QString();
    }
    return reply.arguments().first().value<QDBusVariant>().variant().toString();
}

} // namespace

MediaKeys::MediaKeys(QObject* parent)
    : QObject(parent)
{
    m_positionTimer.setInterval(1000);
    connect(&m_positionTimer, &QTimer::timeout, this, &MediaKeys::pollPosition);
    // Every player's changes (any sender): which one started playing.
    QDBusConnection::sessionBus().connect(QString(), mprisPath, propertiesInterface, QStringLiteral("PropertiesChanged"),
        this, SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
    // Players coming and going, for the card.
    QDBusConnection::sessionBus().connect(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("NameOwnerChanged"),
        this, SLOT(onNameOwnerChanged(QString, QString, QString)));
    if (QDBusConnectionInterface* bus = QDBusConnection::sessionBus().interface()) {
        for (const QString& name : bus->registeredServiceNames().value()) {
            if (name.startsWith(mprisPrefix)) {
                add(name, bus->serviceOwner(name).value());
            }
        }
    }
}

void MediaKeys::onNameOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner)
{
    if (!name.startsWith(mprisPrefix)) {
        return;
    }
    if (!oldOwner.isEmpty() && m_players.value(oldOwner).name == name) {
        m_players.remove(oldOwner);
    }
    add(name, newOwner);
    update();
}

void MediaKeys::add(const QString& name, const QString& owner)
{
    // playerctld only mirrors the other players: the card would show one twice.
    if (owner.isEmpty() || name == mprisPrefix + QLatin1String("playerctld")) {
        return;
    }
    m_players[owner].name = name;
    fetch(owner, rootInterface);
    fetch(owner, playerInterface);
}

void MediaKeys::fetch(const QString& owner, const QString& interface)
{
    QDBusMessage getAll = QDBusMessage::createMethodCall(owner, mprisPath, propertiesInterface, QStringLiteral("GetAll"));
    getAll << interface;
    auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(getAll, 2000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, owner, interface](QDBusPendingCallWatcher* w) {
        w->deleteLater();
        const QDBusPendingReply<QVariantMap> reply = *w;
        auto it = m_players.find(owner);
        if (reply.isError() || it == m_players.end()) {
            return;
        }
        (interface == rootInterface ? it->root : it->player) = reply.value();
        update();
    });
}

void MediaKeys::onPropertiesChanged(const QString& interface, const QVariantMap& changed, const QStringList& invalidated)
{
    if (interface == playerInterface && changed.value(QStringLiteral("PlaybackStatus")).toString() == QLatin1String("Playing")) {
        m_lastPlaying = message().service();
    }
    auto it = m_players.find(message().service());
    if (it == m_players.end() || (interface != playerInterface && interface != rootInterface)) {
        return;
    }
    QVariantMap& properties = interface == rootInterface ? it->root : it->player;
    properties.insert(changed);
    if (!invalidated.isEmpty()) {
        fetch(it.key(), interface);
    }
    update();
}

void MediaKeys::update()
{
    // Like target(): the one playing (the last to start, if more do), the
    // last to play, a paused one, any.
    QString playing, last, paused, any;
    for (auto it = m_players.cbegin(); it != m_players.cend(); ++it) {
        if (it->player.isEmpty()) {
            continue; // not answered (yet): nothing to show
        }
        const QString status = it->player.value(QStringLiteral("PlaybackStatus")).toString();
        const bool wasLast = it.key() == m_lastPlaying;
        if (status == QLatin1String("Playing") && (playing.isEmpty() || wasLast)) {
            playing = it.key();
        }
        if (wasLast) {
            last = it.key();
        }
        if (status == QLatin1String("Paused") && paused.isEmpty()) {
            paused = it.key();
        }
        if (any.isEmpty()) {
            any = it.key();
        }
    }
    // The card's player: the one the user picked, while it is there.
    QStringList ready;
    for (auto it = m_players.cbegin(); it != m_players.cend(); ++it) {
        if (!it->player.isEmpty()) {
            ready << it.key();
        }
    }
    std::sort(ready.begin(), ready.end(), [this](const QString& a, const QString& b) {
        return m_players[a].name != m_players[b].name ? m_players[a].name < m_players[b].name : a < b;
    });
    if (!ready.contains(m_selected)) {
        m_selected.clear();
    }
    QVariantMap current;
    for (const QString& owner : { m_selected, playing, last, paused, any }) {
        if (owner.isEmpty()) {
            continue;
        }
        m_shown = owner;
        const Player& p = m_players[owner];
        const QVariantMap metadata = toMap(p.player.value(QStringLiteral("Metadata")));
        // Cover art only from files and the web: a player's metadata is no
        // place for other schemes (image providers, qrc...).
        const QUrl art(metadata.value(QStringLiteral("mpris:artUrl")).toString());
        const bool artOk = art.isLocalFile() || art.scheme() == QLatin1String("http") || art.scheme() == QLatin1String("https");
        current = {
            { QStringLiteral("service"), p.name },
            { QStringLiteral("index"), int(ready.indexOf(owner)) + 1 },
            { QStringLiteral("count"), int(ready.size()) },
            { QStringLiteral("identity"), p.root.value(QStringLiteral("Identity")).toString() },
            { QStringLiteral("desktopEntry"), p.root.value(QStringLiteral("DesktopEntry")).toString() },
            { QStringLiteral("title"), metadata.value(QStringLiteral("xesam:title")).toString() },
            { QStringLiteral("artist"), toStringList(metadata.value(QStringLiteral("xesam:artist"))).join(QStringLiteral(", ")) },
            { QStringLiteral("artUrl"), artOk ? art.toString() : QString() },
            { QStringLiteral("playing"), p.player.value(QStringLiteral("PlaybackStatus")).toString() == QLatin1String("Playing") },
            { QStringLiteral("canGoNext"), p.player.value(QStringLiteral("CanGoNext"), true).toBool() },
            { QStringLiteral("canGoPrevious"), p.player.value(QStringLiteral("CanGoPrevious"), true).toBool() },
            // µs to s; the player's own volume (-1: it has none).
            { QStringLiteral("length"), metadata.value(QStringLiteral("mpris:length")).toLongLong() / 1e6 },
            { QStringLiteral("canSeek"), p.player.value(QStringLiteral("CanSeek"), false).toBool() },
            { QStringLiteral("volume"), p.player.contains(QStringLiteral("Volume")) ? p.player.value(QStringLiteral("Volume")).toDouble() : -1.0 },
        };
        break;
    }
    QVariantList list;
    for (const QString& owner : ready) {
        const Player& p = m_players[owner];
        const QVariantMap metadata = toMap(p.player.value(QStringLiteral("Metadata")));
        const QString status = p.player.value(QStringLiteral("PlaybackStatus")).toString();
        list << QVariantMap {
            { QStringLiteral("service"), p.name },
            { QStringLiteral("desktopEntry"), p.root.value(QStringLiteral("DesktopEntry")).toString() },
            { QStringLiteral("identity"), p.root.value(QStringLiteral("Identity")).toString() },
            { QStringLiteral("title"), metadata.value(QStringLiteral("xesam:title")).toString() },
            { QStringLiteral("artist"), toStringList(metadata.value(QStringLiteral("xesam:artist"))).join(QStringLiteral(", ")) },
            { QStringLiteral("status"), status },
            { QStringLiteral("playing"), status == QLatin1String("Playing") },
            { QStringLiteral("canGoNext"), p.player.value(QStringLiteral("CanGoNext"), true).toBool() },
            { QStringLiteral("canGoPrevious"), p.player.value(QStringLiteral("CanGoPrevious"), true).toBool() },
        };
    }
    if (list != m_playersList) {
        m_playersList = list;
        Q_EMIT playersChanged();
    }
    if (current != m_current) {
        m_current = current;
        Q_EMIT currentChanged();
    }
}

QString MediaKeys::target() const
{
    QDBusConnectionInterface* bus = QDBusConnection::sessionBus().interface();
    if (!bus) {
        return QString();
    }
    QString playing, last, paused, any;
    for (const QString& name : bus->registeredServiceNames().value()) {
        if (!name.startsWith(mprisPrefix)) {
            continue;
        }
        const bool wasLast = !m_lastPlaying.isEmpty() && bus->serviceOwner(name).value() == m_lastPlaying;
        const QString status = playbackStatus(name);
        if (status == QLatin1String("Playing") && (playing.isEmpty() || wasLast)) {
            playing = name;
        }
        if (wasLast) {
            last = name;
        }
        if (status == QLatin1String("Paused") && paused.isEmpty()) {
            paused = name;
        }
        if (any.isEmpty()) {
            any = name;
        }
    }
    for (const QString& name : { playing, last, paused, any }) {
        if (!name.isEmpty()) {
            return name;
        }
    }
    return QString();
}

void MediaKeys::press(const QString& key)
{
    const QString name = method(key);
    const QString service = name.isEmpty() ? QString() : target();
    if (service.isEmpty()) {
        return;
    }
    QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(service, mprisPath, playerInterface, name));
}

void MediaKeys::cycle(int delta)
{
    QStringList ready;
    for (auto it = m_players.cbegin(); it != m_players.cend(); ++it) {
        if (!it->player.isEmpty()) {
            ready << it.key();
        }
    }
    const int n = ready.size();
    if (n < 2 || !ready.contains(m_shown)) {
        return;
    }
    std::sort(ready.begin(), ready.end(), [this](const QString& a, const QString& b) {
        return m_players[a].name != m_players[b].name ? m_players[a].name < m_players[b].name : a < b;
    });
    m_selected = ready.at((ready.indexOf(m_shown) + delta % n + n) % n);
    update();
}

void MediaKeys::command(const QString& key)
{
    const QString name = method(key);
    const QString service = m_current.value(QStringLiteral("service")).toString();
    if (!name.isEmpty() && !service.isEmpty()) {
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(service, mprisPath, playerInterface, name));
    }
}

QVariantMap MediaKeys::playerFor(const QString& appId, const QString& title, int sameAppWindows) const
{
    return MediaMatch::pick(m_playersList, appId, title, sameAppWindows);
}

void MediaKeys::commandTo(const QString& service, const QString& key)
{
    const QString name = method(key);
    if (!name.isEmpty() && service.startsWith(mprisPrefix)) {
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(service, mprisPath, playerInterface, name));
    }
}

// Position (µs) of the shown player, asked asynchronously.
void MediaKeys::pollPosition()
{
    const QString service = m_current.value(QStringLiteral("service")).toString();
    if (service.isEmpty()) {
        if (m_position != -1) {
            m_position = -1;
            Q_EMIT positionChanged();
        }
        return;
    }
    QDBusMessage get = QDBusMessage::createMethodCall(service, mprisPath, propertiesInterface, QStringLiteral("Get"));
    get << playerInterface << QStringLiteral("Position");
    auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(get, 1000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, service](QDBusPendingCallWatcher* w) {
        w->deleteLater();
        const QDBusPendingReply<QDBusVariant> reply = *w;
        const double seconds = reply.isError() || service != m_current.value(QStringLiteral("service")).toString()
            ? -1.0 : reply.value().variant().toLongLong() / 1e6;
        if (seconds != m_position) {
            m_position = seconds;
            Q_EMIT positionChanged();
        }
    });
}

void MediaKeys::watchPosition(bool on)
{
    m_watchers = std::max(0, m_watchers + (on ? 1 : -1));
    if (m_watchers > 0 && !m_positionTimer.isActive()) {
        m_positionTimer.start();
        pollPosition();
    } else if (m_watchers == 0) {
        m_positionTimer.stop();
    }
}

void MediaKeys::seekTo(double seconds)
{
    const QString service = m_current.value(QStringLiteral("service")).toString();
    if (service.isEmpty() || m_position < 0 || !m_current.value(QStringLiteral("canSeek")).toBool()) {
        return;
    }
    QDBusMessage seek = QDBusMessage::createMethodCall(service, mprisPath, playerInterface, QStringLiteral("Seek"));
    seek << qlonglong((seconds - m_position) * 1e6);
    QDBusConnection::sessionBus().asyncCall(seek);
    m_position = seconds;
    Q_EMIT positionChanged();
}

void MediaKeys::setVolume(double volume)
{
    const QString service = m_current.value(QStringLiteral("service")).toString();
    if (service.isEmpty()) {
        return;
    }
    QDBusMessage set = QDBusMessage::createMethodCall(service, mprisPath, propertiesInterface, QStringLiteral("Set"));
    set << playerInterface << QStringLiteral("Volume") << QVariant::fromValue(QDBusVariant(std::clamp(volume, 0.0, 1.0)));
    QDBusConnection::sessionBus().asyncCall(set);
}
