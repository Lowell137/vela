// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QList>

// The kitty graphics protocol: a program sends ESC _ G <keys> ; <base64>, ended by
// ST (ESC and a backslash), with a picture, in chunks when it's big. libvterm
// ignores these, so they're taken out of the stream here, whole, and the text
// around them goes on.
class Graphics {
public:
    struct Piece {
        QByteArray text; // what libvterm reads, when it's not a command
        bool command = false;
        QHash<QByteArray, QByteArray> keys; // "a=T,f=100": the control keys, the first chunk's
        QByteArray payload; // the base64, all the chunks gathered
    };

    // Splits the bytes into text and commands; a command cut short waits for the rest.
    QList<Piece> split(const QByteArray& bytes);

    // The picture a command makes: PNG, GIF (its first frame) and any format Qt reads,
    // or raw pixels with f=24 or f=32. Null when it can't be read.
    static QImage decode(const QHash<QByteArray, QByteArray>& keys, const QByteArray& payload);

private:
    QByteArray m_pending; // the start of a command, or of the ESC that may start one
    bool m_gathering = false; // a command sent in chunks, the first one's keys in m_keys
    QHash<QByteArray, QByteArray> m_keys;
    QByteArray m_chunks;
};
