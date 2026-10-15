// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The crosshair's mark (Crosshair.qml, and GameBar.qml while it's dragged):
// a dot, a cross, a cross with a gap, a circle or an image, with an outline
// so it shows on any background. Settings are the gamebar/crosshair-* keys.
Item {
    id: mark
    readonly property var settings: Config.gameBar
    readonly property string style: settings["crosshair-style"] ?? "cross"
    readonly property int size: Math.max(2, Math.min(128, settings["crosshair-size"] ?? 24))
    readonly property real thickness: Math.max(1, Math.min(16, settings["crosshair-thickness"] ?? 2))
    readonly property real gap: Math.max(0, Math.min(64, settings["crosshair-gap"] ?? 4))
    readonly property int dx: Math.max(-3000, Math.min(3000, settings["crosshair-x"] ?? 0))
    readonly property int dy: Math.max(-3000, Math.min(3000, settings["crosshair-y"] ?? 0))
    readonly property bool outline: settings["crosshair-outline"] ?? true
    readonly property color tint: settings["crosshair-color"] ?? "#00ff66"
    readonly property string image: settings["crosshair-image"] ?? ""
    // Which output it's on (the main one if that is gone).
    readonly property string screenName: {
        const name = String(settings["crosshair-screen"] ?? "")
        return Qt.application.screens.some(s => s.name === name) ? name : Shell.primaryScreen
    }

    width: size + 4
    height: size + 4
    opacity: Math.max(0.1, Math.min(1, settings["crosshair-opacity"] ?? 1))

    Image {
        visible: mark.style === "image" && mark.image !== ""
        anchors.fill: parent
        source: visible ? "file://" + mark.image : ""
        sourceSize: Qt.size(width, height)
        fillMode: Image.PreserveAspectFit
    }

    Canvas {
        id: canvas
        visible: mark.style !== "image"
        anchors.fill: parent
        Connections {
            target: mark
            function onSettingsChanged() { canvas.requestPaint() }
        }
        onWidthChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const c = width / 2
            const half = mark.size / 2
            const t = mark.thickness
            // The pieces as rectangles [x, y, w, h], drawn twice: the
            // outline one pixel larger in black, then the color.
            let bars = []
            if (mark.style === "dot") {
                bars = [[c - t, c - t, 2 * t, 2 * t]]
            } else if (mark.style === "cross" || mark.style === "gap") {
                const g = mark.style === "gap" ? mark.gap : 0
                const len = half - g
                if (g === 0) {
                    bars = [[c - half, c - t / 2, mark.size, t], [c - t / 2, c - half, t, mark.size]]
                } else {
                    bars = [[c - half, c - t / 2, len, t], [c + g, c - t / 2, len, t],
                            [c - t / 2, c - half, t, len], [c - t / 2, c + g, t, len]]
                }
            }
            if (mark.style === "circle") {
                for (const [color, w] of [["black", t + 2], [mark.tint, t]]) {
                    if (color === "black" && !mark.outline) continue
                    ctx.beginPath()
                    ctx.arc(c, c, half - t / 2 - 1, 0, 2 * Math.PI)
                    ctx.lineWidth = w
                    ctx.strokeStyle = color
                    ctx.stroke()
                }
                return
            }
            if (mark.outline) {
                ctx.fillStyle = "black"
                for (const b of bars) ctx.fillRect(b[0] - 1, b[1] - 1, b[2] + 2, b[3] + 2)
            }
            ctx.fillStyle = mark.tint
            for (const b of bars) ctx.fillRect(b[0], b[1], b[2], b[3])
        }
    }
}
