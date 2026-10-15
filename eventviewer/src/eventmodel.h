// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The entries, newest first, at most `cap` of them and each cursor once (a
// follow that starts where the load ended sees a few twice). Entries come in
// batches so the view moves once per batch and keeps its place. Roles: "row"
// is the entry's map; the others are what the header sorts by.

#include "journal.h"

#include <QAbstractListModel>
#include <QList>
#include <QSet>

class EventModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role { RowRole = Qt::UserRole, TimeRole, LevelRole, SourceRole, MessageRole };

    explicit EventModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int count() const { return m_entries.size(); }
    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : m_entries.size(); }
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int cap() const { return m_cap; }
    void setCap(int cap) { m_cap = qMax(1, cap); }
    const QList<journal::Entry>& entries() const { return m_entries; }

    void clear();
    // Older entries after the ones there; returns true when some were left
    // out because of the cap.
    bool appendOlder(const QList<journal::Entry>& entries);
    // Newer entries, oldest first as they come, put on top; the oldest go
    // when it's over the cap.
    void prependNewer(const QList<journal::Entry>& entries);

signals:
    void countChanged();

private:
    QList<journal::Entry> m_entries;
    QSet<QString> m_cursors;
    int m_cap = 20000;
};
