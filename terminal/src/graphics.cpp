// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "graphics.h"

#include <QFile>

namespace {
const QByteArray kStart = QByteArrayLiteral("\x1b_G");
const QByteArray kEnd = QByteArrayLiteral("\x1b\\");
// shortcut: a command is kept whole in memory, one that's bigger than this is dropped
constexpr qsizetype kMaxCommand = 64 * 1024 * 1024;
}

QList<Graphics::Piece> Graphics::split(const QByteArray& bytes)
{
    QList<Piece> pieces;
    const auto addText = [&pieces](const QByteArray& text) {
        if (!text.isEmpty()) {
            Piece piece;
            piece.text = text;
            pieces.append(piece);
        }
    };
    const auto addCommand = [&](const QByteArray& body) {
        const qsizetype separator = body.indexOf(';');
        const QByteArray control = separator < 0 ? body : body.left(separator);
        const QByteArray payload = separator < 0 ? QByteArray() : body.mid(separator + 1);
        QHash<QByteArray, QByteArray> keys;
        for (const QByteArray& pair : control.split(',')) {
            const qsizetype equal = pair.indexOf('=');
            if (equal > 0) {
                keys.insert(pair.left(equal), pair.mid(equal + 1));
            }
        }
        if (keys.value("m") == "1") {
            if (!m_gathering) {
                m_gathering = true;
                m_keys = keys;
                m_chunks.clear();
            }
            m_chunks += payload;
            return;
        }
        Piece piece;
        piece.command = true;
        if (m_gathering) {
            piece.keys = m_keys;
            piece.payload = m_chunks + payload;
            m_gathering = false;
            m_chunks.clear();
        } else {
            piece.keys = keys;
            piece.payload = payload;
        }
        pieces.append(piece);
    };

    const QByteArray data = m_pending + bytes;
    m_pending.clear();
    qsizetype pos = 0;
    while (pos < data.size()) {
        const qsizetype start = data.indexOf(kStart, pos);
        if (start < 0) {
            // An ESC or ESC _ at the very end may be the start of a command.
            qsizetype keep = 0;
            for (qsizetype n = qMin<qsizetype>(kStart.size() - 1, data.size() - pos); n > 0; n--) {
                if (data.right(n) == kStart.left(n)) {
                    keep = n;
                    break;
                }
            }
            addText(data.mid(pos, data.size() - pos - keep));
            m_pending = data.right(keep);
            break;
        }
        addText(data.mid(pos, start - pos));
        const qsizetype end = data.indexOf(kEnd, start + kStart.size());
        if (end < 0) {
            m_pending = data.mid(start);
            if (m_pending.size() > kMaxCommand) {
                m_pending.clear();
            }
            break;
        }
        addCommand(data.mid(start + kStart.size(), end - start - kStart.size()));
        pos = end + kEnd.size();
    }
    return pieces;
}

QImage Graphics::decode(const QHash<QByteArray, QByteArray>& keys, const QByteArray& payload)
{
    QByteArray data = QByteArray::fromBase64(payload);
    const QByteArray format = keys.value("f", "32");
    if (keys.value("t") == "f") {
        // The payload is the path of a picture file. Only pictures are read this way, never
        // raw pixels: any file would show up as noise on screen.
        QFile file(QFile::decodeName(data));
        if (format != "100" || !file.open(QIODevice::ReadOnly) || file.size() > kMaxCommand) {
            return {};
        }
        data = file.readAll();
    }
    if (format == "100") {
        return QImage::fromData(data); // PNG, GIF's first frame, anything Qt reads
    }
    if (format != "24" && format != "32") {
        return {};
    }
    const int width = keys.value("s").toInt();
    const int height = keys.value("v").toInt();
    const int bytes = format == "24" ? 3 : 4;
    if (width <= 0 || height <= 0 || data.size() < qsizetype(width) * height * bytes) {
        return {};
    }
    const QImage raw(reinterpret_cast<const uchar*>(data.constData()), width, height, width * bytes,
        bytes == 3 ? QImage::Format_RGB888 : QImage::Format_RGBA8888);
    return raw.copy();
}
