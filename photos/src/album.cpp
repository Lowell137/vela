// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "album.h"

#include "imageformats.h"

#include "portalprint.h"

#include <QClipboard>
#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QLocale>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <algorithm>

namespace {

// What Qt can read here: it depends on the installed image plugins.
QStringList patterns()
{
    QStringList list;
    const QList<QByteArray> formats = QImageReader::supportedImageFormats();
    for (const QByteArray& format : formats) {
        list.append(QStringLiteral("*.") + QString::fromLatin1(format));
    }
    // AVIF, HEIC, JPEG XL, PSD, RAW...: through ImageMagick, when it's there.
    const QStringList extra = formats::extra();
    for (const QString& suffix : extra) {
        list.append(QStringLiteral("*.") + suffix);
    }
    return list;
}

} // namespace

void Album::setIndex(int index)
{
    if (index >= 0 && index < m_files.size() && index != m_index) {
        m_index = index;
        emit indexChanged();
    }
}

QString Album::name() const
{
    return current().fileName();
}

QString Album::folder() const
{
    return QFileInfo(current().toLocalFile()).path();
}

QString Album::size() const
{
    const QFileInfo info(current().toLocalFile());
    return info.exists() ? QLocale().formattedDataSize(info.size(), 2, QLocale::DataSizeTraditionalFormat) : QString();
}

QString Album::modified() const
{
    return QLocale().toString(QFileInfo(current().toLocalFile()).lastModified(), QLocale::ShortFormat);
}

QSize Album::dimensions() const
{
    if (m_index < 0) {
        return {};
    }
    if (formats::isExtra(current().toLocalFile())) {
        return m_sizes.value(current().toLocalFile()); // known once the provider decoded it
    }
    QImageReader reader(current().toLocalFile());
    const QSize size = reader.size();
    return reader.transformation() & QImageIOHandler::TransformationRotate90 ? size.transposed() : size;
}

bool Album::animated() const
{
    if (m_index < 0) {
        return false;
    }
    QImageReader reader(current().toLocalFile());
    return reader.supportsAnimation() && reader.imageCount() > 1;
}

bool Album::scalable() const
{
    return name().endsWith(QLatin1String(".svg"), Qt::CaseInsensitive)
        || name().endsWith(QLatin1String(".svgz"), Qt::CaseInsensitive);
}

void Album::open(const QUrl& url)
{
    const QFileInfo info(url.toLocalFile());
    QDir dir(info.isDir() ? info.filePath() : info.path());
    QStringList names = dir.entryList(patterns(), QDir::Files);
    QCollator order; // "a2" before "a10", like the File Explorer
    order.setNumericMode(true);
    order.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(names.begin(), names.end(), order);
    // The opened file is shown even if its name doesn't say it's an image.
    if (info.isFile() && !names.contains(info.fileName())) {
        names.append(info.fileName());
    }

    m_files.clear();
    for (const QString& name : std::as_const(names)) {
        m_files.append(QUrl::fromLocalFile(dir.filePath(name)));
    }
    m_index = info.isFile() ? names.indexOf(info.fileName()) : names.isEmpty() ? -1 : 0;
    emit filesChanged();
    emit indexChanged();
}

void Album::openInPaint() const
{
    if (m_index >= 0) {
        QProcess::startDetached(QCoreApplication::applicationDirPath() + QStringLiteral("/vela-paint"), { current().toLocalFile() });
    }
}

QString Album::trash()
{
    if (m_index < 0) {
        return {};
    }
    QFile file(current().toLocalFile());
    if (!file.moveToTrash()) {
        return file.errorString();
    }
    m_files.removeAt(m_index);
    m_index = std::min(m_index, int(m_files.size()) - 1);
    emit filesChanged();
    emit indexChanged();
    return {};
}

void Album::copy() const
{
    const QImage image = formats::read(current().toLocalFile());
    if (!image.isNull()) {
        QGuiApplication::clipboard()->setImage(image);
    }
}

// What Settings > Personalization > Background writes (settings/src/
// preferences.cpp): the shell watches the file and changes the desktop.
void Album::setAsWallpaper() const
{
    QString path = current().toLocalFile();
    if (path.isEmpty()) {
        return;
    }
    // The desktop's background reads what Qt reads: the others go as PNG.
    if (formats::isExtra(path)) {
        const QImage image = formats::read(path);
        const QString folder = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/wallpapers");
        QDir().mkpath(folder);
        const QString png = folder + QLatin1Char('/') + QFileInfo(path).completeBaseName() + QStringLiteral(".png");
        if (image.isNull() || !image.save(png)) {
            return;
        }
        path = png;
    }
    QSettings settings(QStringLiteral("Vela"), QStringLiteral("vela-shell"));
    QStringList recent = settings.value(QStringLiteral("appearance/recentWallpapers")).toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    settings.setValue(QStringLiteral("appearance/wallpaper"), path);
    settings.setValue(QStringLiteral("appearance/recentWallpapers"), recent.mid(0, 5));
}

void Album::print() const
{
    const QImage image = formats::read(current().toLocalFile());
    QTemporaryFile file(QDir::temp().filePath(QStringLiteral("vela-photos-XXXXXX.pdf")));
    file.setAutoRemove(false); // the print job removes it
    if (image.isNull() || !file.open()) {
        return;
    }
    {
        QPdfWriter pdf(&file);
        pdf.setPageSize(QPageSize(QPageSize::A4));
        pdf.setPageOrientation(image.width() > image.height() ? QPageLayout::Landscape : QPageLayout::Portrait);
        QPainter painter(&pdf);
        const QRect page = painter.viewport();
        const QSize fitted = image.size().scaled(page.size(), Qt::KeepAspectRatio);
        painter.drawImage(QRect(QPoint((page.width() - fitted.width()) / 2, (page.height() - fitted.height()) / 2), fitted), image);
    }
    file.close();
    PortalPrint::print(file.fileName(), name());
}

QString Album::filter() const
{
    return patterns().join(QLatin1Char(' '));
}

QUrl Album::shown(const QUrl& file) const
{
    return formats::shown(file);
}

void Album::extraDecoded(const QString& path, const QSize& size)
{
    if (m_sizes.value(path) == size) {
        return;
    }
    m_sizes.insert(path, size);
    if (m_index >= 0 && current().toLocalFile() == path) {
        emit indexChanged(); // the dimensions the page sizes the picture by
    }
}
