// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

// Mirrored bars of peaks (0..1), one pixel column each; the player draws it
// twice, in two colors, one clipped to the played part.
Canvas {
    id: wave
    property var peaks: []
    property color color: "gray"
    // shortcut: repaints on resize and new peaks only; ~800 values, cheap.
    onPeaksChanged: requestPaint()
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const n = peaks.length
        if (n === 0 || width <= 0) return
        ctx.fillStyle = color
        const mid = height / 2
        const step = 3
        for (let x = 0; x < width; x += step) {
            const i = Math.min(n - 1, Math.floor(x * n / width))
            // The loudest of the values this bar covers, so a click is not lost.
            let peak = 0
            const end = Math.min(n, Math.max(i + 1, Math.floor((x + step) * n / width)))
            for (let k = i; k < end; k++) peak = Math.max(peak, peaks[k])
            const h = Math.max(2, Math.sqrt(peak) * (height - 4))
            ctx.fillRect(x, mid - h / 2, 2, h)
        }
    }
}
