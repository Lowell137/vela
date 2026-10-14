// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QObject>
#include <QPoint>

#include <deque>
#include <vector>

extern "C" {
#include <vterm.h>
}

// What a program in the terminal shows: libvterm reads what it writes and
// keeps the screen; here are the lines that left it at the top (the history)
// and the text between two places. Lines are counted from the screen's first
// (0); the history's are negative, -1 the most recent.
class Vt : public QObject {
    Q_OBJECT

public:
    explicit Vt(QObject* parent = nullptr);
    ~Vt() override;

    int rows() const { return m_rows; }
    int columns() const { return m_columns; }
    int history() const { return int(m_history.size()); }
    void resize(int rows, int columns);

    void input(const QByteArray& bytes); // what the program wrote
    void setColors(const QColor& foreground, const QColor& background, const QList<QColor>& ansi);

    VTermScreenCell cell(int line, int column) const;
    QColor color(VTermColor color) const; // not for the default ones: those are the item's
    static QString characters(const VTermScreenCell& cell);
    // The text from one place to another (x: column, y: line), both included.
    QString text(QPoint from, QPoint to) const;

    QPoint cursor() const { return m_cursor; }
    bool cursorVisible() const { return m_cursorVisible; }
    int cursorShape() const { return m_cursorShape; }
    bool altScreen() const { return m_altScreen; }
    int mouse() const { return m_mouse; } // 0: the program doesn't ask for it

    void key(VTermKey key, VTermModifier modifiers);
    void character(uint32_t character, VTermModifier modifiers);
    void paste(const QString& text);
    void mouseMove(int line, int column, VTermModifier modifiers);
    void mouseButton(int button, bool pressed, VTermModifier modifiers);

signals:
    void output(const QByteArray& bytes); // for the program
    void damaged(int firstLine, int lastLine);
    void pushed(); // a line went into the history
    void titleChanged(const QString& title);
    void bell();

private:
    static int onDamage(VTermRect rect, void* user);
    static int onMoveCursor(VTermPos pos, VTermPos old, int visible, void* user);
    static int onProperty(VTermProp property, VTermValue* value, void* user);
    static int onBell(void* user);
    static int onPush(int columns, const VTermScreenCell* cells, void* user);
    static int onPop(int columns, VTermScreenCell* cells, void* user);
    static int onClear(void* user);

    VTerm* m_vt = nullptr;
    VTermScreen* m_screen = nullptr;
    int m_rows = 24;
    int m_columns = 80;
    std::deque<std::vector<VTermScreenCell>> m_history; // front: the most recent
    QPoint m_cursor;
    bool m_cursorVisible = true;
    int m_cursorShape = VTERM_PROP_CURSORSHAPE_BLOCK;
    bool m_altScreen = false;
    int m_mouse = 0;
    QByteArray m_title;
};
