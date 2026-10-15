// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The images of the opened one's folder (`Album` in QML): the arrows go
// through them, in the order a file manager lists them.

#include <QHash>
#include <QList>
#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QUrl>

class Album : public QObject {
    Q_OBJECT
    Q_PROPERTY(QList<QUrl> files READ files NOTIFY filesChanged)
    Q_PROPERTY(int index READ index WRITE setIndex NOTIFY indexChanged)
    Q_PROPERTY(QUrl current READ current NOTIFY indexChanged)
    Q_PROPERTY(QString name READ name NOTIFY indexChanged)
    Q_PROPERTY(QString folder READ folder NOTIFY indexChanged)
    Q_PROPERTY(QString size READ size NOTIFY indexChanged) // "1.2 MB"
    Q_PROPERTY(QString modified READ modified NOTIFY indexChanged)
    Q_PROPERTY(QSize dimensions READ dimensions NOTIFY indexChanged) // as shown: turned like the camera was
    Q_PROPERTY(bool animated READ animated NOTIFY indexChanged)
    Q_PROPERTY(bool scalable READ scalable NOTIFY indexChanged) // an SVG: redrawn at every size

public:
    using QObject::QObject;

    QList<QUrl> files() const { return m_files; }
    int index() const { return m_index; }
    void setIndex(int index);
    QUrl current() const { return m_files.value(m_index); }
    QString name() const;
    QString folder() const;
    QString size() const;
    QString modified() const;
    QSize dimensions() const;
    bool animated() const;
    bool scalable() const;

    // An image, with its folder's others, or a folder, from its first image.
    Q_INVOKABLE void open(const QUrl& url);
    // The shown image to the trash. The error, empty if it went.
    Q_INVOKABLE QString trash();
    Q_INVOKABLE void copy() const; // the image itself, to the clipboard
    Q_INVOKABLE void setAsWallpaper() const; // Vela's desktop background
    Q_INVOKABLE void openInPaint() const; // vela-paint, installed next to Photos
    Q_INVOKABLE void print() const; // on one page, through the system's dialog
    Q_INVOKABLE QString filter() const; // "*.png *.jpg ...", for the Open dialog
    // What a QML Image shows for a file: the file, or, for the formats only
    // ImageMagick reads (AVIF, HEIC, RAW...), the image provider's picture.
    Q_INVOKABLE QUrl shown(const QUrl& file) const;
    // The provider found the size of a picture (queued from its thread).
    Q_INVOKABLE void extraDecoded(const QString& path, const QSize& size);

signals:
    void filesChanged();
    void indexChanged();

private:
    QList<QUrl> m_files;
    QHash<QString, QSize> m_sizes; // of the extra formats, once decoded
    int m_index = -1;
};
