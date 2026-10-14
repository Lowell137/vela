// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// Tests of the flag handing a command to a terminal (terminalexec.h).
//
//   ctest --test-dir build -R terminalexec

#include "terminalexec.h"

#include <QTest>

class TerminalExecTest : public QObject {
    Q_OBJECT

private slots:
    void flags_data()
    {
        QTest::addColumn<QString>("program");
        QTest::addColumn<QStringList>("expected");
        QTest::newRow("konsole") << "konsole" << QStringList { "konsole", "-e" };
        QTest::newRow("kitty") << "kitty" << QStringList { "kitty" };
        QTest::newRow("kitty path") << "/usr/bin/kitty" << QStringList { "/usr/bin/kitty" };
        QTest::newRow("ghostty") << "ghostty" << QStringList { "ghostty", "-e" };
        QTest::newRow("ptyxis") << "ptyxis" << QStringList { "ptyxis", "-x" };
        QTest::newRow("gnome-terminal") << "gnome-terminal" << QStringList { "gnome-terminal", "--" };
        QTest::newRow("wezterm") << "wezterm" << QStringList { "wezterm", "start", "--" };
        QTest::newRow("foot") << "foot" << QStringList { "foot", "-e" };
        QTest::newRow("vela-terminal") << "vela-terminal" << QStringList { "vela-terminal", "-e" };
        QTest::newRow("unknown") << "mystery-term" << QStringList { "mystery-term", "-e" };
    }

    void flags()
    {
        QFETCH(QString, program);
        QFETCH(QStringList, expected);
        QCOMPARE(vela::terminalCommand(program), expected);
    }
};

QTEST_APPLESS_MAIN(TerminalExecTest)
#include "terminalexec.moc"
