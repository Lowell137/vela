// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The image being edited (`EditView` in QML): it draws the result of the
// edits so far, and writes it to a file when asked.

#include "edits.h"

#include <QQuickPaintedItem>
#include <QUrl>

class EditView : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY changed)
    // While cropping the whole image is shown, the frame is QML's.
    Q_PROPERTY(bool cropping READ cropping WRITE setCropping NOTIFY changed)
    Q_PROPERTY(QRectF crop READ crop WRITE setCrop NOTIFY changed)
    Q_PROPERTY(QSize imageSize READ imageSize NOTIFY changed) // of what's drawn, in the image's pixels
    Q_PROPERTY(double brightness READ brightness WRITE setBrightness NOTIFY changed)
    Q_PROPERTY(double contrast READ contrast WRITE setContrast NOTIFY changed)
    Q_PROPERTY(double saturation READ saturation WRITE setSaturation NOTIFY changed)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY changed)
    Q_PROPERTY(bool hasStrokes READ hasStrokes NOTIFY changed)
    Q_PROPERTY(QUrl saved READ saved NOTIFY changed) // the file the last save wrote

public:
    using QQuickPaintedItem::QQuickPaintedItem;

    void paint(QPainter* painter) override;

    QUrl source() const { return m_source; }
    void setSource(const QUrl& source);
    bool cropping() const { return m_cropping; }
    void setCropping(bool cropping);
    QRectF crop() const { return m_edits.crop; }
    void setCrop(const QRectF& crop);
    QSize imageSize() const { return edits::outputSize(m_size, m_edits, !m_cropping); }
    double brightness() const { return m_edits.brightness; }
    void setBrightness(double value);
    double contrast() const { return m_edits.contrast; }
    void setContrast(double value);
    double saturation() const { return m_edits.saturation; }
    void setSaturation(double value);
    QString filter() const { return m_edits.filter; }
    void setFilter(const QString& filter);
    bool hasStrokes() const { return !m_edits.strokes.isEmpty(); }
    QUrl saved() const { return m_saved; }

    Q_INVOKABLE void rotate();
    Q_INVOKABLE void flip(bool horizontal);
    Q_INVOKABLE void reset();
    // Drawing: (u, v) is a point of this item, 0 to 1.
    Q_INVOKABLE void beginStroke(const QColor& color, double width, double u, double v);
    Q_INVOKABLE void strokeTo(double u, double v);
    Q_INVOKABLE void eraseAt(double u, double v); // the strokes under it
    Q_INVOKABLE void undoStroke();
    // Beside the original as "name (1).jpg", or over it. The error, empty if
    // it was written.
    Q_INVOKABLE QString save(bool overwrite);

signals:
    void changed();

private:
    QPointF toImage(double u, double v) const;
    void touch(bool colors = false);

    QUrl m_source;
    QUrl m_saved;
    QSize m_size; // the whole image's
    QImage m_preview; // a copy small enough to redraw at every change
    QImage m_colored; // m_preview with the colors applied
    bool m_colorsDirty = true;
    bool m_cropping = false;
    Edits m_edits;
};
