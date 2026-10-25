// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "terminal.h"

#include "vt.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFontMetricsF>
#include <QGlyphRun>
#include <QGuiApplication>
#include <QPainter>
#include <QProcess>
#include <QSocketNotifier>
#include <QStyleHints>

#include <csignal>
#include <pty.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {
constexpr qreal kPadding = 6; // around the text

VTermModifier modifiersOf(Qt::KeyboardModifiers modifiers)
{
    int out = VTERM_MOD_NONE;
    if (modifiers & Qt::ShiftModifier) {
        out |= VTERM_MOD_SHIFT;
    }
    if (modifiers & Qt::AltModifier) {
        out |= VTERM_MOD_ALT;
    }
    if (modifiers & Qt::ControlModifier) {
        out |= VTERM_MOD_CTRL;
    }
    return VTermModifier(out);
}

// Lines first, then columns.
bool before(QPoint a, QPoint b)
{
    return a.y() < b.y() || (a.y() == b.y() && a.x() < b.x());
}
}

Terminal::Terminal(QQuickItem* parent)
    : QQuickPaintedItem(parent)
    , m_vt(new Vt(this))
{
    setRenderTarget(QQuickPaintedItem::FramebufferObject); // drawn by the GPU
    setFillColor(Qt::transparent);
    setAcceptedMouseButtons(Qt::AllButtons);
    setFlag(ItemIsFocusScope, false);
    setActiveFocusOnTab(true);
    setFlag(ItemAcceptsInputMethod, false);

    m_font.setStyleHint(QFont::Monospace);
    m_font.setFixedPitch(true);
    m_font.setFamily(QStringLiteral("monospace"));
    m_font.setPixelSize(14);
    m_font.setKerning(false);
    measure();

    connect(m_vt, &Vt::output, this, &Terminal::write);
    connect(m_vt, &Vt::damaged, this, &Terminal::updateLines);
    connect(m_vt, &Vt::pushed, this, [this] {
        // What's being read stays where it is while more arrives below.
        if (m_scroll > 0) {
            m_scroll = qMin(m_scroll + 1, m_vt->history());
        }
        if (m_selected) {
            m_selStart.ry()--;
            m_selEnd.ry()--;
        }
        emit scrollChanged();
        update();
    });
    connect(m_vt, &Vt::titleChanged, this, [this](const QString& title) {
        m_title = title;
        emit titleChanged();
    });
    connect(m_vt, &Vt::bell, this, &Terminal::bell);
}

Terminal::~Terminal()
{
    if (m_pid > 0) {
        ::kill(m_pid, SIGHUP);
    }
    if (m_master >= 0) {
        ::close(m_master);
    }
}

void Terminal::setFontFamily(const QString& family)
{
    if (family.isEmpty() || family == m_font.family()) {
        return;
    }
    m_font.setFamily(family);
    measure();
    emit fontChanged();
}

void Terminal::setFontSize(int size)
{
    size = qBound(6, size, 72);
    if (size == m_font.pixelSize()) {
        return;
    }
    m_font.setPixelSize(size);
    measure();
    emit fontChanged();
}

void Terminal::measure()
{
    for (int style = 0; style < 4; style++) {
        QFont font = m_font;
        font.setBold(style & 1);
        font.setItalic(style & 2);
        m_raw[style] = QRawFont::fromFont(font);
        m_glyphs[style].clear();
    }
    const QFontMetricsF metrics(m_font);
    m_cellWidth = qMax(1.0, std::ceil(metrics.horizontalAdvance(QLatin1Char('M'))));
    m_cellHeight = qMax(1.0, std::ceil(metrics.lineSpacing()));
    m_ascent = std::ceil(metrics.ascent());
    m_vt->setCellSize(QSizeF(m_cellWidth, m_cellHeight));
    fit();
    update();
}

void Terminal::setScheme(const QVariantMap& scheme)
{
    m_scheme = scheme;
    m_foreground = QColor(scheme.value(QStringLiteral("foreground")).toString());
    m_background = QColor(scheme.value(QStringLiteral("background")).toString());
    m_cursorColor = QColor(scheme.value(QStringLiteral("cursor"), m_foreground.name()).toString());
    m_selection = QColor(scheme.value(QStringLiteral("selection"), QStringLiteral("#66808080")).toString());
    QList<QColor> ansi;
    for (const QVariant& color : scheme.value(QStringLiteral("colors")).toList()) {
        ansi.append(QColor(color.toString()));
    }
    m_vt->setColors(m_foreground, m_background, ansi);
    emit schemeChanged();
    update();
}

void Terminal::setBackgroundOpacity(qreal opacity)
{
    opacity = qBound(0.0, opacity, 1.0);
    if (qFuzzyCompare(opacity, m_opacity)) {
        return;
    }
    m_opacity = opacity;
    emit schemeChanged();
    update();
}

int Terminal::history() const { return m_vt->history(); }
int Terminal::rows() const { return m_vt->rows(); }
int Terminal::columns() const { return m_vt->columns(); }

void Terminal::setScroll(int lines)
{
    lines = qBound(0, lines, m_vt->altScreen() ? 0 : m_vt->history());
    if (lines == m_scroll) {
        return;
    }
    m_scroll = lines;
    emit scrollChanged();
    update();
}

// --- the program ---

void Terminal::start(const QString& command, const QString& directory)
{
    if (m_pid > 0) {
        return;
    }
    QStringList words = QProcess::splitCommand(command);
    if (words.isEmpty()) {
        words = { qEnvironmentVariable("SHELL", QStringLiteral("/bin/sh")) };
    }
    // Everything the child needs is made before the fork: after it, only
    // calls that are safe between fork and exec.
    QList<QByteArray> bytes;
    std::vector<char*> arguments;
    for (const QString& word : std::as_const(words)) {
        bytes.append(QFile::encodeName(word));
    }
    for (QByteArray& word : bytes) {
        arguments.push_back(word.data());
    }
    arguments.push_back(nullptr);
    const QByteArray folder = QFile::encodeName(directory.isEmpty() ? QDir::homePath() : directory);

    struct winsize size = {};
    size.ws_row = static_cast<unsigned short>(m_vt->rows());
    size.ws_col = static_cast<unsigned short>(m_vt->columns());
    m_pid = forkpty(&m_master, nullptr, nullptr, &size);
    if (m_pid == 0) {
        if (::chdir(folder.constData()) != 0) {
            // A folder that's gone: wherever we are.
        }
        ::signal(SIGCHLD, SIG_DFL); // ignored in main.cpp
        ::execvp(arguments[0], arguments.data());
        ::_exit(127);
    }
    if (m_pid < 0) {
        m_vt->input(QByteArrayLiteral("Could not open a terminal for the program.\r\n"));
        return;
    }
    m_notifier = new QSocketNotifier(m_master, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &Terminal::readProgram);
}

void Terminal::readProgram()
{
    char buffer[65536];
    const ssize_t count = ::read(m_master, buffer, sizeof(buffer));
    if (count > 0) {
        m_vt->input(QByteArray::fromRawData(buffer, count));
        return;
    }
    if (count < 0 && (errno == EINTR || errno == EAGAIN)) {
        return;
    }
    // The program closed its side: it ended.
    m_notifier->setEnabled(false);
    ::close(m_master);
    m_master = -1;
    m_pid = -1;
    emit finished();
}

void Terminal::write(const QByteArray& bytes)
{
    // shortcut: the write waits when the program doesn't read (a very large
    // paste into one that isn't listening); queue behind a write notifier if
    // that's ever seen.
    qsizetype done = 0;
    while (m_master >= 0 && done < bytes.size()) {
        const ssize_t count = ::write(m_master, bytes.constData() + done, size_t(bytes.size() - done));
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        done += count;
    }
}

QString Terminal::directory() const
{
    return m_pid > 0 ? QFile::symLinkTarget(QStringLiteral("/proc/%1/cwd").arg(m_pid)) : QString();
}

// --- its size ---

void Terminal::geometryChange(const QRectF& now, const QRectF& before)
{
    QQuickPaintedItem::geometryChange(now, before);
    if (now.size() != before.size()) {
        fit();
    }
}

void Terminal::fit()
{
    const int rows = int((height() - 2 * kPadding) / m_cellHeight);
    const int columns = int((width() - 2 * kPadding) / m_cellWidth);
    if (rows < 1 || columns < 2 || (rows == m_vt->rows() && columns == m_vt->columns())) {
        return;
    }
    m_vt->resize(rows, columns);
    if (m_master >= 0) {
        struct winsize size = {};
        size.ws_row = static_cast<unsigned short>(rows);
        size.ws_col = static_cast<unsigned short>(columns);
        ::ioctl(m_master, TIOCSWINSZ, &size);
    }
    m_scroll = qMin(m_scroll, m_vt->history());
    emit sizeChanged();
    emit scrollChanged();
    update();
}

void Terminal::updateLines(int first, int last)
{
    const qreal top = kPadding + (first + m_scroll) * m_cellHeight;
    update(QRectF(0, top, width(), (last - first + 1) * m_cellHeight).toAlignedRect());
}

// --- drawing ---

void Terminal::paint(QPainter* painter)
{
    QRectF clip = painter->clipBoundingRect();
    if (clip.isEmpty()) {
        clip = boundingRect();
    }
    QColor background = m_background;
    background.setAlphaF(m_opacity);
    painter->setCompositionMode(QPainter::CompositionMode_Source);
    painter->fillRect(clip, background);

    const int rows = m_vt->rows();
    const int columns = m_vt->columns();
    const int firstRow = qMax(0, int((clip.top() - kPadding) / m_cellHeight));
    const int lastRow = qMin(rows - 1, int((clip.bottom() - kPadding) / m_cellHeight));
    const QPoint cursor = m_vt->cursor();
    const bool showCursor = m_vt->cursorVisible() && m_scroll == 0;

    struct Drawn {
        QColor foreground;
        int style;
        QString fallback; // drawn as text: not in the font, or with combining marks
        quint32 glyph;
        bool underline;
        bool strike;
        int width;
    };
    std::vector<Drawn> cells;
    cells.resize(size_t(columns));

    for (int row = firstRow; row <= lastRow; row++) {
        const int line = row - m_scroll;
        const qreal y = kPadding + row * m_cellHeight;

        // The backgrounds, a stretch of the same color at a time.
        painter->setCompositionMode(QPainter::CompositionMode_Source);
        int runStart = 0;
        QColor runColor;
        const auto flush = [&](int end) {
            if (runColor.isValid() && end > runStart) {
                painter->fillRect(QRectF(kPadding + runStart * m_cellWidth, y, (end - runStart) * m_cellWidth, m_cellHeight), runColor);
            }
        };
        for (int column = 0; column < columns; column++) {
            const VTermScreenCell cell = m_vt->cell(line, column);
            QColor fg = VTERM_COLOR_IS_DEFAULT_FG(&cell.fg) ? m_foreground : m_vt->color(cell.fg);
            QColor bg = VTERM_COLOR_IS_DEFAULT_BG(&cell.bg) ? QColor() : m_vt->color(cell.bg);
            if (cell.attrs.reverse) {
                const QColor old = fg;
                fg = bg.isValid() ? bg : m_background;
                bg = old;
            }
            if (showCursor && hasActiveFocus() && line == cursor.y() && column == cursor.x() && m_vt->cursorShape() == VTERM_PROP_CURSORSHAPE_BLOCK) {
                bg = m_cursorColor;
                fg = m_background;
            } else if (selected(line, column)) {
                // Mixed here: the background is written as it is, not blended.
                const QColor under = bg.isValid() ? bg : m_background;
                const float a = m_selection.alphaF();
                bg = QColor::fromRgbF(under.redF() * (1 - a) + m_selection.redF() * a, under.greenF() * (1 - a) + m_selection.greenF() * a,
                    under.blueF() * (1 - a) + m_selection.blueF() * a);
            }
            if (bg != runColor) {
                flush(column);
                runStart = column;
                runColor = bg;
            }

            Drawn& drawn = cells[size_t(column)];
            drawn.foreground = fg;
            drawn.style = (cell.attrs.bold ? 1 : 0) | (cell.attrs.italic ? 2 : 0);
            drawn.underline = cell.attrs.underline != 0;
            drawn.strike = cell.attrs.strike;
            drawn.width = qMax(1, int(cell.width));
            drawn.glyph = 0;
            drawn.fallback.clear();
            const quint32 character = cell.chars[0];
            if (character == 0 || character == uint32_t(-1) || character == ' ' || cell.attrs.conceal) {
                continue;
            }
            if (cell.chars[1] == 0) {
                auto found = m_glyphs[drawn.style].constFind(character);
                if (found == m_glyphs[drawn.style].constEnd()) {
                    const QList<quint32> glyphs = m_raw[drawn.style].glyphIndexesForString(QString::fromUcs4(reinterpret_cast<const char32_t*>(&character), 1));
                    found = m_glyphs[drawn.style].insert(character, glyphs.size() == 1 ? glyphs.first() : 0);
                }
                drawn.glyph = *found;
            }
            if (drawn.glyph == 0) {
                drawn.fallback = Vt::characters(cell);
            }
        }
        flush(columns);

        // The text, a stretch of the same color and weight at a time: glyphs
        // placed on the grid, without shaping.
        painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
        int column = 0;
        while (column < columns) {
            const Drawn& first = cells[size_t(column)];
            QList<quint32> glyphs;
            QList<QPointF> positions;
            int end = column;
            while (end < columns && cells[size_t(end)].foreground == first.foreground && cells[size_t(end)].style == first.style) {
                const Drawn& drawn = cells[size_t(end)];
                const qreal x = kPadding + end * m_cellWidth;
                if (drawn.glyph != 0) {
                    glyphs.append(drawn.glyph);
                    positions.append(QPointF(x, y + m_ascent));
                }
                if (!drawn.fallback.isEmpty() || drawn.underline || drawn.strike) {
                    painter->setPen(drawn.foreground);
                    if (!drawn.fallback.isEmpty()) {
                        QFont font = m_font;
                        font.setBold(drawn.style & 1);
                        font.setItalic(drawn.style & 2);
                        painter->setFont(font);
                        painter->drawText(QPointF(x, y + m_ascent), drawn.fallback);
                    }
                    if (drawn.underline) {
                        painter->fillRect(QRectF(x, y + m_ascent + 1, drawn.width * m_cellWidth, 1), drawn.foreground);
                    }
                    if (drawn.strike) {
                        painter->fillRect(QRectF(x, y + m_cellHeight / 2, drawn.width * m_cellWidth, 1), drawn.foreground);
                    }
                }
                end++;
            }
            if (!glyphs.isEmpty()) {
                QGlyphRun run;
                run.setRawFont(m_raw[first.style]);
                run.setGlyphIndexes(glyphs);
                run.setPositions(positions);
                painter->setPen(first.foreground);
                painter->drawGlyphRun(QPointF(0, 0), run);
            }
            column = end;
        }

        // The cursor when it isn't a full block, or the window isn't the one typed in.
        if (showCursor && line == cursor.y() && !(hasActiveFocus() && m_vt->cursorShape() == VTERM_PROP_CURSORSHAPE_BLOCK)) {
            const QRectF cell(kPadding + cursor.x() * m_cellWidth, y, m_cellWidth, m_cellHeight);
            if (!hasActiveFocus()) {
                painter->setPen(m_cursorColor);
                painter->drawRect(cell.adjusted(0.5, 0.5, -0.5, -0.5));
            } else if (m_vt->cursorShape() == VTERM_PROP_CURSORSHAPE_UNDERLINE) {
                painter->fillRect(QRectF(cell.left(), cell.bottom() - 2, cell.width(), 2), m_cursorColor);
            } else {
                painter->fillRect(QRectF(cell.left(), cell.top(), 2, cell.height()), m_cursorColor);
            }
        }
    }

    // Drawn last, over the text. The rows above leave Source mode on, which would replace what's under
    // the picture, not blend with it, so it's set back. A line drawn again gets its fill first.
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    for (const Vt::Picture& picture : m_vt->pictures()) {
        const QRectF target(kPadding + picture.column * m_cellWidth, kPadding + (picture.line + m_scroll) * m_cellHeight,
            picture.columns * m_cellWidth, picture.rows * m_cellHeight);
        if (target.intersects(clip)) {
            painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter->drawImage(target, picture.image);
        }
    }
}

// --- keys ---

void Terminal::keyPressEvent(QKeyEvent* event)
{
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    const int key = event->key();
    if ((modifiers & Qt::ControlModifier) && (modifiers & Qt::ShiftModifier)) {
        if (key == Qt::Key_C) {
            copy();
            return;
        }
        if (key == Qt::Key_V) {
            paste();
            return;
        }
        if (key == Qt::Key_A) {
            selectAll();
            return;
        }
    }
    // As in Windows Terminal: Ctrl+V pastes, and Ctrl+C copies what's selected.
    // With nothing selected it's the interrupt it has always been.
    if (modifiers == Qt::ControlModifier && key == Qt::Key_V) {
        paste();
        return;
    }
    if (modifiers == Qt::ControlModifier && key == Qt::Key_C && hasSelection()) {
        copy();
        clearSelection();
        return;
    }
    if ((modifiers & Qt::ShiftModifier) && (key == Qt::Key_PageUp || key == Qt::Key_PageDown)) {
        setScroll(m_scroll + (key == Qt::Key_PageUp ? 1 : -1) * qMax(1, m_vt->rows() - 1));
        return;
    }

    static const QHash<int, VTermKey> keys = {
        { Qt::Key_Return, VTERM_KEY_ENTER }, { Qt::Key_Enter, VTERM_KEY_ENTER }, { Qt::Key_Tab, VTERM_KEY_TAB },
        { Qt::Key_Backtab, VTERM_KEY_TAB }, { Qt::Key_Backspace, VTERM_KEY_BACKSPACE }, { Qt::Key_Escape, VTERM_KEY_ESCAPE },
        { Qt::Key_Up, VTERM_KEY_UP }, { Qt::Key_Down, VTERM_KEY_DOWN }, { Qt::Key_Left, VTERM_KEY_LEFT },
        { Qt::Key_Right, VTERM_KEY_RIGHT }, { Qt::Key_Insert, VTERM_KEY_INS }, { Qt::Key_Delete, VTERM_KEY_DEL },
        { Qt::Key_Home, VTERM_KEY_HOME }, { Qt::Key_End, VTERM_KEY_END }, { Qt::Key_PageUp, VTERM_KEY_PAGEUP },
        { Qt::Key_PageDown, VTERM_KEY_PAGEDOWN }
    };
    const VTermModifier mods = modifiersOf(modifiers);
    if (keys.contains(key)) {
        m_vt->key(keys.value(key), mods);
    } else if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        m_vt->key(VTermKey(VTERM_KEY_FUNCTION(key - Qt::Key_F1 + 1)), mods);
    } else if ((modifiers & Qt::ControlModifier) && key >= 0x20 && key < 0x80) {
        // Ctrl+C and the like: the letter, and libvterm makes the control character.
        m_vt->character(uint32_t(QChar::toLower(char32_t(key))), VTermModifier(mods & ~VTERM_MOD_SHIFT));
    } else if (!event->text().isEmpty()) {
        const QList<uint> characters = event->text().toUcs4();
        for (const uint character : characters) {
            m_vt->character(character, VTermModifier(mods & VTERM_MOD_ALT));
        }
    } else {
        event->ignore();
        return;
    }
    // Typing brings back the last line and lets go of what was selected.
    setScroll(0);
    clearSelection();
}

void Terminal::focusInEvent(QFocusEvent* event)
{
    QQuickPaintedItem::focusInEvent(event);
    update();
}

void Terminal::focusOutEvent(QFocusEvent* event)
{
    QQuickPaintedItem::focusOutEvent(event);
    update();
}

// --- the mouse: the program's when it asks for it (Shift: ours anyway),
// otherwise it selects ---

QPoint Terminal::cellAt(QPointF position) const
{
    const int column = qBound(0, int((position.x() - kPadding) / m_cellWidth), m_vt->columns() - 1);
    const int row = qBound(0, int((position.y() - kPadding) / m_cellHeight), m_vt->rows() - 1);
    return QPoint(column, row - m_scroll);
}

bool Terminal::forwardMouse(QMouseEvent* event, bool pressed, bool moved)
{
    if (m_vt->mouse() == 0 || (event->modifiers() & Qt::ShiftModifier) || m_scroll != 0) {
        return false;
    }
    const QPoint cell = cellAt(event->position());
    const VTermModifier mods = modifiersOf(event->modifiers());
    m_vt->mouseMove(cell.y(), cell.x(), mods);
    if (!moved) {
        const int button = event->button() == Qt::LeftButton ? 1 : event->button() == Qt::MiddleButton ? 2 : 3;
        m_vt->mouseButton(button, pressed, mods);
    }
    return true;
}

void Terminal::mousePressEvent(QMouseEvent* event)
{
    forceActiveFocus();
    if (forwardMouse(event, true, false)) {
        return;
    }
    if (event->button() == Qt::MiddleButton) {
        // The compositor may not offer a primary selection: use our own copy then.
        const QString primary = QGuiApplication::clipboard()->text(QClipboard::Selection);
        m_vt->paste(primary.isEmpty() ? m_primary : primary);
        setScroll(0);
        return;
    }
    if (event->button() == Qt::RightButton) {
        emit contextMenuRequested(event->position());
        return;
    }
    const QPoint cell = cellAt(event->position());
    // A third click right after two: the line.
    if (m_clicks == 2 && event->timestamp() - m_doubleAt < quint64(QGuiApplication::styleHints()->mouseDoubleClickInterval())) {
        m_clicks = 0;
        m_selected = true;
        select(QPoint(0, cell.y()), QPoint(m_vt->columns() - 1, cell.y()));
        setPrimary();
        return;
    }
    m_selecting = true;
    m_selected = false;
    m_clicks = 1;
    select(cell, cell);
}

void Terminal::mouseMoveEvent(QMouseEvent* event)
{
    if (forwardMouse(event, false, true)) {
        return;
    }
    if (!m_selecting) {
        return;
    }
    // Past the edge while dragging: the text follows.
    if (event->position().y() < 0) {
        setScroll(m_scroll + 1);
    } else if (event->position().y() > height()) {
        setScroll(m_scroll - 1);
    }
    // Only once it leaves the cell: a click with a trembling hand selects nothing.
    const QPoint cell = cellAt(event->position());
    if (cell != m_selStart || m_selected) {
        m_selected = true;
        select(m_selStart, cell);
    }
}

void Terminal::mouseReleaseEvent(QMouseEvent* event)
{
    if (forwardMouse(event, false, false)) {
        return;
    }
    if (m_selecting) {
        m_selecting = false;
        if (hasSelection()) {
            setPrimary();
        }
    }
}

void Terminal::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || forwardMouse(event, true, false)) {
        return;
    }
    // Twice: the word.
    m_clicks = 2;
    m_doubleAt = event->timestamp();
    const QPoint cell = cellAt(event->position());
    int first = cell.x();
    int last = cell.x();
    const auto inWord = [this, &cell](int column) {
        const QString text = Vt::characters(m_vt->cell(cell.y(), column));
        return !text.isEmpty() && !text.at(0).isSpace();
    };
    while (first > 0 && inWord(first - 1)) {
        first--;
    }
    while (last < m_vt->columns() - 1 && inWord(last + 1)) {
        last++;
    }
    m_selected = true;
    m_selecting = false;
    select(QPoint(first, cell.y()), QPoint(last, cell.y()));
    setPrimary();
}

void Terminal::wheelEvent(QWheelEvent* event)
{
    const int lines = qRound(event->angleDelta().y() / 120.0 * 3);
    if (lines == 0) {
        return;
    }
    if (m_vt->mouse() != 0 && !(event->modifiers() & Qt::ShiftModifier)) {
        const QPoint cell = cellAt(event->position());
        m_vt->mouseMove(cell.y(), cell.x(), VTERM_MOD_NONE);
        for (int i = 0; i < qAbs(lines); i++) {
            m_vt->mouseButton(lines > 0 ? 4 : 5, true, VTERM_MOD_NONE);
        }
    } else if (m_vt->altScreen()) {
        // A pager or an editor without the mouse: the wheel is the arrows.
        for (int i = 0; i < qAbs(lines); i++) {
            m_vt->key(lines > 0 ? VTERM_KEY_UP : VTERM_KEY_DOWN, VTERM_MOD_NONE);
        }
    } else {
        setScroll(m_scroll + lines);
    }
}

// --- the selection ---

void Terminal::clearSelection()
{
    m_selected = false;
    select(QPoint(), QPoint());
}

void Terminal::select(QPoint from, QPoint to)
{
    m_selStart = from;
    m_selEnd = to;
    emit selectionChanged();
    update();
}

bool Terminal::selected(int line, int column) const
{
    if (!hasSelection()) {
        return false;
    }
    const QPoint first = before(m_selEnd, m_selStart) ? m_selEnd : m_selStart;
    const QPoint last = before(m_selEnd, m_selStart) ? m_selStart : m_selEnd;
    const QPoint here(column, line);
    return !before(here, first) && !before(last, here);
}

QString Terminal::selectedText() const
{
    if (!hasSelection()) {
        return {};
    }
    const bool backwards = before(m_selEnd, m_selStart);
    return m_vt->text(backwards ? m_selEnd : m_selStart, backwards ? m_selStart : m_selEnd);
}

void Terminal::setPrimary()
{
    m_primary = selectedText();
    QGuiApplication::clipboard()->setText(m_primary, QClipboard::Selection);
}

void Terminal::copy()
{
    const QString text = selectedText();
    if (!text.isEmpty()) {
        QGuiApplication::clipboard()->setText(text);
    }
}

void Terminal::paste()
{
    m_vt->paste(QGuiApplication::clipboard()->text());
    setScroll(0);
}

void Terminal::selectAll()
{
    m_selected = true;
    select(QPoint(0, -m_vt->history()), QPoint(m_vt->columns() - 1, m_vt->rows() - 1));
}
