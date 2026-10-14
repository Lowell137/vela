// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "rowmodel.h"

#include <QSet>

QVariant RowModel::data(const QModelIndex& index, int role) const
{
    return role == Qt::UserRole && index.row() >= 0 && index.row() < m_rows.size()
        ? QVariant(m_rows.at(index.row()).data) : QVariant();
}

void RowModel::set(const QList<Row>& rows)
{
    const int before = m_rows.size();
    QSet<QString> wanted;
    for (const Row& row : rows) {
        wanted.insert(row.key);
    }
    for (int i = m_rows.size() - 1; i >= 0; i--) {
        if (!wanted.contains(m_rows.at(i).key)) {
            beginRemoveRows({}, i, i);
            m_rows.removeAt(i);
            endRemoveRows();
        }
    }
    QSet<QString> present;
    for (const Row& row : std::as_const(m_rows)) {
        present.insert(row.key);
    }
    for (const Row& row : rows) {
        if (!present.contains(row.key)) {
            beginInsertRows({}, m_rows.size(), m_rows.size());
            m_rows.append(row);
            endInsertRows();
        }
    }
    // The same rows now, perhaps in another order and with other values.
    bool moved = false;
    for (int i = 0; i < rows.size() && !moved; i++) {
        moved = rows.at(i).key != m_rows.at(i).key;
    }
    if (moved) {
        emit layoutAboutToBeChanged();
        m_rows = rows;
        emit layoutChanged();
    } else {
        m_rows = rows;
    }
    if (!m_rows.isEmpty()) {
        emit dataChanged(index(0), index(m_rows.size() - 1));
    }
    if (before != m_rows.size()) {
        emit countChanged();
    }
}
