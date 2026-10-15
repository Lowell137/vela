// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

// The Game Bar's crosshair (Win+Alt+C, Settings > Gaming): in the center of
// the main output, moved by gamebar/crosshair-x and -y; a dot, a cross, a
// cross with a gap, a circle or an image, with an outline so it shows on
// any background. Click-through. The window is centered by the compositor:
// it grows by twice the offset and the mark is drawn off its center.
Window {
    id: root
    visible: false
    color: "transparent"

    readonly property var settings: Config.gameBar
    readonly property bool on: settings.crosshair === true
    readonly property string style: settings["crosshair-style"] ?? "cross"
    readonly property int size: Math.max(2, Math.min(128, settings["crosshair-size"] ?? 24))
    readonly property real thickness: Math.max(1, Math.min(16, settings["crosshair-thickness"] ?? 2))
    readonly property real gap: Math.max(0, Math.min(64, settings["crosshair-gap"] ?? 4))
    readonly property int dx: Math.max(-400, Math.min(400, settings["crosshair-x"] ?? 0))
    readonly property int dy: Math.max(-400, Math.min(400, settings["crosshair-y"] ?? 0))
    readonly property bool outline: settings["crosshair-outline"] ?? true
    readonly property color tint: settings["crosshair-color"] ?? "#00ff66"
    readonly property string image: settings["crosshair-image"] ?? ""

    width: size + 4 + 2 * Math.abs(dx)
    height: size + 4 + 2 * Math.abs(dy)

    Connections {
        target: Shell
        function onCrosshairToggleRequested() { Config.setGameBar("crosshair", !root.on) }
    }
    function sync() {
        if (on && !visible) Shell.placeOnScreen(root, Shell.primaryScreen)
        visible = on
    }
    onOnChanged: sync()
    Component.onCompleted: Qt.callLater(sync)

    Item {
        id: mark
        width: root.size + 4
        height: root.size + 4
        x: (root.width - width) / 2 + root.dx
        y: (root.height - height) / 2 + root.dy
        opacity: Math.max(0.1, Math.min(1, root.settings["crosshair-opacity"] ?? 1))

        Image {
            visible: root.style === "image" && root.image !== ""
            anchors.fill: parent
            source: visible ? "file://" + root.image : ""
            sourceSize: Qt.size(width, height)
            fillMode: Image.PreserveAspectFit
        }

        Canvas {
            id: canvas
            visible: root.style !== "image"
            anchors.fill: parent
            Connections {
                target: root
                function onSettingsChanged() { canvas.requestPaint() }
            }
            onWidthChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                const c = width / 2
                const half = root.size / 2
                const t = root.thickness
                // The pieces as rectangles [x, y, w, h], drawn twice: the
                // outline one pixel larger in black, then the color.
                let bars = []
                if (root.style === "dot") {
                    bars = [[c - t, c - t, 2 * t, 2 * t]]
                } else if (root.style === "cross" || root.style === "gap") {
                    const g = root.style === "gap" ? root.gap : 0
                    const len = half - g
                    if (g === 0) {
                        bars = [[c - half, c - t / 2, root.size, t], [c - t / 2, c - half, t, root.size]]
                    } else {
                        bars = [[c - half, c - t / 2, len, t], [c + g, c - t / 2, len, t],
                                [c - t / 2, c - half, t, len], [c - t / 2, c + g, t, len]]
                    }
                }
                if (root.style === "circle") {
                    for (const [color, w] of [["black", t + 2], [root.tint, t]]) {
                        if (color === "black" && !root.outline) continue
                        ctx.beginPath()
                        ctx.arc(c, c, half - t / 2 - 1, 0, 2 * Math.PI)
                        ctx.lineWidth = w
                        ctx.strokeStyle = color
                        ctx.stroke()
                    }
                    return
                }
                if (root.outline) {
                    ctx.fillStyle = "black"
                    for (const b of bars) ctx.fillRect(b[0] - 1, b[1] - 1, b[2] + 2, b[3] + 2)
                }
                ctx.fillStyle = root.tint
                for (const b of bars) ctx.fillRect(b[0], b[1], b[2], b[3])
            }
        }
    }
}
