// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "vt.h"

#include <cmath>

namespace {
constexpr size_t kHistoryLines = 10000;

VTermColor toVterm(const QColor& color)
{
    VTermColor out;
    vterm_color_rgb(&out, uint8_t(color.red()), uint8_t(color.green()), uint8_t(color.blue()));
    return out;
}
}

Vt::Vt(QObject* parent)
    : QObject(parent)
{
    m_vt = vterm_new(m_rows, m_columns);
    vterm_set_utf8(m_vt, 1);
    vterm_output_set_callback(
        m_vt, [](const char* bytes, size_t length, void* user) { emit static_cast<Vt*>(user)->output(QByteArray(bytes, qsizetype(length))); },
        this);
    m_screen = vterm_obtain_screen(m_vt);
    static const VTermScreenCallbacks callbacks = {
        .damage = onDamage,
        .moverect = nullptr, // a moved piece is damage like any other
        .movecursor = onMoveCursor,
        .settermprop = onProperty,
        .bell = onBell,
        .resize = nullptr,
        .sb_pushline = onPush,
        .sb_popline = onPop,
        .sb_clear = onClear,
    };
    vterm_screen_set_callbacks(m_screen, &callbacks, this);
    vterm_screen_enable_altscreen(m_screen, 1);
    vterm_screen_enable_reflow(m_screen, true);
    vterm_screen_set_damage_merge(m_screen, VTERM_DAMAGE_SCROLL);
    vterm_screen_reset(m_screen, 1);
}

Vt::~Vt()
{
    vterm_free(m_vt);
}

void Vt::resize(int rows, int columns)
{
    rows = qMax(1, rows);
    columns = qMax(2, columns);
    if (rows == m_rows && columns == m_columns) {
        return;
    }
    m_rows = rows;
    m_columns = columns;
    vterm_set_size(m_vt, rows, columns);
    vterm_screen_flush_damage(m_screen);
    emit damaged(0, m_rows - 1);
}

void Vt::input(const QByteArray& bytes)
{
    for (const Graphics::Piece& piece : m_graphics.split(bytes)) {
        if (piece.command) {
            place(piece.keys, piece.payload);
        } else {
            vterm_input_write(m_vt, piece.text.constData(), size_t(piece.text.size()));
        }
    }
    vterm_screen_flush_damage(m_screen);
}

void Vt::setCellSize(QSizeF size)
{
    m_cell = size;
}

void Vt::place(const QHash<QByteArray, QByteArray>& keys, const QByteArray& payload)
{
    // Only a=T (transmit and show) is handled; the rest are read and dropped.
    if (keys.value("a") != "T") {
        return;
    }
    const QImage image = Graphics::decode(keys, payload);
    if (image.isNull()) {
        return;
    }
    Picture picture;
    picture.image = image;
    picture.line = m_cursor.y();
    picture.column = m_cursor.x();
    picture.columns = keys.value("c").toInt() > 0 ? keys.value("c").toInt() : int(std::ceil(image.width() / m_cell.width()));
    picture.rows = keys.value("r").toInt() > 0 ? keys.value("r").toInt() : int(std::ceil(image.height() / m_cell.height()));
    m_pictures.append(picture);
    emit damaged(0, m_rows - 1);

    // The cursor goes to the line after it, unless the program asks it not to (C=1).
    // Added after the picture, so the lines that scroll away move it too.
    if (keys.value("C") != "1") {
        const QByteArray newlines = "\r" + QByteArray(picture.rows, '\n');
        vterm_input_write(m_vt, newlines.constData(), size_t(newlines.size()));
    }
}

void Vt::setColors(const QColor& foreground, const QColor& background, const QList<QColor>& ansi)
{
    VTermState* state = vterm_obtain_state(m_vt);
    const VTermColor fg = toVterm(foreground);
    const VTermColor bg = toVterm(background);
    vterm_state_set_default_colors(state, &fg, &bg);
    for (int i = 0; i < ansi.size() && i < 16; i++) {
        const VTermColor color = toVterm(ansi.at(i));
        vterm_state_set_palette_color(state, i, &color);
    }
    emit damaged(0, m_rows - 1);
}

VTermScreenCell Vt::cell(int line, int column) const
{
    VTermScreenCell cell = {};
    if (line >= 0) {
        vterm_screen_get_cell(m_screen, VTermPos { line, column }, &cell);
        return cell;
    }
    const size_t index = size_t(-line - 1);
    if (index < m_history.size() && size_t(column) < m_history[index].size()) {
        return m_history[index][size_t(column)];
    }
    // Past a shorter line's end: empty, in the default colors.
    vterm_screen_get_cell(m_screen, VTermPos { 0, 0 }, &cell);
    VTermState* state = vterm_obtain_state(m_vt);
    vterm_state_get_default_colors(state, &cell.fg, &cell.bg);
    cell.chars[0] = 0;
    cell.width = 1;
    cell.attrs = {};
    return cell;
}

QColor Vt::color(VTermColor color) const
{
    vterm_screen_convert_color_to_rgb(m_screen, &color);
    return QColor(color.rgb.red, color.rgb.green, color.rgb.blue);
}

QString Vt::characters(const VTermScreenCell& cell)
{
    if (cell.chars[0] == 0 || cell.chars[0] == uint32_t(-1)) {
        return {};
    }
    int count = 0;
    while (count < VTERM_MAX_CHARS_PER_CELL && cell.chars[count]) {
        count++;
    }
    return QString::fromUcs4(reinterpret_cast<const char32_t*>(cell.chars), count);
}

QString Vt::text(QPoint from, QPoint to) const
{
    QString out;
    for (int line = from.y(); line <= to.y(); line++) {
        const int first = line == from.y() ? from.x() : 0;
        const int last = line == to.y() ? qMin(to.x(), m_columns - 1) : m_columns - 1;
        QString row;
        for (int column = first; column <= last; column++) {
            const VTermScreenCell c = cell(line, column);
            if (c.chars[0] == uint32_t(-1)) {
                continue; // the second half of a wide character
            }
            const QString text = characters(c);
            row += text.isEmpty() ? QStringLiteral(" ") : text;
        }
        while (row.endsWith(QLatin1Char(' '))) {
            row.chop(1);
        }
        out += row;
        // shortcut: a long line that continues below is copied with a line
        // break in it; ask libvterm's lineinfo for continuations if it matters.
        if (line < to.y()) {
            out += QLatin1Char('\n');
        }
    }
    return out;
}

void Vt::key(VTermKey key, VTermModifier modifiers)
{
    vterm_keyboard_key(m_vt, key, modifiers);
}

void Vt::character(uint32_t character, VTermModifier modifiers)
{
    vterm_keyboard_unichar(m_vt, character, modifiers);
}

void Vt::paste(const QString& text)
{
    if (text.isEmpty()) {
        return;
    }
    QString lines = text;
    lines.replace(QLatin1String("\r\n"), QLatin1String("\r")).replace(QLatin1Char('\n'), QLatin1Char('\r'));
    vterm_keyboard_start_paste(m_vt); // the brackets, when the program asked for them
    emit output(lines.toUtf8());
    vterm_keyboard_end_paste(m_vt);
}

void Vt::mouseMove(int line, int column, VTermModifier modifiers)
{
    vterm_mouse_move(m_vt, line, column, modifiers);
}

void Vt::mouseButton(int button, bool pressed, VTermModifier modifiers)
{
    vterm_mouse_button(m_vt, button, pressed, modifiers);
}

int Vt::onDamage(VTermRect rect, void* user)
{
    emit static_cast<Vt*>(user)->damaged(rect.start_row, rect.end_row - 1);
    return 1;
}

int Vt::onMoveCursor(VTermPos pos, VTermPos old, int, void* user)
{
    auto* self = static_cast<Vt*>(user);
    self->m_cursor = QPoint(pos.col, pos.row);
    emit self->damaged(qMin(pos.row, old.row), qMax(pos.row, old.row));
    return 1;
}

int Vt::onProperty(VTermProp property, VTermValue* value, void* user)
{
    auto* self = static_cast<Vt*>(user);
    switch (property) {
    case VTERM_PROP_CURSORVISIBLE:
        self->m_cursorVisible = value->boolean;
        emit self->damaged(self->m_cursor.y(), self->m_cursor.y());
        break;
    case VTERM_PROP_CURSORSHAPE:
        self->m_cursorShape = value->number;
        break;
    case VTERM_PROP_ALTSCREEN:
        self->m_altScreen = value->boolean;
        // shortcut: pictures are dropped on a screen switch, not kept per screen
        self->m_pictures.clear();
        emit self->damaged(0, self->m_rows - 1);
        break;
    case VTERM_PROP_MOUSE:
        self->m_mouse = value->number;
        break;
    case VTERM_PROP_TITLE:
        // It arrives in pieces.
        if (value->string.initial) {
            self->m_title.clear();
        }
        self->m_title.append(value->string.str, qsizetype(value->string.len));
        if (value->string.final) {
            emit self->titleChanged(QString::fromUtf8(self->m_title));
        }
        break;
    default:
        break;
    }
    return 1;
}

int Vt::onBell(void* user)
{
    emit static_cast<Vt*>(user)->bell();
    return 1;
}

int Vt::onPush(int columns, const VTermScreenCell* cells, void* user)
{
    auto* self = static_cast<Vt*>(user);
    self->m_history.emplace_front(cells, cells + columns);
    if (self->m_history.size() > kHistoryLines) {
        self->m_history.pop_back();
    }
    for (Picture& picture : self->m_pictures) {
        picture.line--;
    }
    // A picture that has gone past the kept history is gone.
    self->m_pictures.removeIf([self](const Picture& picture) {
        return picture.line + picture.rows <= -int(self->m_history.size());
    });
    emit self->pushed();
    return 1;
}

// A taller screen takes lines back from the history.
int Vt::onPop(int columns, VTermScreenCell* cells, void* user)
{
    auto* self = static_cast<Vt*>(user);
    if (self->m_history.empty()) {
        return 0;
    }
    const std::vector<VTermScreenCell>& line = self->m_history.front();
    for (int i = 0; i < columns; i++) {
        if (size_t(i) < line.size()) {
            cells[i] = line[size_t(i)];
        } else {
            cells[i] = {};
            cells[i].width = 1;
            VTermState* state = vterm_obtain_state(self->m_vt);
            vterm_state_get_default_colors(state, &cells[i].fg, &cells[i].bg);
        }
    }
    self->m_history.pop_front();
    return 1;
}

int Vt::onClear(void* user)
{
    auto* self = static_cast<Vt*>(user);
    self->m_history.clear();
    self->m_pictures.clear();
    emit self->damaged(0, self->m_rows - 1);
    return 1;
}
