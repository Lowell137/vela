// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// A resource's last minute, as the line Windows' Task Manager draws
// (`Graph` in QML): the newest value at the right, filled underneath.

#include <QColor>
#include <QQuickPaintedItem>
#include <QVariantList>

class Graph : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QVariantList values MEMBER m_values NOTIFY changed)
    Q_PROPERTY(double ceiling MEMBER m_ceiling NOTIFY changed) // the top of the scale; 0: the largest value
    Q_PROPERTY(QColor color MEMBER m_color NOTIFY changed)
    Q_PROPERTY(bool grid MEMBER m_grid NOTIFY changed)

public:
    explicit Graph(QQuickItem* parent = nullptr);
    void paint(QPainter* painter) override;

    static constexpr int samples = 60;

signals:
    void changed();

private:
    QVariantList m_values;
    double m_ceiling = 100;
    QColor m_color = Qt::cyan;
    bool m_grid = true;
};
