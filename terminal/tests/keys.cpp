// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "terminal.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QSignalSpy>
#include <QTest>

// The item with its keys within reach: its pty is a real one.
struct Keyed : Terminal {
    using Terminal::keyPressEvent;
    void press(int key, Qt::KeyboardModifiers modifiers, const QString& text)
    {
        QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
        keyPressEvent(&event);
    }
    // What the screen shows now, through the clipboard.
    bool shows(const QString& text)
    {
        selectAll();
        copy();
        return QGuiApplication::clipboard()->text().contains(text);
    }
};

class KeysTest : public QObject {
    Q_OBJECT

private slots:
    void ctrlCCopiesWhatIsSelectedAndInterruptsOtherwise()
    {
        Keyed terminal;
        QSignalSpy finished(&terminal, &Terminal::finished);
        terminal.start(QStringLiteral("sh -c \"printf hello; sleep 30\""), QStringLiteral("/"));
        QTRY_VERIFY(terminal.shows(QStringLiteral("hello")));

        QGuiApplication::clipboard()->clear();
        terminal.selectAll();
        terminal.press(Qt::Key_C, Qt::ControlModifier, QStringLiteral("\x03"));
        QVERIFY(QGuiApplication::clipboard()->text().contains(QLatin1String("hello")));
        QVERIFY(!terminal.hasSelection()); // copying let go of it
        QTest::qWait(300);
        QCOMPARE(finished.count(), 0); // and didn't interrupt the program

        // Nothing selected: the same keys are the interrupt.
        terminal.press(Qt::Key_C, Qt::ControlModifier, QStringLiteral("\x03"));
        QTRY_COMPARE(finished.count(), 1);
    }

    void ctrlVPastes()
    {
        Keyed terminal;
        terminal.start(QStringLiteral("sh -c \"read x; echo got:$x; sleep 30\""), QStringLiteral("/"));
        QGuiApplication::clipboard()->setText(QStringLiteral("typed"));
        terminal.press(Qt::Key_V, Qt::ControlModifier, QStringLiteral("\x16"));
        terminal.press(Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
        QTRY_VERIFY(terminal.shows(QStringLiteral("got:typed")));
    }
};

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    KeysTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "keys.moc"
