// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "vt.h"

#include <QTest>

class VtTest : public QObject {
    Q_OBJECT

private slots:
    void showsWhatIsWritten()
    {
        Vt vt;
        vt.resize(5, 20);
        vt.input("hello\r\nw\xc3\xb6rld");
        QCOMPARE(vt.text(QPoint(0, 0), QPoint(19, 1)), QStringLiteral("hello\nwörld"));
        QCOMPARE(vt.cursor(), QPoint(5, 1));
    }

    void keepsLinesThatLeave()
    {
        Vt vt;
        vt.resize(3, 20);
        vt.input("one\r\ntwo\r\nthree\r\nfour\r\nfive");
        QCOMPARE(vt.history(), 2);
        QCOMPARE(vt.text(QPoint(0, -2), QPoint(19, -1)), QStringLiteral("one\ntwo"));
        QCOMPARE(vt.text(QPoint(0, 0), QPoint(19, 2)), QStringLiteral("three\nfour\nfive"));
        // A piece of a line, from a column to another.
        QCOMPARE(vt.text(QPoint(1, 0), QPoint(3, 0)), QStringLiteral("hre"));
    }

    void colorsFollowTheScheme()
    {
        Vt vt;
        vt.setColors(Qt::white, Qt::black, { QColor("#000000"), QColor("#c50f1f") });
        vt.input("\x1b[31mred");
        const VTermScreenCell cell = vt.cell(0, 0);
        QVERIFY(!VTERM_COLOR_IS_DEFAULT_FG(&cell.fg));
        QCOMPARE(vt.color(cell.fg), QColor("#c50f1f"));
        QVERIFY(VTERM_COLOR_IS_DEFAULT_BG(&cell.bg));
    }

    void keysReachTheProgram()
    {
        Vt vt;
        QByteArray sent;
        connect(&vt, &Vt::output, this, [&sent](const QByteArray& bytes) { sent += bytes; });
        vt.character('a', VTERM_MOD_NONE);
        vt.character('c', VTERM_MOD_CTRL);
        vt.key(VTERM_KEY_ENTER, VTERM_MOD_NONE);
        vt.key(VTERM_KEY_UP, VTERM_MOD_NONE);
        QCOMPARE(sent, QByteArray("a\x03\r\x1b[A"));
        sent.clear();
        vt.paste(QStringLiteral("ls\necho"));
        QCOMPARE(sent, QByteArray("ls\recho"));
        // A program that asks gets the paste between brackets.
        sent.clear();
        vt.input("\x1b[?2004h");
        vt.paste(QStringLiteral("x"));
        QCOMPARE(sent, QByteArray("\x1b[200~x\x1b[201~"));
    }

    void titleAndMouseAreToldApart()
    {
        Vt vt;
        QString title;
        connect(&vt, &Vt::titleChanged, this, [&title](const QString& text) { title = text; });
        vt.input("\x1b]0;~/Projects\x07");
        QCOMPARE(title, QStringLiteral("~/Projects"));
        QCOMPARE(vt.mouse(), 0);
        vt.input("\x1b[?1000h\x1b[?1049h");
        QVERIFY(vt.mouse() != 0);
        QVERIFY(vt.altScreen());
    }
};

QTEST_GUILESS_MAIN(VtTest)
#include "vt.moc"
