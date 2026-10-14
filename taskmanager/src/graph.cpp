// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "graph.h"

#include <QPainter>
#include <QPainterPath>

Graph::Graph(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &Graph::changed, this, [this] { update(); });
}

void Graph::paint(QPainter* painter)
{
    const double w = width();
    const double h = height();
    QColor faint = m_color;
    faint.setAlphaF(0.25);
    if (m_grid) {
        painter->setPen(QPen(faint, 1));
        for (int i = 1; i < 4; i++) {
            painter->drawLine(QPointF(0, h * i / 4), QPointF(w, h * i / 4));
        }
        for (int i = 1; i < 6; i++) {
            painter->drawLine(QPointF(w * i / 6, 0), QPointF(w * i / 6, h));
        }
    }
    double top = m_ceiling;
    if (top <= 0) {
        for (const QVariant& value : std::as_const(m_values)) {
            top = std::max(top, value.toDouble());
        }
        top = std::max(top, 1.0);
    }
    if (!m_values.isEmpty()) {
        QPainterPath line;
        const qsizetype count = m_values.size();
        for (qsizetype i = 0; i < count; i++) {
            const double x = w * (samples - count + i) / (samples - 1);
            const double y = h - std::min(1.0, m_values.at(i).toDouble() / top) * (h - 1);
            i == 0 ? line.moveTo(x, y) : line.lineTo(x, y);
        }
        QPainterPath area = line;
        area.lineTo(w, h);
        area.lineTo(w * (samples - count) / (samples - 1), h);
        area.closeSubpath();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->fillPath(area, faint);
        painter->setPen(QPen(m_color, 1.5));
        painter->drawPath(line);
    }
    if (m_grid) {
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setPen(QPen(m_color, 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(QRectF(0.5, 0.5, w - 1, h - 1));
    }
}
