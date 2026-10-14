// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// What Edit writes: where the pixels go when the image is turned, flipped and
// cropped, and what the filters do to them.

#include "edits.h"

#include <QtTest>

class EditsTest : public QObject {
    Q_OBJECT

    // 4×2, white, its top left pixel red.
    static QImage sample()
    {
        QImage image(4, 2, QImage::Format_RGB32);
        image.fill(Qt::white);
        image.setPixelColor(0, 0, Qt::red);
        return image;
    }
    static bool red(const QImage& image, int x, int y)
    {
        const QColor c = image.pixelColor(x, y);
        return c.red() > 200 && c.green() < 60 && c.blue() < 60;
    }

private slots:
    void nothing()
    {
        const QImage out = edits::render(sample(), {});
        QCOMPARE(out.size(), QSize(4, 2));
        QVERIFY(red(out, 0, 0));
        QCOMPARE(out.pixelColor(3, 1), QColor(Qt::white));
    }

    void turnsAndFlips()
    {
        Edits e;
        edits::rotate(e); // clockwise: the top left goes to the top right
        QImage out = edits::render(sample(), e);
        QCOMPARE(out.size(), QSize(2, 4));
        QVERIFY(red(out, 1, 0));
        edits::flip(e, true);
        out = edits::render(sample(), e);
        QVERIFY(red(out, 0, 0));
        edits::rotate(e); // the flip stays what it was on screen
        out = edits::render(sample(), e);
        QCOMPARE(out.size(), QSize(4, 2));
        QVERIFY(red(out, 3, 0));
    }

    void cropFollowsTheTurn()
    {
        Edits e;
        e.crop = QRectF(0, 0, 0.5, 1); // the left half, with the red pixel
        QCOMPARE(edits::render(sample(), e).size(), QSize(2, 2));
        edits::rotate(e);
        const QImage out = edits::render(sample(), e);
        QCOMPARE(out.size(), QSize(2, 2));
        QVERIFY(red(out, 1, 0));
        edits::flip(e, false);
        QVERIFY(red(edits::render(sample(), e), 1, 1));
    }

    void colors()
    {
        Edits e;
        e.filter = QStringLiteral("mono");
        QColor c = edits::render(sample(), e).pixelColor(0, 0);
        QCOMPARE(c.red(), c.green());
        QCOMPARE(c.green(), c.blue());
        e = {};
        e.brightness = -1;
        c = edits::render(sample(), e).pixelColor(3, 1);
        QVERIFY(c.red() < 255 && c.red() == c.blue());
    }

    void strokesStayOnTheImage()
    {
        Edits e;
        e.strokes.append(Stroke { Qt::blue, 0.5, { QPointF(3.5, 1.5) } }); // on the bottom right pixel
        edits::rotate(e); // which a quarter turn takes to the bottom left
        const QColor c = edits::render(sample(), e).pixelColor(0, 3);
        QVERIFY(c.blue() > 200 && c.red() < 60);
    }
};

QTEST_GUILESS_MAIN(EditsTest)
#include "edits.moc"
