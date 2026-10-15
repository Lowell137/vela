// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "notestore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QVariantMap>

#include <algorithm>

namespace {

struct Swatch { const char* name; const char* body; const char* strip; const char* text; };
// Windows' seven pastels; the strip is the title bar, a shade darker.
constexpr Swatch kSwatches[] = {
    { "yellow", "#fff7d1", "#f7e98a", "#1f1f1f" },
    { "green", "#e2f7cb", "#c6ec9e", "#1f1f1f" },
    { "pink", "#ffe4e8", "#fbc4cf", "#1f1f1f" },
    { "purple", "#ebdbfb", "#d6bcf4", "#1f1f1f" },
    { "blue", "#d9e8fb", "#b6d3f6", "#1f1f1f" },
    { "gray", "#ececec", "#d4d4d4", "#1f1f1f" },
    { "charcoal", "#575757", "#434343", "#f5f5f5" },
};

constexpr int kMinSize = 160;

} // namespace

NoteStore::NoteStore(const QString& path, int saveDelayMs, QObject* parent)
    : QAbstractListModel(parent)
    , m_path(path)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(saveDelayMs);
    connect(&m_timer, &QTimer::timeout, this, &NoteStore::flush);
    load();
    rebuildView();
}

QString NoteStore::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/Vela/stickynotes/notes.json");
}

const QStringList& NoteStore::colorNames()
{
    static const QStringList names = [] {
        QStringList list;
        for (const Swatch& s : kSwatches) {
            list << QString::fromLatin1(s.name);
        }
        return list;
    }();
    return names;
}

QVariantList NoteStore::palette()
{
    QVariantList list;
    for (const Swatch& s : kSwatches) {
        list << QVariantMap { { "name", s.name }, { "body", s.body }, { "strip", s.strip }, { "text", s.text } };
    }
    return list;
}

QString NoteStore::validColor(const QString& name)
{
    return colorNames().contains(name) ? name : colorNames().first();
}

bool NoteStore::matches(const QString& text, const QString& filter)
{
    const QString needle = filter.trimmed();
    return needle.isEmpty() || text.contains(needle, Qt::CaseInsensitive);
}

int NoteStore::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_view.size();
}

QVariant NoteStore::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_view.size()) {
        return {};
    }
    const Note& n = m_notes[m_view[index.row()]];
    switch (role) {
    case IdRole: return n.id;
    case TextRole: return n.text;
    case PreviewRole: {
        for (const QString& line : n.text.split(u'\n')) {
            if (!line.trimmed().isEmpty()) {
                return line.trimmed();
            }
        }
        return QString();
    }
    case ColorRole: return n.color;
    case ModifiedRole: return QDateTime::fromMSecsSinceEpoch(n.modified);
    }
    return {};
}

QHash<int, QByteArray> NoteStore::roleNames() const
{
    return { { IdRole, "noteId" }, { TextRole, "text" }, { PreviewRole, "preview" },
             { ColorRole, "noteColor" }, { ModifiedRole, "modified" } };
}

void NoteStore::setFilter(const QString& filter)
{
    if (filter == m_filter) {
        return;
    }
    m_filter = filter;
    rebuildView();
    emit filterChanged();
}

QVariantList NoteStore::openIds() const
{
    QVariantList ids;
    for (const Note& n : m_notes) {
        if (n.open) {
            ids << n.id;
        }
    }
    return ids;
}

int NoteStore::indexOf(int id) const
{
    for (int i = 0; i < m_notes.size(); ++i) {
        if (m_notes[i].id == id) {
            return i;
        }
    }
    return -1;
}

int NoteStore::create(const QString& color)
{
    Note n;
    for (const Note& other : std::as_const(m_notes)) {
        n.id = std::max(n.id, other.id);
    }
    n.id += 1; // above every id in use, also after deleting the newest
    n.color = validColor(color);
    n.open = true;
    n.modified = QDateTime::currentMSecsSinceEpoch();
    m_notes.prepend(n);
    rebuildView();
    emit countChanged();
    emit openIdsChanged();
    touch();
    return n.id;
}

void NoteStore::remove(int id)
{
    const int i = indexOf(id);
    if (i < 0) {
        return;
    }
    const bool wasOpen = m_notes[i].open;
    m_notes.removeAt(i);
    rebuildView();
    emit countChanged();
    if (wasOpen) {
        emit openIdsChanged();
    }
    touch();
}

QVariantMap NoteStore::get(int id) const
{
    const int i = indexOf(id);
    if (i < 0) {
        return {};
    }
    const Note& n = m_notes[i];
    return { { "id", n.id }, { "text", n.text }, { "color", n.color }, { "x", n.x }, { "y", n.y },
             { "width", n.width }, { "height", n.height }, { "open", n.open } };
}

void NoteStore::setText(int id, const QString& text)
{
    const int i = indexOf(id);
    if (i < 0 || m_notes[i].text == text) {
        return;
    }
    m_notes[i].text = text;
    m_notes[i].modified = QDateTime::currentMSecsSinceEpoch();
    if (i != 0) {
        m_notes.move(i, 0); // the list is newest first
    }
    rebuildView();
    touch();
}

void NoteStore::setColor(int id, const QString& color)
{
    const int i = indexOf(id);
    const QString valid = validColor(color);
    if (i < 0 || m_notes[i].color == valid) {
        return;
    }
    m_notes[i].color = valid;
    rebuildView();
    touch();
}

void NoteStore::setGeometry(int id, int x, int y, int width, int height)
{
    const int i = indexOf(id);
    if (i < 0) {
        return;
    }
    Note& n = m_notes[i];
    if (n.x == x && n.y == y && n.width == width && n.height == height) {
        return;
    }
    n.x = x;
    n.y = y;
    n.width = std::max(width, kMinSize);
    n.height = std::max(height, kMinSize);
    touch(); // not the list's business
}

void NoteStore::setOpen(int id, bool open)
{
    const int i = indexOf(id);
    // While quitting the windows go away without the user closing them.
    if (i < 0 || m_notes[i].open == open || (m_quitting && !open)) {
        return;
    }
    m_notes[i].open = open;
    emit openIdsChanged();
    touch();
}

void NoteStore::closeAll()
{
    bool any = false;
    for (Note& n : m_notes) {
        any |= n.open;
        n.open = false;
    }
    if (any) {
        emit openIdsChanged();
        touch();
    }
}

void NoteStore::touch()
{
    m_dirty = true;
    m_timer.start(); // restarts: written once typing pauses
}

void NoteStore::flush()
{
    m_timer.stop();
    if (m_dirty && save()) {
        m_dirty = false;
    }
}

void NoteStore::rebuildView()
{
    beginResetModel();
    m_view.clear();
    for (int i = 0; i < m_notes.size(); ++i) {
        if (matches(m_notes[i].text, m_filter)) {
            m_view << i;
        }
    }
    endResetModel();
}

bool NoteStore::save()
{
    QJsonArray array;
    for (const Note& n : std::as_const(m_notes)) {
        array << QJsonObject { { "id", n.id }, { "text", n.text }, { "color", n.color }, { "x", n.x },
                               { "y", n.y }, { "width", n.width }, { "height", n.height },
                               { "open", n.open }, { "modified", n.modified } };
    }
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(QJsonObject { { "version", 1 }, { "notes", array } }).toJson(QJsonDocument::Compact));
    return file.commit();
}

void NoteStore::load()
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return; // none yet
    }
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    if (error.error != QJsonParseError::NoError || !doc.isObject() || !doc.object().value("notes").isArray()) {
        // Kept for a look; the next save must not overwrite it.
        QFile::remove(m_path + QStringLiteral(".bad"));
        file.rename(m_path + QStringLiteral(".bad"));
        return;
    }
    QSet<int> used;
    QList<Note> loose; // without a usable id, numbered after the others
    for (const QJsonValue& value : doc.object().value("notes").toArray()) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject o = value.toObject();
        Note n;
        n.id = o.value("id").toInt(0);
        n.text = o.value("text").toString();
        n.color = validColor(o.value("color").toString());
        n.x = o.value("x").toInt(0);
        n.y = o.value("y").toInt(0);
        n.width = std::max(o.value("width").toInt(300), kMinSize);
        n.height = std::max(o.value("height").toInt(300), kMinSize);
        n.open = o.value("open").toBool(false);
        n.modified = static_cast<qint64>(o.value("modified").toDouble(0));
        if (n.id <= 0 || used.contains(n.id)) {
            loose << n;
        } else {
            used.insert(n.id);
            m_notes << n;
        }
    }
    int next = 0;
    for (int id : std::as_const(used)) {
        next = std::max(next, id);
    }
    for (Note& n : loose) {
        n.id = ++next;
        m_notes << n;
    }
    std::stable_sort(m_notes.begin(), m_notes.end(), [](const Note& a, const Note& b) { return a.modified > b.modified; });
}
