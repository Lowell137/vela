// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "engine.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QHash>
#include <QLocale>
#include <QVariantMap>

#include <algorithm>
#include <cmath>

namespace {

const int MaxDigits = 16; // typed, like Windows

// shortcut: doubles shown to 15 digits (0.1 + 0.2 reads 0.3), Windows keeps
// exact fractions; change the type here if results past 15 digits matter.
QString format(double value)
{
    return QString::number(value == 0 ? 0 : value, 'g', 15);
}

// "-1234567.5" as the locale writes it: "-1,234,567.5".
QString shown(const QString& raw)
{
    const QLocale locale;
    if (raw.contains(QLatin1Char('e'))) {
        return QString(raw).replace(QLatin1Char('.'), locale.decimalPoint());
    }
    const bool negative = raw.startsWith(QLatin1Char('-'));
    const int dot = raw.indexOf(QLatin1Char('.'));
    QString whole = raw.mid(negative ? 1 : 0, dot < 0 ? -1 : dot - (negative ? 1 : 0));
    for (int i = whole.size() - 3; i > 0; i -= 3) {
        whole.insert(i, locale.groupSeparator());
    }
    return (negative ? QStringLiteral("-") : QString()) + whole
        + (dot < 0 ? QString() : locale.decimalPoint() + raw.mid(dot + 1));
}

int precedence(const QString& op)
{
    return op == QStringLiteral("+") || op == QStringLiteral("−") ? 1
        : op == QStringLiteral("^") || op == QStringLiteral("yroot") ? 3
        : 2;
}

const QString Open = QStringLiteral("(");

} // namespace

QString Engine::display() const
{
    return error() ? m_error : shown(m_entry.isEmpty() ? format(m_value) : m_entry);
}

QString Engine::expression() const
{
    if (!m_frames.isEmpty()) {
        return (m_text + m_operandText).trimmed();
    }
    return m_operandText.isEmpty() ? m_expression : m_operandText;
}

int Engine::openParentheses() const
{
    return std::count_if(m_frames.cbegin(), m_frames.cend(), [](const Frame& frame) { return frame.op == Open; });
}

QStringList Engine::memoryList() const
{
    QStringList list;
    for (const double value : m_memory) {
        list.append(shown(format(value)));
    }
    return list;
}

void Engine::setScientific(bool scientific)
{
    if (scientific != m_scientific) {
        m_scientific = scientific;
        clear(); // what's pending was pending by the other mode's rules
    }
}

void Engine::setAngleUnit(const QString& unit)
{
    m_angleUnit = unit;
    emit changed();
}

double Engine::current() const
{
    return m_entry.isEmpty() ? m_value : m_entry.toDouble();
}

QString Engine::operandText() const
{
    return m_operandText.isEmpty() ? shown(format(current())) : m_operandText;
}

// The operator waiting for its right side, if the innermost thing waiting
// is one.
const Engine::Frame* Engine::pendingOperator() const
{
    return m_frames.isEmpty() || m_frames.last().op == Open ? nullptr : &m_frames.last();
}

void Engine::operandChanged()
{
    m_fresh = true;
    if (m_frames.isEmpty()) {
        m_expression.clear(); // the last = is over
    }
}

void Engine::setValue(double value, const QString& text)
{
    if (!std::isfinite(value)) {
        fail(tr("Overflow"));
        return;
    }
    m_entry.clear();
    m_value = value;
    m_operandText = text;
    operandChanged();
}

void Engine::fail(const QString& message)
{
    m_error = message;
}

// After an error the next key starts over. True if there was one.
bool Engine::recover()
{
    if (!error()) {
        return false;
    }
    clear();
    return true;
}

bool Engine::apply(double lhs, const QString& op, double rhs, double* result)
{
    if ((op == QStringLiteral("÷") || op == QStringLiteral("mod")) && rhs == 0) {
        fail(lhs == 0 ? tr("Result is undefined") : tr("Cannot divide by zero"));
        return false;
    }
    *result = op == QStringLiteral("+") ? lhs + rhs
        : op == QStringLiteral("−") ? lhs - rhs
        : op == QStringLiteral("×") ? lhs * rhs
        : op == QStringLiteral("÷") ? lhs / rhs
        : op == QStringLiteral("mod") ? std::fmod(lhs, rhs)
        : op == QStringLiteral("^") ? std::pow(lhs, rhs)
        : std::pow(lhs, 1 / rhs); // yroot
    if (std::isnan(*result)) {
        fail(tr("Invalid input"));
        return false;
    }
    if (!std::isfinite(*result)) {
        fail(tr("Overflow"));
        return false;
    }
    return true;
}

bool Engine::key(const QString& text)
{
    if (text.size() != 1) {
        return false;
    }
    const QChar c = text.at(0).toLower();
    static const QHash<QChar, QString> operators {
        { QLatin1Char('+'), QStringLiteral("+") }, { QLatin1Char('-'), QStringLiteral("−") },
        { QLatin1Char('*'), QStringLiteral("×") }, { QLatin1Char('/'), QStringLiteral("÷") }
    };
    static const QHash<QChar, QString> functions {
        { QLatin1Char('r'), QStringLiteral("inv") }, { QLatin1Char('q'), QStringLiteral("sqr") },
        { QLatin1Char('@'), QStringLiteral("sqrt") }
    };
    static const QHash<QChar, QString> scientificFunctions {
        { QLatin1Char('!'), QStringLiteral("fact") }, { QLatin1Char('s'), QStringLiteral("sin") },
        { QLatin1Char('o'), QStringLiteral("cos") }, { QLatin1Char('t'), QStringLiteral("tan") },
        { QLatin1Char('l'), QStringLiteral("log") }, { QLatin1Char('n'), QStringLiteral("ln") }
    };
    if (c.isDigit()) {
        digit(c.digitValue());
    } else if (c == QLatin1Char('.') || c == QLatin1Char(',')) {
        point();
    } else if (operators.contains(c)) {
        operation(operators.value(c));
    } else if (c == QLatin1Char('=')) {
        equals();
    } else if (functions.contains(c)) {
        function(functions.value(c));
    } else if (!m_scientific) {
        if (c != QLatin1Char('%')) {
            return false;
        }
        percent();
    } else if (scientificFunctions.contains(c)) {
        function(scientificFunctions.value(c));
    } else if (c == QLatin1Char('(')) {
        openParenthesis();
    } else if (c == QLatin1Char(')')) {
        closeParenthesis();
    } else if (c == QLatin1Char('^')) {
        operation(QStringLiteral("^"));
    } else if (c == QLatin1Char('%')) {
        operation(QStringLiteral("mod"));
    } else if (c == QLatin1Char('p')) {
        constant(QStringLiteral("pi"));
    } else {
        return false;
    }
    return true;
}

void Engine::digit(int digit)
{
    recover();
    if (m_entry.isEmpty()) {
        m_entry = QStringLiteral("0");
    }
    const QString n = QString::number(digit);
    if (m_entry == QStringLiteral("0")) {
        m_entry = n;
    } else if (m_entry == QStringLiteral("-0")) {
        m_entry = QLatin1Char('-') + n;
    } else if (m_entry.size() - m_entry.count(QLatin1Char('-')) - m_entry.count(QLatin1Char('.')) < MaxDigits) {
        m_entry += n;
    }
    m_operandText.clear();
    operandChanged();
    emit changed();
}

void Engine::point()
{
    recover();
    if (m_entry.isEmpty()) {
        m_entry = QStringLiteral("0");
    }
    if (!m_entry.contains(QLatin1Char('.'))) {
        m_entry += QLatin1Char('.');
    }
    m_operandText.clear();
    operandChanged();
    emit changed();
}

void Engine::backspace()
{
    if (!recover() && !m_entry.isEmpty()) {
        m_entry.chop(1);
        if (m_entry.isEmpty() || m_entry == QStringLiteral("-")) {
            m_entry = QStringLiteral("0");
        }
    }
    emit changed();
}

void Engine::clearEntry()
{
    if (!recover()) {
        m_entry.clear();
        m_value = 0;
        m_operandText.clear();
        if (m_frames.isEmpty()) {
            m_expression.clear();
        }
    }
    emit changed();
}

void Engine::clear()
{
    m_entry.clear();
    m_value = m_lastRhs = 0;
    m_operandText.clear();
    m_frames.clear();
    m_text.clear();
    m_lastOp.clear();
    m_expression.clear();
    m_error.clear();
    m_fresh = false;
    emit changed();
}

void Engine::negate()
{
    if (recover()) {
        return;
    }
    if (!m_entry.isEmpty()) {
        if (m_entry.startsWith(QLatin1Char('-'))) {
            m_entry.remove(0, 1);
        } else if (m_entry != QStringLiteral("0")) {
            m_entry.prepend(QLatin1Char('-'));
        }
    } else if (m_value != 0) {
        setValue(-m_value, QStringLiteral("negate(%1)").arg(operandText()));
    }
    emit changed();
}

void Engine::function(const QString& name)
{
    if (recover()) {
        return;
    }
    const double x = current();
    const double pi = std::acos(-1.0);
    // An angle in the chosen unit, as radians.
    const double turn = m_angleUnit == QStringLiteral("RAD") ? 1 : m_angleUnit == QStringLiteral("GRAD") ? pi / 200 : pi / 180;
    QString label = name + QStringLiteral("(%1)");
    double result = 0;
    bool valid = true;
    if (name == QStringLiteral("inv")) {
        if (x == 0) {
            fail(tr("Cannot divide by zero"));
            emit changed();
            return;
        }
        result = 1 / x;
        label = QStringLiteral("1/(%1)");
    } else if (name == QStringLiteral("sqr")) {
        result = x * x;
    } else if (name == QStringLiteral("cube")) {
        result = x * x * x;
    } else if (name == QStringLiteral("sqrt")) {
        valid = x >= 0;
        result = std::sqrt(x);
        label = QStringLiteral("√(%1)");
    } else if (name == QStringLiteral("cbrt")) {
        result = std::cbrt(x);
        label = QStringLiteral("∛(%1)");
    } else if (name == QStringLiteral("abs")) {
        result = std::fabs(x);
    } else if (name == QStringLiteral("fact")) {
        valid = x >= 0 || x != std::floor(x); // the gamma function, where it exists
        result = std::tgamma(x + 1);
    } else if (name == QStringLiteral("log")) {
        valid = x > 0;
        result = std::log10(x);
    } else if (name == QStringLiteral("ln")) {
        valid = x > 0;
        result = std::log(x);
    } else if (name == QStringLiteral("exp10")) {
        result = std::pow(10, x);
        label = QStringLiteral("10^(%1)");
    } else if (name == QStringLiteral("exp2")) {
        result = std::pow(2, x);
        label = QStringLiteral("2^(%1)");
    } else if (name == QStringLiteral("expe")) {
        result = std::exp(x);
        label = QStringLiteral("e^(%1)");
    } else if (name == QStringLiteral("sin")) {
        result = std::sin(x * turn);
    } else if (name == QStringLiteral("cos")) {
        result = std::cos(x * turn);
    } else if (name == QStringLiteral("tan")) {
        valid = std::fabs(std::cos(x * turn)) > 1e-15; // 90°
        result = std::tan(x * turn);
    } else if (name == QStringLiteral("asin")) {
        valid = std::fabs(x) <= 1;
        result = std::asin(x) / turn;
    } else if (name == QStringLiteral("acos")) {
        valid = std::fabs(x) <= 1;
        result = std::acos(x) / turn;
    } else if (name == QStringLiteral("atan")) {
        result = std::atan(x) / turn;
    } else {
        return;
    }
    if (!valid || std::isnan(result)) {
        fail(tr("Invalid input"));
    } else {
        // sin 180° is 0, not the 1.2e-16 the rounding of π leaves.
        setValue(std::fabs(result) < 1e-15 ? 0 : result, label.arg(operandText()));
    }
    emit changed();
}

void Engine::constant(const QString& name)
{
    recover();
    if (name == QStringLiteral("pi")) {
        setValue(std::acos(-1.0), QStringLiteral("π"));
    } else {
        setValue(std::exp(1.0), QStringLiteral("e"));
    }
    emit changed();
}

void Engine::openParenthesis()
{
    if (recover()) {
        return;
    }
    m_frames.append(Frame { 0, Open, m_text.size() });
    m_text += Open;
    m_entry.clear();
    m_value = 0;
    m_operandText.clear();
    m_expression.clear();
    m_fresh = false;
    emit changed();
}

// What's inside becomes the operand, written "(2 + 3)" above.
void Engine::closeParenthesis()
{
    if (recover() || openParentheses() == 0) {
        return;
    }
    double x = current();
    const QString last = operandText();
    while (m_frames.last().op != Open) {
        if (!apply(m_frames.last().lhs, m_frames.last().op, x, &x)) {
            emit changed();
            return;
        }
        m_frames.removeLast();
    }
    const Frame open = m_frames.takeLast();
    const QString inside = m_text.mid(open.text) + last + QLatin1Char(')');
    m_text.truncate(open.text);
    setValue(x, inside);
    emit changed();
}

// Of the left side when adding or subtracting (200 + 10 % is 200 + 20), a
// plain hundredth when multiplying or dividing; nothing without an operator.
void Engine::percent()
{
    if (recover()) {
        return;
    }
    const Frame* pending = pendingOperator();
    const bool additive = pending && precedence(pending->op) == 1;
    setValue(!pending ? 0 : additive ? pending->lhs * current() / 100 : current() / 100);
    emit changed();
}

void Engine::operation(const QString& op)
{
    if (recover()) {
        return;
    }
    double x = current();
    if (!m_fresh && pendingOperator()) { // the operator is only being changed
        const Frame changed = m_frames.takeLast();
        x = changed.lhs;
        m_text.chop(changed.op.size() + 1);
    } else {
        m_text += operandText() + QLatin1Char(' ');
    }
    // What can be computed now: everything in Standard, in Scientific what
    // binds at least as tightly as the new operator.
    bool reduced = false;
    while (pendingOperator() && (!m_scientific || precedence(pendingOperator()->op) >= precedence(op))) {
        if (!apply(pendingOperator()->lhs, pendingOperator()->op, x, &x)) {
            emit changed();
            return;
        }
        m_frames.removeLast();
        reduced = true;
    }
    if (reduced && !m_scientific) {
        m_text = shown(format(x)) + QLatin1Char(' '); // Standard shows the result so far: "5 ×"
    }
    m_text += op + QLatin1Char(' ');
    m_frames.append(Frame { x, op, 0 });
    m_entry.clear();
    m_value = x;
    m_operandText.clear();
    m_expression.clear();
    m_fresh = false;
    emit changed();
}

void Engine::equals()
{
    if (recover()) {
        return;
    }
    double x = current(); // 5 + = is 5 + 5
    QString text = m_text + operandText();
    QString lastOp = m_lastOp;
    double lastRhs = m_lastRhs;
    if (m_frames.isEmpty()) {
        if (m_lastOp.isEmpty()) {
            setValue(x);
            m_expression = text + QStringLiteral(" =");
            emit changed();
            return;
        }
        // = again: the last operator, on what's shown.
        text += QLatin1Char(' ') + m_lastOp + QLatin1Char(' ') + shown(format(m_lastRhs));
        if (!apply(x, m_lastOp, m_lastRhs, &x)) {
            emit changed();
            return;
        }
    } else {
        if (pendingOperator()) {
            lastOp = pendingOperator()->op;
            lastRhs = x;
        }
        // Innermost first; parentheses left open are closed.
        for (auto frame = m_frames.crbegin(); frame != m_frames.crend(); ++frame) {
            if (frame->op == Open) {
                text += QLatin1Char(')');
            } else if (!apply(frame->lhs, frame->op, x, &x)) {
                emit changed();
                return;
            }
        }
    }
    m_frames.clear();
    m_text.clear();
    setValue(x);
    m_expression = text + QStringLiteral(" =");
    m_lastOp = lastOp;
    m_lastRhs = lastRhs;
    m_history.prepend(QVariantMap {
        { QStringLiteral("expression"), m_expression },
        { QStringLiteral("result"), display() },
        { QStringLiteral("value"), x } });
    emit changed();
}

void Engine::memory(const QString& key, int index)
{
    if (recover()) {
        return;
    }
    const double x = current();
    const int at = std::max(index, 0); // the keys work on the newest
    if (key == QStringLiteral("MS")) {
        m_memory.prepend(x);
    } else if (key == QStringLiteral("MC")) {
        if (index < 0) {
            m_memory.clear();
        } else if (index < m_memory.size()) {
            m_memory.removeAt(index);
        }
        emit changed();
        return;
    } else if (key == QStringLiteral("MR")) {
        if (at < m_memory.size()) {
            setValue(m_memory.at(at));
        }
        emit changed();
        return;
    } else { // M+ and M-: with nothing stored, on a 0
        if (m_memory.isEmpty()) {
            m_memory.append(0);
        }
        if (at >= m_memory.size()) {
            return;
        }
        m_memory[at] += key == QStringLiteral("M+") ? x : -x;
    }
    m_value = x; // the next digit starts a new number
    m_entry.clear();
    emit changed();
}

void Engine::recall(int index)
{
    if (index < 0 || index >= m_history.size()) {
        return;
    }
    recover();
    setValue(m_history.at(index).toMap().value(QStringLiteral("value")).toDouble());
    emit changed();
}

void Engine::clearHistory()
{
    m_history.clear();
    emit changed();
}

void Engine::copy() const
{
    QGuiApplication::clipboard()->setText(display());
}

void Engine::paste()
{
    const QLocale locale;
    bool ok = false;
    const double value = QGuiApplication::clipboard()->text().trimmed()
        .remove(locale.groupSeparator()).replace(locale.decimalPoint(), QStringLiteral(".")).toDouble(&ok);
    if (ok) {
        recover();
        setValue(value);
        emit changed();
    }
}
