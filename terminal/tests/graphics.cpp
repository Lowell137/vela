// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "graphics.h"
#include "vt.h"

#include <QBuffer>
#include <QTemporaryDir>
#include <QTest>

namespace {
// A 16x32 PNG, as a program would send it, base64.
QByteArray pngBase64()
{
    QImage image(16, 32, QImage::Format_ARGB32);
    image.fill(Qt::red);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return png.toBase64();
}
}

class GraphicsTest : public QObject {
    Q_OBJECT

private slots:
    void splitsTextAndCommands()
    {
        Graphics graphics;
        const QList<Graphics::Piece> pieces = graphics.split("ab\x1b_Ga=T,f=100;QUJD\x1b\\cd");
        QCOMPARE(pieces.size(), 3);
        QCOMPARE(pieces[0].text, QByteArray("ab"));
        QVERIFY(pieces[1].command);
        QCOMPARE(pieces[1].keys.value("a"), QByteArray("T"));
        QCOMPARE(pieces[1].keys.value("f"), QByteArray("100"));
        QCOMPARE(pieces[1].payload, QByteArray("QUJD"));
        QCOMPARE(pieces[2].text, QByteArray("cd"));
    }

    void waitsForACommandCutShort()
    {
        Graphics graphics;
        const QList<Graphics::Piece> first = graphics.split("x\x1b_Ga=T;QU");
        QCOMPARE(first.size(), 1);
        QCOMPARE(first[0].text, QByteArray("x"));
        const QList<Graphics::Piece> second = graphics.split("JD\x1b\\y");
        QCOMPARE(second.size(), 2);
        QVERIFY(second[0].command);
        QCOMPARE(second[0].payload, QByteArray("QUJD"));
        QCOMPARE(second[1].text, QByteArray("y"));
    }

    void waitsForAnEscapeAtTheEnd()
    {
        Graphics graphics;
        QCOMPARE(graphics.split("x\x1b_").size(), 1);
        const QList<Graphics::Piece> rest = graphics.split("Ga=T;QUJD\x1b\\");
        QCOMPARE(rest.size(), 1);
        QVERIFY(rest[0].command);
    }

    void gathersChunks()
    {
        Graphics graphics;
        QCOMPARE(graphics.split("\x1b_Ga=T,f=100,m=1;QU\x1b\\").size(), 0);
        const QList<Graphics::Piece> pieces = graphics.split("\x1b_Gm=0;JD\x1b\\");
        QCOMPARE(pieces.size(), 1);
        QCOMPARE(pieces[0].keys.value("a"), QByteArray("T"));
        QCOMPARE(pieces[0].payload, QByteArray("QUJD"));
    }

    void decodesPngAndRaw()
    {
        const QImage png = Graphics::decode({ { "f", "100" } }, pngBase64());
        QCOMPARE(png.size(), QSize(16, 32));

        const QByteArray pixels(2 * 1 * 3, '\xff');
        const QImage raw = Graphics::decode({ { "f", "24" }, { "s", "2" }, { "v", "1" } }, pixels.toBase64());
        QCOMPARE(raw.size(), QSize(2, 1));

        QVERIFY(Graphics::decode({ { "f", "100" } }, "bm90IGFuIGltYWdl").isNull());
    }

    void readsAPictureFromAFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("logo.png"));
        QVERIFY(QImage(16, 32, QImage::Format_ARGB32).save(path, "PNG"));
        const QByteArray encoded = path.toUtf8().toBase64();
        QCOMPARE(Graphics::decode({ { "f", "100" }, { "t", "f" } }, encoded).size(), QSize(16, 32));
        // Raw pixels from a file are not read: any file would become noise on screen.
        QVERIFY(Graphics::decode({ { "f", "32" }, { "t", "f" }, { "s", "2" }, { "v", "2" } }, encoded).isNull());
        QVERIFY(Graphics::decode({ { "f", "100" }, { "t", "f" } }, QByteArray("/nonexistent/x.png").toBase64()).isNull());
    }

    void placesAPictureAtTheCursor()
    {
        Vt vt;
        vt.resize(10, 40);
        vt.setCellSize(QSizeF(8, 16));
        vt.input("ab");
        vt.input("\x1b_Ga=T,f=100;" + pngBase64() + "\x1b\\");
        QCOMPARE(vt.pictures().size(), 1);
        const Vt::Picture& picture = vt.pictures().first();
        QCOMPARE(picture.line, 0);
        QCOMPARE(picture.column, 2);
        QCOMPARE(picture.columns, 2); // 16 pixels in 8-wide cells
        QCOMPARE(picture.rows, 2);    // 32 pixels in 16-high cells
        // The cursor is on the line after the picture, at its first column.
        QCOMPARE(vt.cursor(), QPoint(0, 2));
    }

    void movesAPictureWithTheLinesAndDropsItPastTheHistory()
    {
        Vt vt;
        vt.resize(3, 40);
        vt.setCellSize(QSizeF(8, 16));
        vt.input("\x1b_Ga=T,f=100;" + pngBase64() + "\x1b\\");
        // The cursor is on the line after the picture (2 rows), so three line feeds scroll 3 lines.
        vt.input("\r\n\r\n\r\n");
        QCOMPARE(vt.history(), 3);
        QCOMPARE(vt.pictures().first().line, -3);
        // The history is kept for 10000 lines; past that, the picture is gone too.
        vt.input(QByteArray(10000, '\n'));
        QVERIFY(vt.pictures().isEmpty());
    }

    void keepsTheCursorWhenAskedTo()
    {
        Vt vt;
        vt.resize(10, 40);
        vt.setCellSize(QSizeF(8, 16));
        vt.input("\x1b_Ga=T,f=100,C=1;" + pngBase64() + "\x1b\\");
        QCOMPARE(vt.pictures().size(), 1);
        QCOMPARE(vt.cursor(), QPoint(0, 0));
    }
};

QTEST_GUILESS_MAIN(GraphicsTest)
#include "graphics.moc"
