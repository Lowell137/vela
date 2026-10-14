// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "edits.h"

#include <QPainter>
#include <QPen>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

// Red, green and blue each from the three and a constant: every color change
// here is one of these, so they multiply into one and a pixel is visited once.
using Matrix = std::array<std::array<double, 4>, 3>;

const Matrix Identity { { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } } };

Matrix then(const Matrix& first, const Matrix& second)
{
    Matrix out {};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            out[i][j] = second[i][0] * first[0][j] + second[i][1] * first[1][j] + second[i][2] * first[2][j]
                + (j == 3 ? second[i][3] : 0);
        }
    }
    return out;
}

Matrix saturation(double amount) // 0: grey, 1: unchanged
{
    const double luma[3] = { 0.2126, 0.7152, 0.0722 };
    Matrix m {};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            m[i][j] = luma[j] * (1 - amount) + (i == j ? amount : 0);
        }
    }
    return m;
}

Matrix contrast(double factor) // around mid grey
{
    Matrix m = Identity;
    for (int i = 0; i < 3; ++i) {
        m[i][i] = factor;
        m[i][3] = 127.5 * (1 - factor);
    }
    return m;
}

Matrix scale(double red, double green, double blue)
{
    return { { { red, 0, 0, 0 }, { 0, green, 0, 0 }, { 0, 0, blue, 0 } } };
}

Matrix colorMatrix(const Edits& edits)
{
    Matrix m = Identity;
    if (edits.filter == QLatin1String("mono")) {
        m = saturation(0);
    } else if (edits.filter == QLatin1String("sepia")) {
        m = { { { 0.393, 0.769, 0.189, 0 }, { 0.349, 0.686, 0.168, 0 }, { 0.272, 0.534, 0.131, 0 } } };
    } else if (edits.filter == QLatin1String("warm")) {
        m = scale(1.10, 1.0, 0.88);
    } else if (edits.filter == QLatin1String("cool")) {
        m = scale(0.90, 1.0, 1.12);
    } else if (edits.filter == QLatin1String("vivid")) {
        m = then(saturation(1.4), contrast(1.15));
    }
    m = then(m, saturation(1 + edits.saturation));
    m = then(m, contrast(1 + edits.contrast));
    for (int i = 0; i < 3; ++i) {
        m[i][3] += edits.brightness * 96;
    }
    return m;
}

int clamped(double value)
{
    return int(std::clamp(value, 0.0, 255.0) + 0.5);
}

} // namespace

void edits::rotate(Edits& edits)
{
    edits.turn = (edits.turn + 90) % 360;
    std::swap(edits.flipH, edits.flipV); // a flip, then a quarter turn, is the turn and the other flip
    const QRectF c = edits.crop;
    edits.crop = QRectF(1 - c.y() - c.height(), c.x(), c.height(), c.width());
}

void edits::flip(Edits& edits, bool horizontal)
{
    QRectF& c = edits.crop;
    if (horizontal) {
        edits.flipH = !edits.flipH;
        c.moveLeft(1 - c.x() - c.width());
    } else {
        edits.flipV = !edits.flipV;
        c.moveTop(1 - c.y() - c.height());
    }
}

QSize edits::outputSize(QSize source, const Edits& edits, bool cropped)
{
    const QSize turned = edits.turn % 180 != 0 ? source.transposed() : source;
    if (!cropped) {
        return turned;
    }
    return QSize(std::max(1, qRound(turned.width() * edits.crop.width())),
        std::max(1, qRound(turned.height() * edits.crop.height())));
}

QTransform edits::toOutput(QSize source, const Edits& edits, QSizeF target, bool cropped)
{
    const QRectF c = cropped ? edits.crop : QRectF(0, 0, 1, 1);
    return QTransform::fromScale(1.0 / source.width(), 1.0 / source.height()) // to a unit square,
        * QTransform::fromTranslate(-0.5, -0.5) // turned and flipped around its middle,
        * QTransform().rotate(edits.turn)
        * QTransform::fromScale(edits.flipH ? -1 : 1, edits.flipV ? -1 : 1)
        * QTransform::fromTranslate(0.5, 0.5)
        * QTransform::fromTranslate(-c.x(), -c.y()) // of which the crop fills the target
        * QTransform::fromScale(target.width() / c.width(), target.height() / c.height());
}

QImage edits::recolor(const QImage& image, const Edits& edits)
{
    const Matrix m = colorMatrix(edits);
    if (m == Identity) {
        return image;
    }
    QImage out = image.convertToFormat(image.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32);
    for (int y = 0; y < out.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x) {
            const double r = qRed(line[x]), g = qGreen(line[x]), b = qBlue(line[x]);
            line[x] = qRgba(clamped(m[0][0] * r + m[0][1] * g + m[0][2] * b + m[0][3]),
                clamped(m[1][0] * r + m[1][1] * g + m[1][2] * b + m[1][3]),
                clamped(m[2][0] * r + m[2][1] * g + m[2][2] * b + m[2][3]),
                qAlpha(line[x]));
        }
    }
    return out;
}

void edits::paint(QPainter& painter, const QImage& colored, QSize source, const Edits& edits, QSizeF target, bool cropped)
{
    painter.save();
    painter.setClipRect(QRectF(QPointF(0, 0), target));
    painter.setRenderHints(QPainter::SmoothPixmapTransform | QPainter::Antialiasing);
    painter.setTransform(toOutput(source, edits, target, cropped), true);
    painter.drawImage(QRectF(0, 0, source.width(), source.height()), colored);
    const double longer = std::max(source.width(), source.height());
    for (const Stroke& stroke : edits.strokes) {
        painter.setPen(QPen(stroke.color, stroke.width * longer, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (stroke.points.size() == 1) {
            painter.drawPoint(stroke.points.first());
        } else {
            painter.drawPolyline(stroke.points.constData(), stroke.points.size());
        }
    }
    painter.restore();
}

QImage edits::render(const QImage& source, const Edits& edits)
{
    QImage out(outputSize(source.size(), edits),
        source.hasAlphaChannel() ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32);
    out.fill(Qt::transparent);
    QPainter painter(&out);
    paint(painter, recolor(source, edits), source.size(), edits, out.size());
    return out;
}
