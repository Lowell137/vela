// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFont>
#include <QHash>
#include <QQuickPaintedItem>
#include <QRawFont>
#include <QVariantMap>

#include <sys/types.h>

class QSocketNotifier;
class Vt;

// A terminal: the program runs on a pty, Vt (libvterm) keeps what it shows
// and this item draws it, takes the keys and the mouse and selects text.
// It's painted into a framebuffer object, so the glyphs are drawn by the GPU
// from Qt's glyph cache, and only the lines that changed are drawn again.
class Terminal : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontChanged)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY fontChanged)
    // { foreground, background, cursor, selection, colors: the 16 of ANSI }
    Q_PROPERTY(QVariantMap scheme READ scheme WRITE setScheme NOTIFY schemeChanged)
    Q_PROPERTY(qreal backgroundOpacity READ backgroundOpacity WRITE setBackgroundOpacity NOTIFY schemeChanged)
    // How many lines back it's looking, of how many kept.
    Q_PROPERTY(int scroll READ scroll WRITE setScroll NOTIFY scrollChanged)
    Q_PROPERTY(int history READ history NOTIFY scrollChanged)
    Q_PROPERTY(int rows READ rows NOTIFY sizeChanged)
    Q_PROPERTY(int columns READ columns NOTIFY sizeChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)

public:
    explicit Terminal(QQuickItem* parent = nullptr);
    ~Terminal() override;

    QString title() const { return m_title; }
    QString fontFamily() const { return m_font.family(); }
    void setFontFamily(const QString& family);
    int fontSize() const { return m_font.pixelSize(); }
    void setFontSize(int size);
    QVariantMap scheme() const { return m_scheme; }
    void setScheme(const QVariantMap& scheme);
    qreal backgroundOpacity() const { return m_opacity; }
    void setBackgroundOpacity(qreal opacity);
    int scroll() const { return m_scroll; }
    void setScroll(int lines);
    int history() const;
    int rows() const;
    int columns() const;
    bool hasSelection() const { return m_selStart != m_selEnd || m_selected; }

    // Runs a command line in a folder (empty: the user's shell, at home).
    Q_INVOKABLE void start(const QString& command, const QString& directory);
    Q_INVOKABLE QString directory() const; // where the program is now
    Q_INVOKABLE void copy();
    Q_INVOKABLE void paste();
    Q_INVOKABLE void selectAll();

    void paint(QPainter* painter) override;

signals:
    void titleChanged();
    void fontChanged();
    void schemeChanged();
    void scrollChanged();
    void sizeChanged();
    void selectionChanged();
    void finished(); // the program ended
    void bell();

protected:
    void geometryChange(const QRectF& now, const QRectF& before) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void measure();
    void fit();
    void readProgram();
    void write(const QByteArray& bytes);
    void updateLines(int first, int last);
    QPoint cellAt(QPointF position) const; // x: column, y: line
    bool selected(int line, int column) const;
    void select(QPoint from, QPoint to);
    void clearSelection();
    QString selectedText() const;
    bool forwardMouse(QMouseEvent* event, bool pressed, bool moved);

    Vt* m_vt;
    int m_master = -1;
    pid_t m_pid = -1;
    QSocketNotifier* m_notifier = nullptr;

    QString m_title;
    QFont m_font;
    QRawFont m_raw[4]; // plain, bold, italic, both
    QHash<quint32, quint32> m_glyphs[4]; // character -> glyph, 0: not in the font
    qreal m_cellWidth = 8;
    qreal m_cellHeight = 16;
    qreal m_ascent = 12;

    QVariantMap m_scheme;
    QColor m_foreground = Qt::white;
    QColor m_background = Qt::black;
    QColor m_cursorColor = Qt::white;
    QColor m_selection = QColor(255, 255, 255, 80);
    qreal m_opacity = 1;

    int m_scroll = 0;
    QPoint m_selStart; // where the drag began
    QPoint m_selEnd;
    bool m_selected = false; // a single cell counts too (double click on a letter)
    bool m_selecting = false;
    int m_clicks = 0;
    quint64 m_doubleAt = 0;
};
