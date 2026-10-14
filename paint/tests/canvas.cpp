// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// What Paint's tools leave on the picture.

#include "canvas.h"

#include <QtTest>

class CanvasTest : public QObject {
    Q_OBJECT

    static void drag(PaintCanvas& canvas, QPointF from, QPointF to)
    {
        canvas.press(from.x(), from.y());
        canvas.move(to.x(), to.y());
        canvas.release();
    }

private slots:
    void fillStopsAtLines()
    {
        PaintCanvas canvas;
        canvas.newImage(40, 40);
        canvas.setProperty("size", 1);
        canvas.setTool("rectangle");
        drag(canvas, { 10, 10 }, { 30, 30 });
        canvas.setTool("fill");
        canvas.setProperty("color", QColor(Qt::red));
        canvas.press(20, 20);
        QCOMPARE(canvas.image().pixelColor(20, 20), QColor(Qt::red));
        QCOMPARE(canvas.image().pixelColor(2, 2), QColor(Qt::white)); // outside the rectangle
        QCOMPARE(canvas.image().pixelColor(28, 12), QColor(Qt::red)); // its far corner
    }

    void undoAndRedo()
    {
        PaintCanvas canvas;
        canvas.newImage(20, 20);
        QVERIFY(!canvas.modified());
        drag(canvas, { 5, 5 }, { 15, 5 });
        QCOMPARE(canvas.image().pixelColor(10, 5), QColor(Qt::black));
        QVERIFY(canvas.modified());
        canvas.undo();
        QCOMPARE(canvas.image().pixelColor(10, 5), QColor(Qt::white));
        QVERIFY(!canvas.canUndo());
        canvas.redo();
        QCOMPARE(canvas.image().pixelColor(10, 5), QColor(Qt::black));
    }

    void rightButtonDrawsWithTheSecondColor()
    {
        PaintCanvas canvas;
        canvas.newImage(20, 20);
        canvas.setProperty("color2", QColor(Qt::blue));
        canvas.press(5, 5, true);
        canvas.move(15, 5);
        canvas.release();
        QCOMPARE(canvas.image().pixelColor(10, 5), QColor(Qt::blue));
    }

    void movesTheSelection()
    {
        PaintCanvas canvas;
        canvas.newImage(40, 40);
        canvas.setProperty("size", 4);
        canvas.press(10, 10); // a dot
        canvas.release();
        canvas.setTool("select");
        drag(canvas, { 5, 5 }, { 15, 15 });
        QCOMPARE(canvas.selection(), QRect(5, 5, 10, 10));
        drag(canvas, { 10, 10 }, { 30, 30 }); // inside it: moved by 20
        canvas.deselect();
        QCOMPARE(canvas.image().pixelColor(30, 30), QColor(Qt::black));
        QCOMPARE(canvas.image().pixelColor(10, 10), QColor(Qt::white));
        canvas.undo(); // the move, in one step
        QCOMPARE(canvas.image().pixelColor(10, 10), QColor(Qt::black));
    }

    void freeFormSelectionCarriesOnlyItsInside()
    {
        PaintCanvas canvas;
        canvas.newImage(60, 60);
        canvas.setTool("fill");
        canvas.setProperty("color", QColor(Qt::red));
        canvas.press(1, 1); // all red
        canvas.setProperty("color2", QColor(Qt::white));
        canvas.setTool("lasso");
        canvas.press(10, 10); // a triangle: (10,10) (30,10) (10,30)
        canvas.move(30, 10);
        canvas.move(10, 30);
        canvas.release();
        QCOMPARE(canvas.selection(), QRect(10, 10, 20, 20));
        drag(canvas, { 13, 13 }, { 43, 13 }); // moved 30 to the right
        canvas.deselect();
        QCOMPARE(canvas.image().pixelColor(13, 13), QColor(Qt::white)); // left behind
        QCOMPARE(canvas.image().pixelColor(27, 27), QColor(Qt::red)); // in the rectangle, outside the triangle
    }

    void transparentSelectionLeavesTheBackground()
    {
        PaintCanvas canvas;
        canvas.newImage(60, 20);
        canvas.setProperty("size", 4);
        canvas.press(5, 5); // a dot to carry
        canvas.release();
        canvas.setProperty("transparentSelection", true);
        canvas.setTool("select");
        drag(canvas, { 0, 0 }, { 10, 10 });
        canvas.stretchSelection(QRect(20, 0, 20, 20)); // moved and doubled
        canvas.deselect();
        QCOMPARE(canvas.image().pixelColor(30, 10), QColor(Qt::black)); // the dot, twice as large
        QCOMPARE(canvas.imageSize(), QSize(60, 20));
    }

    void polygonAndShift()
    {
        PaintCanvas canvas;
        canvas.newImage(60, 60);
        canvas.setProperty("size", 1);
        canvas.setProperty("filled", true);
        canvas.setProperty("color2", QColor(Qt::blue));
        canvas.setTool("polygon");
        drag(canvas, { 10, 10 }, { 50, 10 });
        canvas.press(50, 50);
        canvas.release();
        canvas.finishShape();
        QCOMPARE(canvas.image().pixelColor(45, 20), QColor(Qt::blue)); // inside the triangle
        QCOMPARE(canvas.image().pixelColor(15, 45), QColor(Qt::white));

        canvas.setProperty("filled", false);
        canvas.setTool("line");
        canvas.press(5, 55);
        canvas.move(55, 57, true); // Shift: level
        canvas.release();
        QVERIFY(canvas.image().pixelColor(40, 55) != QColor(Qt::white)); // softened: not pure black
        QCOMPARE(canvas.image().pixelColor(40, 57), QColor(Qt::white)); // where the pointer went
    }

    void cropsAndResizes()
    {
        PaintCanvas canvas;
        canvas.newImage(40, 40);
        canvas.setTool("select");
        drag(canvas, { 10, 10 }, { 30, 20 });
        canvas.crop();
        QCOMPARE(canvas.imageSize(), QSize(20, 10));
        canvas.setProperty("color2", QColor(Qt::green));
        canvas.resize(30, 10, false); // extended with the second color
        QCOMPARE(canvas.image().pixelColor(25, 5), QColor(Qt::green));
        canvas.rotate();
        QCOMPARE(canvas.imageSize(), QSize(10, 30));
    }
};

QTEST_MAIN(CanvasTest)
#include "canvas.moc"
