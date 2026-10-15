// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusContext>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
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

Q_SIGNALS:
    void currentChanged();

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

    QHash<QString, Player> m_players; // by unique bus name
    QVariantMap m_current;

    // The bus name of the player the keys are for; empty if there is none.
    QString target() const;

    QString m_selected; // the unique bus name the user picked for the card, or empty
    QString m_shown; // the one the card shows
    QString m_lastPlaying; // the unique bus name of the player that played last
};
