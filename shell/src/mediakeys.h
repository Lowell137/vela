// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusContext>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// The keyboard's media keys, whatever window has the focus, like Windows: they
// go to the media player (MPRIS on the session bus) that is playing, otherwise
// to the one that played last, otherwise to a paused one.
// The same player, for the quick settings' media card: `current` is its
// {service, identity, desktopEntry, title, artist, artUrl, playing,
// canGoNext, canGoPrevious, index, count}, empty without a player. Kept from the players'
// signals and asynchronous calls, so a hung player can't freeze the card.
class MediaKeys : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
    // Every player: [{service, desktopEntry, identity, title, artist, status, playing, canGoNext, canGoPrevious}]
    Q_PROPERTY(QVariantList players READ players NOTIFY playersChanged)
    // Where the shown player is, in seconds (-1 unknown): asked once a second
    // while someone watches (the Game Bar's Media widget), as players don't
    // announce it.
    Q_PROPERTY(double position READ position NOTIFY positionChanged)

public:
    explicit MediaKeys(QObject* parent = nullptr);

    // "play-pause", "pause", "stop", "next", "previous".
    void press(const QString& key);
    // The same, for the player the card shows.
    Q_INVOKABLE void command(const QString& key);

    // The card's previous (-1) / next (+1) player; the pick stays until that
    // player is gone.
    Q_INVOKABLE void cycle(int delta);

    QVariantMap current() const { return m_current; }
    QVariantList players() const { return m_playersList; }

    // The player of a window (taskbar preview's strip): {} for none.
    Q_INVOKABLE QVariantMap playerFor(const QString& appId, const QString& title, int sameAppWindows) const;
    // `key` (see command) for one player, by its bus name.
    Q_INVOKABLE void commandTo(const QString& service, const QString& key);
    // Seek to `seconds` in the shown player (Seek by the difference) and set
    // its own volume (0..1).
    Q_INVOKABLE void seekTo(double seconds);
    Q_INVOKABLE void setVolume(double volume);
    // Counted: the position is polled while someone watches.
    Q_INVOKABLE void watchPosition(bool on);
    double position() const { return m_position; }

Q_SIGNALS:
    void currentChanged();
    void playersChanged();
    void positionChanged();

private Q_SLOTS:
    void onPropertiesChanged(const QString& interface, const QVariantMap& changed, const QStringList& invalidated);
    void onNameOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner);

private:
    struct Player {
        QString name; // org.mpris.MediaPlayer2.*
        QVariantMap root; // org.mpris.MediaPlayer2's properties
        QVariantMap player; // org.mpris.MediaPlayer2.Player's
    };
    void add(const QString& name, const QString& owner);
    void fetch(const QString& owner, const QString& interface);
    void update();
    void pollPosition();

    QHash<QString, Player> m_players; // by unique bus name
    QVariantMap m_current;
    QVariantList m_playersList;

    // The bus name of the player the keys are for; empty if there is none.
    QString target() const;

    QString m_selected; // the unique bus name the user picked for the card, or empty
    QString m_shown; // the one the card shows
    QTimer m_positionTimer;
    int m_watchers = 0;
    double m_position = -1;
    QString m_lastPlaying; // the unique bus name of the player that played last
};
