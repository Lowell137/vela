// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The calculator (`Calc` in QML), like Windows'. Standard: each operator
// computes what's pending, with no precedence (2 + 3 × 4 is 20). Scientific:
// × before +, parentheses, and the functions.

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

class Engine : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString display READ display NOTIFY changed)
    Q_PROPERTY(QString expression READ expression NOTIFY changed) // the small line above
    Q_PROPERTY(bool error READ error NOTIFY changed)
    Q_PROPERTY(bool scientific READ scientific WRITE setScientific NOTIFY changed)
    Q_PROPERTY(QString angleUnit READ angleUnit WRITE setAngleUnit NOTIFY changed) // "DEG", "RAD", "GRAD"
    Q_PROPERTY(int openParentheses READ openParentheses NOTIFY changed)
    Q_PROPERTY(QStringList memoryList READ memoryList NOTIFY changed) // the stored values, newest first
    Q_PROPERTY(QVariantList history READ history NOTIFY changed) // newest first: {expression, result}

public:
    using QObject::QObject;

    QString display() const;
    QString expression() const;
    bool error() const { return !m_error.isEmpty(); }
    bool scientific() const { return m_scientific; }
    void setScientific(bool scientific);
    QString angleUnit() const { return m_angleUnit; }
    void setAngleUnit(const QString& unit);
    int openParentheses() const;
    QStringList memoryList() const;
    QVariantList history() const { return m_history; }

    // A typed character: digits, the decimal point, + - * / = %, and Windows'
    // letters (r: 1/x, q: x², @: √x; in Scientific also ( ) ^ ! and p: π,
    // s o t: sin cos tan, l: log, n: ln). False if it isn't a key.
    Q_INVOKABLE bool key(const QString& text);
    Q_INVOKABLE void digit(int digit);
    Q_INVOKABLE void point();
    Q_INVOKABLE void backspace();
    Q_INVOKABLE void clearEntry();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void negate();
    // "inv", "sqr", "cube", "sqrt", "cbrt", "abs", "fact", "log", "ln",
    // "exp10", "exp2", "expe", "sin", "cos", "tan", "asin", "acos", "atan"
    Q_INVOKABLE void function(const QString& name);
    Q_INVOKABLE void constant(const QString& name); // "pi", "e"
    Q_INVOKABLE void openParenthesis();
    Q_INVOKABLE void closeParenthesis();
    Q_INVOKABLE void percent();
    Q_INVOKABLE void operation(const QString& op); // "+", "−", "×", "÷", "mod", "^", "yroot"
    Q_INVOKABLE void equals();
    // "MS" stores another value. "MR", "M+", "M-": on the newest, or on the
    // one at `index` of the list. "MC": all of them, or that one.
    Q_INVOKABLE void memory(const QString& key, int index = -1);
    Q_INVOKABLE void recall(int index); // a history entry's result
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void copy() const;
    Q_INVOKABLE void paste();

signals:
    void changed();

private:
    // What waits for its right side: an operator with its left side, or an
    // open parenthesis ("("). Standard only ever has one.
    struct Frame {
        double lhs;
        QString op;
        qsizetype text; // of "(": where it starts in m_text
    };

    double current() const;
    QString operandText() const;
    void setValue(double value, const QString& text = {});
    void operandChanged();
    void fail(const QString& message);
    bool recover();
    bool apply(double lhs, const QString& op, double rhs, double* result);
    const Frame* pendingOperator() const;

    QString m_entry; // the number being typed ("12.", "-0.5"); empty when the operand is m_value
    double m_value = 0;
    QString m_operandText; // how the operand reads above, if a function made it: "√(9)"
    QList<Frame> m_frames;
    QString m_text; // what's above so far, without the operand: "2 + (3 × "
    bool m_fresh = false; // an operand was given since the operator
    bool m_scientific = false;
    QString m_angleUnit = QStringLiteral("DEG");
    QString m_lastOp; // = again repeats it
    double m_lastRhs = 0;
    QString m_expression; // of the last =
    QString m_error;
    QList<double> m_memory;
    QVariantList m_history;
};
