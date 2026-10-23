// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QImage>
#include <QImageReader>
#include <QString>
#include <algorithm>
#include <array>

namespace vela {

// The accent a wallpaper suggests: the hue most of its colorful pixels agree
// on, made vivid enough to read on both light and dark surfaces. Invalid when
// the image can't be read or has no color (a gray wallpaper).
inline QColor accentFromImage(const QImage& source)
{
    if (source.isNull()) {
        return {};
    }
    const QImage image = source.scaled(64, 64, Qt::KeepAspectRatio, Qt::FastTransformation)
                             .convertToFormat(QImage::Format_RGB32);
    constexpr int bins = 24;
    std::array<double, bins> weight {};
    std::array<double, bins> sumS {}, sumV {};
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor c = image.pixelColor(x, y);
            const float s = c.hsvSaturationF();
            const float v = c.valueF();
            if (s < 0.25f || v < 0.25f) {
                continue; // grays, near-black and near-white say nothing
            }
            const int bin = std::clamp(int(c.hsvHueF() * bins), 0, bins - 1);
            const double w = double(s) * v;
            weight[bin] += w;
            sumS[bin] += w * s;
            sumV[bin] += w * v;
        }
    }
    const int best = int(std::max_element(weight.begin(), weight.end()) - weight.begin());
    if (weight[best] <= 0) {
        return {};
    }
    const float s = std::clamp(float(sumS[best] / weight[best]), 0.55f, 1.0f);
    const float v = std::clamp(float(sumV[best] / weight[best]), 0.6f, 0.9f);
    return QColor::fromHsvF((best + 0.5f) / bins, s, v);
}

inline QColor accentFromWallpaper(const QString& path)
{
    QImageReader reader(path);
    reader.setScaledSize(QSize(64, 64)); // vector images render at this size
    return accentFromImage(reader.read());
}

} // namespace vela
