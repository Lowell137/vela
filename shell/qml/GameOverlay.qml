// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The Game Bar's pinned numbers, always on top in a corner of the main
// output (gamebar/corner), as a column or a row, with an optional frame time
// graph. Translucent and click-through; it exists only while something is
// pinned and "Show overlay" is on, and follows the 1 s sampling.
// Over a fullscreen game any overlay surface can stop direct scanout (the
// game is composited again): MangoHud, drawn inside the game, doesn't.
Window {
    id: root
    visible: false
    color: "transparent"
    width: box.width
    height: box.height

    readonly property var settings: Config.gameBar
    readonly property real zoom: Math.max(0.5, Math.min(2, settings.scale ?? 1))
    readonly property bool horizontal: settings.layout === "row"

    function sync() {
        if (!GameNumbers.overlayShown) {
            visible = false
            return
        }
        if (!visible) Shell.placeOnScreen(root, Shell.primaryScreen)
        Shell.placeInCorner(root, settings.corner ?? "top-left", 12)
        visible = true
    }
    Component.onCompleted: Qt.callLater(sync)
    Connections {
        target: GameNumbers
        function onOverlayShownChanged() { root.sync() }
    }
    onSettingsChanged: if (visible) sync()

    Rectangle {
        id: box
        width: column.width + 16 * root.zoom
        height: column.height + 12 * root.zoom
        radius: Theme.radiusOverlay
        color: Qt.rgba(0, 0, 0, Math.max(0, Math.min(1, root.settings.opacity ?? 0.6)))

        Column {
            id: column
            anchors.centerIn: parent
            spacing: 4 * root.zoom
            Grid {
                id: grid
                flow: root.horizontal ? Grid.TopToBottom : Grid.LeftToRight
                rows: root.horizontal ? 2 : -1
                columns: root.horizontal ? -1 : 2
                columnSpacing: 10 * root.zoom
                rowSpacing: 2 * root.zoom
                Repeater {
                    model: GameNumbers.pinned.reduce((list, k) => list.concat([{ key: k, label: true }, { key: k, label: false }]), [])
                    delegate: Text {
                        required property var modelData
                        text: modelData.label ? GameNumbers.label(modelData.key) : GameNumbers.value(modelData.key)
                        color: modelData.label ? "#c8c8c8" : "white"
                        font.pixelSize: (modelData.label ? 11 : 13) * root.zoom
                        font.weight: modelData.label ? Font.Normal : Font.DemiBold
                        font.features: { "tnum": 1 }
                    }
                }
            }
            Sparkline {
                visible: root.settings.graph === true
                width: Math.max(grid.width, 120 * root.zoom)
                height: 28 * root.zoom
                values: visible ? Game.frameHistory : []
                color: "#ffd84d"
            }
        }
    }
}
