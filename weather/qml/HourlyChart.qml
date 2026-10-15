// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The next hours' temperature as a line, with the chance of precipitation as
// bars under it. One column of `step` pixels per hour, like the strip below.
Canvas {
    id: chart
    property var temps: [] // in the chosen unit
    property var chances: [] // percent, -1: none
    property real step: 64
    property color lineColor: Theme.accentFill
    property color barColor: Theme.accentFill
    property color gridColor: Theme.divider

    width: temps.length * step
    height: 110
    onTempsChanged: requestPaint()
    onChancesChanged: requestPaint()
    onLineColorChanged: requestPaint()
    onGridColorChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const n = temps.length
        if (n === 0) return
        const top = 12, bottom = height - 28 // the line's band; the bars are under it
        const lo = Math.min(...temps), hi = Math.max(...temps)
        const span = Math.max(1, hi - lo)
        const x = i => (i + 0.5) * step
        const y = i => bottom - (temps[i] - lo) / span * (bottom - top)

        ctx.strokeStyle = gridColor
        ctx.lineWidth = 1
        ctx.beginPath()
        ctx.moveTo(0, height - 0.5)
        ctx.lineTo(width, height - 0.5)
        ctx.stroke()

        ctx.fillStyle = Qt.rgba(barColor.r, barColor.g, barColor.b, 0.35)
        for (let i = 0; i < n; ++i) {
            const c = chances[i]
            if (c === undefined || c <= 0) continue
            const h = Math.max(2, c / 100 * 22)
            ctx.fillRect(x(i) - 8, height - h, 16, h)
        }

        ctx.strokeStyle = lineColor
        ctx.fillStyle = lineColor
        ctx.lineWidth = 2
        ctx.lineJoin = "round"
        ctx.beginPath()
        for (let i = 0; i < n; ++i) i === 0 ? ctx.moveTo(x(i), y(i)) : ctx.lineTo(x(i), y(i))
        ctx.stroke()
        for (let i = 0; i < n; ++i) {
            ctx.beginPath()
            ctx.arc(x(i), y(i), 3, 0, 2 * Math.PI)
            ctx.fill()
        }
    }
}
