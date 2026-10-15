// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// A small line graph of the last values (the Game Bar's FPS and frame time),
// scaled to the highest one; redrawn only when the values change.
Canvas {
    id: graph
    property var values: []
    property color color: Theme.accent
    implicitWidth: 200
    implicitHeight: 40
    onValuesChanged: requestPaint()
    onColorChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const n = values.length
        if (n < 2) return
        const top = Math.max(1, ...values) * 1.1
        const step = width / (n - 1)
        ctx.beginPath()
        for (let i = 0; i < n; i++) {
            const y = height - 1 - (height - 2) * Math.max(0, values[i]) / top
            if (i === 0) ctx.moveTo(0, y)
            else ctx.lineTo(i * step, y)
        }
        ctx.strokeStyle = graph.color
        ctx.lineWidth = 1.5
        ctx.stroke()
        ctx.lineTo(width, height)
        ctx.lineTo(0, height)
        ctx.closePath()
        ctx.fillStyle = Qt.rgba(graph.color.r, graph.color.g, graph.color.b, 0.15)
        ctx.fill()
    }
}
