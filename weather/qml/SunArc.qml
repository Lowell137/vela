// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The day's sun path: a half circle from the sunrise (left) to the sunset
// (right), the part already gone filled, the sun where it is now.
Canvas {
    id: arc
    property real progress: -1 // below 0 before the sunrise, above 1 after the sunset
    property color lineColor: Theme.textTertiary
    property color sunColor: Theme.accentFill
    implicitHeight: 70

    onProgressChanged: requestPaint()
    onLineColorChanged: requestPaint()
    onSunColorChanged: requestPaint()
    onWidthChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const r = Math.min(width / 2 - 8, height - 10)
        const cx = width / 2, cy = height - 6
        ctx.lineWidth = 2
        ctx.strokeStyle = lineColor
        ctx.setLineDash([3, 4])
        ctx.beginPath()
        ctx.arc(cx, cy, r, Math.PI, 2 * Math.PI)
        ctx.stroke()
        ctx.setLineDash([])
        ctx.beginPath()
        ctx.moveTo(cx - r - 8, cy)
        ctx.lineTo(cx + r + 8, cy)
        ctx.stroke()
        const p = Math.max(0, Math.min(1, progress))
        if (progress > 0) {
            ctx.strokeStyle = sunColor
            ctx.beginPath()
            ctx.arc(cx, cy, r, Math.PI, Math.PI + p * Math.PI)
            ctx.stroke()
        }
        if (progress >= 0 && progress <= 1) {
            const a = Math.PI + p * Math.PI
            ctx.fillStyle = sunColor
            ctx.beginPath()
            ctx.arc(cx + r * Math.cos(a), cy + r * Math.sin(a), 6, 0, 2 * Math.PI)
            ctx.fill()
        }
    }
}
