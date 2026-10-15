// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// What the keys compute, typed as on the keyboard (r: 1/x, q: x², @: √x).

#include "engine.h"

#include <QLocale>
#include <QtTest>

class EngineTest : public QObject {
    Q_OBJECT

    static void type(Engine& calc, const QString& keys)
    {
        for (const QChar key : keys) {
            QVERIFY2(calc.key(key), qPrintable(key));
        }
    }

private slots:
    void initTestCase() { QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates)); }

    void keys_data()
    {
        QTest::addColumn<QString>("keys");
        QTest::addColumn<QString>("display");
        QTest::addColumn<QString>("expression");

        QTest::newRow("add") << "2+3=" << "5" << "2 + 3 =";
        QTest::newRow("no precedence") << "2+3*4=" << "20" << "5 × 4 =";
        QTest::newRow("operator changed") << "2+*4=" << "8" << "2 × 4 =";
        QTest::newRow("no right side") << "5+=" << "10" << "5 + 5 =";
        QTest::newRow("equals again") << "5+3==" << "11" << "8 + 3 =";
        QTest::newRow("new left side") << "5+3=4=" << "7" << "4 + 3 =";
        QTest::newRow("decimal") << "0.1+0.2=" << "0.3" << "0.1 + 0.2 =";
        QTest::newRow("typing") << "007.50" << "7.50" << "";
        QTest::newRow("grouping") << "1234567.5" << "1,234,567.5" << "";
        QTest::newRow("square root") << "9@" << "3" << "√(9)";
        QTest::newRow("square") << "4q" << "16" << "sqr(4)";
        QTest::newRow("reciprocal") << "4r" << "0.25" << "1/(4)";
        QTest::newRow("function as right side") << "1+9@=" << "4" << "1 + √(9) =";
        QTest::newRow("percent of the left side") << "200+10%=" << "220" << "200 + 20 =";
        QTest::newRow("percent when multiplying") << "200*10%=" << "20" << "200 × 0.1 =";
        QTest::newRow("divide by zero") << "5/0=" << "Cannot divide by zero" << "5 ÷";
        QTest::newRow("after an error") << "5/0=7" << "7" << "";
        QTest::newRow("root of a negative") << "0-9=@" << "Invalid input" << "0 − 9 =";
    }
    void keys()
    {
        QFETCH(QString, keys);
        Engine calc;
        type(calc, keys);
        QTEST(calc.display(), "display");
        QTEST(calc.expression(), "expression");
    }

    void scientific_data()
    {
        QTest::addColumn<QString>("keys");
        QTest::addColumn<QString>("display");
        QTest::addColumn<QString>("expression");

        QTest::newRow("precedence") << "2+3*4=" << "14" << "2 + 3 × 4 =";
        QTest::newRow("parentheses") << "(2+3)*4=" << "20" << "(2 + 3) × 4 =";
        QTest::newRow("left open") << "2*(3+4=" << "14" << "2 × (3 + 4) =";
        QTest::newRow("nested") << "((1+2)*(3+4))=" << "21" << "((1 + 2) × (3 + 4)) =";
        QTest::newRow("power first") << "2*3^2=" << "18" << "2 × 3 ^ 2 =";
        QTest::newRow("operator changed") << "2+3*+4=" << "9" << "2 + 3 + 4 =";
        QTest::newRow("while typing") << "2+3*" << "3" << "2 + 3 ×";
        QTest::newRow("sine in degrees") << "30s" << "0.5" << "sin(30)";
        QTest::newRow("sine of half a turn") << "180s" << "0" << "sin(180)";
        QTest::newRow("tangent of a quarter") << "90t" << "Invalid input" << "";
        QTest::newRow("factorial") << "5!" << "120" << "fact(5)";
        QTest::newRow("logarithm") << "1000l" << "3" << "log(1,000)";
        QTest::newRow("modulo") << "17%5=" << "2" << "17 mod 5 =";
        QTest::newRow("function of parentheses") << "(7+9)@=" << "4" << "√((7 + 9)) =";
    }
    void scientific()
    {
        QFETCH(QString, keys);
        Engine calc;
        calc.setScientific(true);
        type(calc, keys);
        QTEST(calc.display(), "display");
        QTEST(calc.expression(), "expression");
    }

    void editing()
    {
        Engine calc;
        type(calc, "12.5");
        calc.negate();
        QCOMPARE(calc.display(), "-12.5");
        calc.backspace();
        calc.backspace();
        calc.backspace();
        calc.backspace();
        QCOMPARE(calc.display(), "0");
        type(calc, "8+3");
        calc.clearEntry();
        type(calc, "2=");
        QCOMPARE(calc.display(), "10");
        calc.clear();
        type(calc, "=");
        QCOMPARE(calc.display(), "0"); // C forgot the + 2
    }

    void memoryAndHistory()
    {
        Engine calc;
        type(calc, "5");
        calc.memory("MS");
        type(calc, "3");
        calc.memory("M+");
        QCOMPARE(calc.display(), "3");
        type(calc, "2*");
        calc.memory("MR");
        type(calc, "=");
        QCOMPARE(calc.display(), "16");
        type(calc, "1+1=");
        QCOMPARE(calc.history().size(), 2);
        calc.recall(1);
        QCOMPARE(calc.display(), "16");
        calc.memory("MS"); // a second value: the keys work on it
        calc.memory("M-");
        QCOMPARE(calc.memoryList(), (QStringList { "0", "8" }));
        calc.memory("MR", 1);
        QCOMPARE(calc.display(), "8");
        calc.memory("MC", 0);
        QCOMPARE(calc.memoryList(), QStringList { "8" });
        calc.memory("MC");
        QVERIFY(calc.memoryList().isEmpty());
    }
};

QTEST_MAIN(EngineTest)
#include "engine.moc"
