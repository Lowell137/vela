// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// A list that's replaced every second without the view starting over: rows
// are known by their key, so set() only removes the ones that are gone, adds
// the new ones and moves the rest. Each row is a map (`row` in the delegate).

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QVariantMap>

class RowModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    struct Row {
        QString key;
        QVariantMap data;
    };

    using QAbstractListModel::QAbstractListModel;

    int count() const { return m_rows.size(); }
    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : m_rows.size(); }
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override { return { { Qt::UserRole, "row" } }; }

    void set(const QList<Row>& rows);
    const QList<Row>& rows() const { return m_rows; }

signals:
    void countChanged();

private:
    QList<Row> m_rows;
};
