// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Windows 11 snap layouts while dragging a window to the top: a pill at the
// top center with the arrangements in one row. The compositor has the pointer
// during the drag: it places the pill, finds the zone under the cursor and
// snaps the window on release (snap.c); this only draws. Its sizes and
// layouts must match snap.c's DRAG_* and drag_layouts.
Window {
    id: root
    visible: false
    color: "transparent"

    readonly property var layouts: [
        [[0, 0, 6, 12], [6, 0, 12, 12]],
        [[0, 0, 8, 12], [8, 0, 12, 12]],
        [[0, 0, 4, 12], [4, 0, 12, 12]],
        [[0, 0, 4, 12], [4, 0, 8, 12], [8, 0, 12, 12]],
        [[0, 0, 6, 12], [6, 0, 12, 6], [6, 6, 12, 12]],
        [[0, 0, 6, 6], [6, 0, 12, 6], [0, 6, 6, 12], [6, 6, 12, 12]],
        [[0, 0, 3, 12], [3, 0, 9, 12], [9, 0, 12, 12]],
        [[0, 0, 6, 6], [0, 6, 6, 12], [6, 0, 12, 12]]
    ]
    property real panelX: 0
    property real panelY: 0
    property int hoverLayout: -1
    property int hoverZone: -1
    property bool shown: false // false while it slides away

    Connections {
        target: Shell
        function onSnapDragChanged(state, name, a, b) {
            if (state === "show") {
                root.show(name, a, b)
            } else if (state === "hover") {
                root.hoverLayout = a
                root.hoverZone = b
            } else {
                // Dropped on a zone: Snap Assist, coming next, offers the
                // layout's other zones (SnapAssist.freeZones).
                if (state === "drop" && a >= 0 && a < root.layouts.length) {
                    Menus.snapLayout = { window: name, zones: root.layouts[a] }
                }
                root.hide()
            }
        }
    }
    function show(output, x, y) {
        const screen = Qt.application.screens.find(s => s.name === output)
        if (screen && screen !== root.screen) {
            // Only while hidden: hiding and showing again at once breaks the
            // layer surface's configure handshake ("wrong configure serial").
            if (root.visible) {
                root.visible = false
                Qt.callLater(root.show, output, x, y)
                return
            }
            Shell.placeOnScreen(root, screen.name)
        }
        panelX = x
        panelY = y
        hoverLayout = -1
        hoverZone = -1
        shown = true
        if (!visible) {
            panel.opacity = 0
            panel.y = panelY - 12
        }
        visible = true
        disappear.stop()
        appear.restart()
        updateBlur()
    }
    function hide() {
        if (!shown) {
            return
        }
        shown = false
        appear.stop()
        disappear.restart()
    }
    function updateBlur() {
        if (visible) {
            Effects.setBlur(root, [Qt.rect(panel.x, root.panelY, panel.width, panel.height)])
        }
    }

    ParallelAnimation {
        id: appear
        NumberAnimation { target: panel; property: "opacity"; to: 1; duration: Theme.normal }
        NumberAnimation { target: panel; property: "y"; to: root.panelY; duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
    }
    SequentialAnimation {
        id: disappear
        ParallelAnimation {
            NumberAnimation { target: panel; property: "opacity"; to: 0; duration: Theme.normal }
            NumberAnimation { target: panel; property: "y"; to: root.panelY - 12; duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.accelerate }
        }
        ScriptAction { script: if (!root.shown) root.visible = false }
    }

    Rectangle {
        id: panel
        x: root.panelX
        width: row.width + 32 // snap.c: DRAG_PAD_X
        height: row.height + 16 // DRAG_PAD_Y
        radius: height / 2
        color: Theme.popup
        border.width: 1
        border.color: Theme.stroke

        Row {
            id: row
            x: 16
            y: 8
            spacing: 8 // DRAG_GAP

            Repeater {
                model: root.layouts
                delegate: Rectangle {
                    id: layout
                    required property var modelData
                    required property int index
                    width: 64 // DRAG_CARD_W, DRAG_CARD_H
                    height: 40
                    radius: Theme.radius
                    color: Theme.light ? Qt.rgba(0, 0, 0, 0.03) : Qt.rgba(1, 1, 1, 0.05)
                    border.width: 1
                    border.color: Theme.stroke

                    Repeater {
                        model: layout.modelData
                        delegate: Rectangle {
                            id: zone
                            required property var modelData
                            required property int index
                            readonly property real unitX: (layout.width - 6) / 12 // DRAG_INSET
                            readonly property real unitY: (layout.height - 6) / 12
                            readonly property bool hovered: root.hoverLayout === layout.index && root.hoverZone === zone.index
                            x: 3 + modelData[0] * unitX + 1
                            y: 3 + modelData[1] * unitY + 1
                            width: (modelData[2] - modelData[0]) * unitX - 2
                            height: (modelData[3] - modelData[1]) * unitY - 2
                            radius: 2
                            color: hovered ? Theme.accent : Theme.light ? Qt.rgba(0, 0, 0, 0.16) : Qt.rgba(1, 1, 1, 0.22)
                            Behavior on color { ColorAnimation { duration: Theme.fast } }
                        }
                    }
                }
            }
        }
    }
}
