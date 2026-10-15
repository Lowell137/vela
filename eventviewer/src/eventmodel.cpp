// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "eventmodel.h"

QVariant EventModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size()) {
        return {};
    }
    const journal::Entry& entry = m_entries.at(index.row());
    switch (role) {
    case RowRole: return entry.toMap();
    case TimeRole: return entry.usec;
    case LevelRole: return entry.priority;
    case SourceRole: return entry.source();
    case MessageRole: return entry.message;
    }
    return {};
}

QHash<int, QByteArray> EventModel::roleNames() const
{
    return { { RowRole, "row" } };
}

void EventModel::clear()
{
    if (m_entries.isEmpty()) {
        return;
    }
    beginResetModel();
    m_entries.clear();
    m_cursors.clear();
    endResetModel();
    emit countChanged();
}

bool EventModel::appendOlder(const QList<journal::Entry>& entries)
{
    QList<journal::Entry> fresh;
    bool capped = false;
    for (const journal::Entry& entry : entries) {
        if (m_cursors.contains(entry.cursor)) {
            continue;
        }
        if (m_entries.size() + fresh.size() >= m_cap) {
            capped = true;
            break;
        }
        m_cursors.insert(entry.cursor);
        fresh.append(entry);
    }
    if (!fresh.isEmpty()) {
        beginInsertRows({}, m_entries.size(), m_entries.size() + fresh.size() - 1);
        m_entries += fresh;
        endInsertRows();
        emit countChanged();
    }
    return capped;
}

void EventModel::prependNewer(const QList<journal::Entry>& entries)
{
    QList<journal::Entry> fresh; // newest first
    for (const journal::Entry& entry : entries) {
        if (!m_cursors.contains(entry.cursor)) {
            m_cursors.insert(entry.cursor);
            fresh.prepend(entry);
        }
    }
    if (fresh.isEmpty()) {
        return;
    }
    beginInsertRows({}, 0, fresh.size() - 1);
    m_entries = fresh + m_entries;
    endInsertRows();
    const int extra = m_entries.size() - m_cap;
    if (extra > 0) {
        beginRemoveRows({}, m_entries.size() - extra, m_entries.size() - 1);
        for (int i = 0; i < extra; i++) {
            m_cursors.remove(m_entries.takeLast().cursor);
        }
        endRemoveRows();
    }
    emit countChanged();
}
