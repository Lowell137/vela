// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Shared by Photos, Paint and the File Explorer (the thumbnails).
//
// The images Qt's own plugins can't read here (AVIF, HEIC, JPEG XL, PSD,
// OpenEXR, camera RAW...): ImageMagick decodes them, when it's installed.
// Nothing changes without it.

#include <QImage>
#include <QObject>
#include <QQuickImageProvider>
#include <QSize>
#include <QStringList>

namespace formats {

// The suffixes ImageMagick's `-list format` says it reads, of the ones that
// are pictures (not its video containers).
QStringList parseReadable(const QByteArray& list);

// What's readable through ImageMagick and not by Qt, as lowercase suffixes.
// Asked of the machine once.
QStringList extra();
bool isExtra(const QString& path);

// The image, turned like the camera held it; at most `bounds` big when it's
// valid (the decoder is asked for a small one). Empty with `error` set when
// nothing can read the file.
QImage read(const QString& path, QSize bounds = {}, QString* error = nullptr);

// `image://photo/<id>`: an extra format's picture for a QML Image.
QUrl shown(const QUrl& file);

} // namespace formats

// Serves `image://photo/`: decodes through formats::read(), keeps the last
// few whole pictures (the viewer and the strip ask for the same one) and
// tells `receiver` (Album::extraDecoded, queued) the size it found.
class FormatImages : public QQuickImageProvider {
public:
    explicit FormatImages(QObject* receiver);
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

private:
    QObject* m_receiver;
};
