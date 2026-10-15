// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "formats.h"

#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QMutex>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>

namespace {

// The picture formats worth listing: ImageMagick reads hundreds more (video
// containers, fonts, plots) that aren't pictures in a folder of photos.
const QSet<QString>& pictures()
{
    static const QSet<QString> set = {
        QStringLiteral("avif"), QStringLiteral("heic"), QStringLiteral("heif"), QStringLiteral("hif"),
        QStringLiteral("jxl"), QStringLiteral("jp2"), QStringLiteral("j2k"), QStringLiteral("jpf"),
        QStringLiteral("jpx"), QStringLiteral("j2c"), QStringLiteral("jfif"), QStringLiteral("jpe"),
        QStringLiteral("psd"), QStringLiteral("psb"), QStringLiteral("xcf"), QStringLiteral("exr"),
        QStringLiteral("hdr"), QStringLiteral("pcx"), QStringLiteral("pnm"), QStringLiteral("pam"),
        QStringLiteral("dds"), QStringLiteral("fits"), QStringLiteral("pfm"), QStringLiteral("sgi"),
        // camera RAW
        QStringLiteral("3fr"), QStringLiteral("arw"), QStringLiteral("cr2"), QStringLiteral("cr3"),
        QStringLiteral("crw"), QStringLiteral("dcr"), QStringLiteral("dng"), QStringLiteral("erf"),
        QStringLiteral("kdc"), QStringLiteral("mef"), QStringLiteral("mrw"), QStringLiteral("nef"),
        QStringLiteral("nrw"), QStringLiteral("orf"), QStringLiteral("pef"), QStringLiteral("raf"),
        QStringLiteral("raw"), QStringLiteral("rw2"), QStringLiteral("sr2"), QStringLiteral("srf"),
        QStringLiteral("srw"), QStringLiteral("x3f"),
    };
    return set;
}

QString magick()
{
    static const QString program = [] {
        for (const char* name : { "magick", "convert" }) {
            const QString found = QStandardPaths::findExecutable(QLatin1String(name));
            if (!found.isEmpty()) {
                return found;
            }
        }
        return QString();
    }();
    return program;
}

QString suffix(const QString& path)
{
    return QFileInfo(path).suffix().toLower();
}

// ImageMagick, fed the file through its standard input so a name with
// brackets or wildcards isn't read as a frame or a pattern.
QImage decode(const QString& path, QSize bounds, QString* error)
{
    const QString program = magick();
    if (program.isEmpty()) {
        if (error) {
            *error = QStringLiteral("no decoder for this format");
        }
        return {};
    }
    QStringList args = { QStringLiteral("-quiet"), suffix(path) + QStringLiteral(":-[0]"), QStringLiteral("-auto-orient"),
        QStringLiteral("-colorspace"), QStringLiteral("sRGB") };
    if (bounds.isValid() && !bounds.isEmpty()) {
        args << QStringLiteral("-thumbnail") << QStringLiteral("%1x%2>").arg(bounds.width()).arg(bounds.height());
    }
    args << QStringLiteral("-depth") << QStringLiteral("8") << QStringLiteral("png32:-");

    QProcess process;
    process.setStandardInputFile(path);
    process.start(program, args);
    // shortcut: a huge RAW can take longer; the viewer shows the previous
    // picture meanwhile (the image is loaded asynchronously).
    if (!process.waitForFinished(60000) || process.exitCode() != 0) {
        if (error) {
            *error = QString::fromUtf8(process.readAllStandardError()).trimmed();
        }
        process.kill();
        return {};
    }
    QImage image;
    image.loadFromData(process.readAllStandardOutput(), "PNG");
    if (image.isNull() && error) {
        *error = QStringLiteral("the decoder wrote no picture");
    }
    return image;
}

} // namespace

namespace formats {

QStringList parseReadable(const QByteArray& list)
{
    QStringList out;
    const QList<QByteArray> lines = list.split('\n');
    for (const QByteArray& line : lines) {
        // "      AVIF  HEIC      rw+   AV1 Image File Format (1.23.6)"
        const QList<QByteArray> parts = line.simplified().split(' ');
        if (parts.size() < 3 || parts.at(2).size() != 3 || parts.at(2).at(0) != 'r') {
            continue;
        }
        QString name = QString::fromLatin1(parts.at(0)).toLower();
        if (name.endsWith(QLatin1Char('*'))) {
            name.chop(1);
        }
        if (pictures().contains(name) && !out.contains(name)) {
            out.append(name);
        }
    }
    return out;
}

QStringList extra()
{
    static const QStringList list = [] {
        const QString program = magick();
        if (program.isEmpty()) {
            return QStringList();
        }
        QProcess process;
        process.start(program, { QStringLiteral("-list"), QStringLiteral("format") });
        if (!process.waitForFinished(10000)) {
            process.kill();
            return QStringList();
        }
        QStringList readable = parseReadable(process.readAllStandardOutput());
        // What Qt reads itself stays Qt's (and is faster).
        const QList<QByteArray> native = QImageReader::supportedImageFormats();
        readable.removeIf([&native](const QString& name) { return native.contains(name.toLatin1()); });
        return readable;
    }();
    return list;
}

bool isExtra(const QString& path)
{
    return extra().contains(suffix(path));
}

QImage read(const QString& path, QSize bounds, QString* error)
{
    QImageReader reader(path);
    reader.setAutoTransform(true); // the camera's orientation
    QImage image = reader.read();
    if (!image.isNull()) {
        if (bounds.isValid() && !bounds.isEmpty() && (image.width() > bounds.width() || image.height() > bounds.height())) {
            image = image.scaled(bounds, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        return image;
    }
    const QString qtError = reader.errorString();
    image = decode(path, bounds, error);
    if (image.isNull() && error && error->isEmpty()) {
        *error = qtError;
    }
    return image;
}

QUrl shown(const QUrl& file)
{
    if (!file.isLocalFile() || !isExtra(file.toLocalFile())) {
        return file;
    }
    const QByteArray id = file.toLocalFile().toUtf8().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QUrl(QStringLiteral("image://photo/") + QString::fromLatin1(id));
}

} // namespace formats

FormatImages::FormatImages(QObject* receiver)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_receiver(receiver)
{
}

QImage FormatImages::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    static QMutex lock;
    static QHash<QString, QImage> kept; // path + modified time -> the whole picture
    const QString path = QString::fromUtf8(QByteArray::fromBase64(id.toLatin1(), QByteArray::Base64UrlEncoding));
    const QString key = path + QLatin1Char('|') + QString::number(QFileInfo(path).lastModified().toMSecsSinceEpoch());

    // A small request is decoded small and not kept: it is a thumbnail.
    const bool small = requestedSize.width() > 0 || requestedSize.height() > 0;
    QImage image;
    {
        const QMutexLocker locker(&lock);
        image = kept.value(key);
    }
    if (image.isNull()) {
        const QSize bounds = small ? QSize(requestedSize.width() > 0 ? requestedSize.width() : 100000, requestedSize.height() > 0 ? requestedSize.height() : 100000) : QSize();
        image = formats::read(path, bounds);
        if (image.isNull()) {
            return {};
        }
        if (!small) {
            const QMutexLocker locker(&lock);
            if (kept.size() >= 4) {
                kept.erase(kept.begin());
            }
            kept.insert(key, image);
        }
    }
    if (m_receiver && !small) {
        QMetaObject::invokeMethod(m_receiver, "extraDecoded", Qt::QueuedConnection, Q_ARG(QString, path), Q_ARG(QSize, image.size()));
    }
    if (size) {
        *size = image.size();
    }
    return image;
}
