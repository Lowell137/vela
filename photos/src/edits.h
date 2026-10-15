// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// What Edit does to an image, kept as a description until it's saved: the
// screen and the saved file are drawn by the same code, from a smaller copy
// and from the whole image.

#include <QColor>
#include <QImage>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QTransform>

class QPainter;

struct Stroke {
    QColor color;
    double width = 0; // of the image's longer side
    QList<QPointF> points; // in the image's own pixels: turning and cropping don't move them
};

struct Edits {
    int turn = 0; // degrees clockwise, then the flips
    bool flipH = false;
    bool flipV = false;
    QRectF crop { 0, 0, 1, 1 }; // of the turned image, 0 to 1
    double brightness = 0; // these three: -1 to 1
    double contrast = 0;
    double saturation = 0;
    QString filter; // "", "mono", "sepia", "warm", "cool", "vivid"
    QList<Stroke> strokes;
};

namespace edits {

// A quarter turn clockwise of what's shown, and a flip of it: the crop follows.
void rotate(Edits& edits);
void flip(Edits& edits, bool horizontal);

QSize outputSize(QSize source, const Edits& edits, bool cropped = true);
// From the image's pixels to a `target` wide and tall drawing of the result.
QTransform toOutput(QSize source, const Edits& edits, QSizeF target, bool cropped = true);
// The colors only: brightness, contrast, saturation and the filter.
QImage recolor(const QImage& image, const Edits& edits);
// `colored` may be a smaller copy of the `source` sized image.
void paint(QPainter& painter, const QImage& colored, QSize source, const Edits& edits, QSizeF target, bool cropped = true);
QImage render(const QImage& source, const Edits& edits);

} // namespace edits
