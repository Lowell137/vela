// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "editview.h"

#include "formats.h"

#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <QSaveFile>

#include <algorithm>
#include <cmath>

namespace {

const int PreviewSide = 2000;

QImage read(const QUrl& url, QString* error = nullptr)
{
    return formats::read(url.toLocalFile(), {}, error);
}

} // namespace

void EditView::paint(QPainter* painter)
{
    if (m_preview.isNull()) {
        return;
    }
    if (m_colorsDirty) {
        m_colored = edits::recolor(m_preview, m_edits);
        m_colorsDirty = false;
    }
    edits::paint(*painter, m_colored, m_size, m_edits, size(), !m_cropping);
}

void EditView::touch(bool colors)
{
    m_colorsDirty = m_colorsDirty || colors;
    update();
    emit changed();
}

void EditView::setSource(const QUrl& source)
{
    m_source = source;
    m_saved.clear();
    m_edits = {};
    const QImage image = source.isEmpty() ? QImage() : read(source);
    m_size = image.size();
    m_preview = std::max(image.width(), image.height()) > PreviewSide
        ? image.scaled(PreviewSide, PreviewSide, Qt::KeepAspectRatio, Qt::SmoothTransformation)
        : image;
    touch(true);
}

void EditView::setCropping(bool cropping)
{
    if (cropping != m_cropping) {
        m_cropping = cropping;
        touch();
    }
}

void EditView::setCrop(const QRectF& crop)
{
    const QRectF bounded = crop.intersected(QRectF(0, 0, 1, 1));
    if (bounded != m_edits.crop && bounded.width() > 0 && bounded.height() > 0) {
        m_edits.crop = bounded;
        touch();
    }
}

void EditView::setBrightness(double value)
{
    m_edits.brightness = value;
    touch(true);
}

void EditView::setContrast(double value)
{
    m_edits.contrast = value;
    touch(true);
}

void EditView::setSaturation(double value)
{
    m_edits.saturation = value;
    touch(true);
}

void EditView::setFilter(const QString& filter)
{
    m_edits.filter = filter;
    touch(true);
}

void EditView::rotate()
{
    edits::rotate(m_edits);
    touch();
}

void EditView::flip(bool horizontal)
{
    edits::flip(m_edits, horizontal);
    touch();
}

void EditView::reset()
{
    m_edits = {};
    touch(true);
}

QPointF EditView::toImage(double u, double v) const
{
    return edits::toOutput(m_size, m_edits, QSizeF(1, 1), !m_cropping).inverted().map(QPointF(u, v));
}

void EditView::beginStroke(const QColor& color, double width, double u, double v)
{
    m_edits.strokes.append(Stroke { color, width, { toImage(u, v) } });
    touch();
}

void EditView::strokeTo(double u, double v)
{
    if (!m_edits.strokes.isEmpty()) {
        m_edits.strokes.last().points.append(toImage(u, v));
        touch();
    }
}

void EditView::eraseAt(double u, double v)
{
    const QPointF at = toImage(u, v);
    const double longer = std::max(m_size.width(), m_size.height());
    const qsizetype removed = m_edits.strokes.removeIf([&](const Stroke& stroke) {
        const double reach = (stroke.width / 2 + 0.01) * longer;
        return std::any_of(stroke.points.cbegin(), stroke.points.cend(), [&](const QPointF& point) {
            return std::hypot(point.x() - at.x(), point.y() - at.y()) <= reach;
        });
    });
    if (removed > 0) {
        touch();
    }
}

void EditView::undoStroke()
{
    if (!m_edits.strokes.isEmpty()) {
        m_edits.strokes.removeLast();
        touch();
    }
}

// shortcut: the copy has the pixels only, the camera's data (EXIF) isn't
// carried over; copy it with a metadata library if it's missed.
QString EditView::save(bool overwrite)
{
    QString error;
    const QImage image = read(m_source, &error);
    if (image.isNull()) {
        return error;
    }
    const QFileInfo original(m_source.toLocalFile());
    // A format Qt reads but can't write (a camera's raw, an icon) becomes a PNG.
    QByteArray format = original.suffix().toLower().toLatin1();
    const bool writable = QImageWriter::supportedImageFormats().contains(format);
    if (!writable) {
        format = "png";
    }
    const QString suffix = writable ? original.suffix() : QStringLiteral("png");
    QString path = original.filePath();
    for (int n = 1; !(overwrite && writable) && (n == 1 || QFileInfo::exists(path)); ++n) {
        path = QStringLiteral("%1/%2 (%3).%4").arg(original.path(), original.completeBaseName()).arg(n).arg(suffix);
    }

    QSaveFile file(path); // the original is replaced only by a whole new file
    if (!file.open(QIODevice::WriteOnly)) {
        return file.errorString();
    }
    QImageWriter writer(&file, format);
    writer.setQuality(92);
    if (!writer.write(edits::render(image, m_edits))) {
        return writer.errorString();
    }
    if (!file.commit()) {
        return file.errorString();
    }
    m_saved = QUrl::fromLocalFile(path);
    emit changed();
    return {};
}
