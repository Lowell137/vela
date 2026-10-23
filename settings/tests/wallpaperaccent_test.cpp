// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later
#include "wallpaperaccent.h"

#include <QPainter>
#include <QTest>

class WallpaperAccentTest : public QObject {
    Q_OBJECT
private slots:
    void picksTheDominantHue()
    {
        QImage image(200, 100, QImage::Format_RGB32);
        image.fill(QColor(30, 60, 220)); // mostly blue
        QPainter(&image).fillRect(0, 0, 40, 100, QColor(220, 40, 40)); // some red
        const QColor accent = vela::accentFromImage(image);
        QVERIFY(accent.isValid());
        QVERIFY(accent.hue() > 200 && accent.hue() < 250);
        QVERIFY(accent.hsvSaturationF() >= 0.55f);
    }
    void grayHasNoAccent()
    {
        QImage image(50, 50, QImage::Format_RGB32);
        image.fill(QColor(128, 128, 128));
        QVERIFY(!vela::accentFromImage(image).isValid());
    }
    void missingFileHasNoAccent()
    {
        QVERIFY(!vela::accentFromWallpaper(QStringLiteral("/nonexistent.png")).isValid());
    }
};

QTEST_MAIN(WallpaperAccentTest)
#include "wallpaperaccent_test.moc"
