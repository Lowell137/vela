// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Windows 11 snap layouts while dragging a window to the top: a pill at the
// top center with the arrangements in one row. The compositor has the pointer
// during the drag: it places the pill, finds the zone under the cursor and
// snaps the window on release (snap.c); this only draws, with the sizes and
// layouts it sends, so that what is seen is what it hits.
Window {
    id: root
    visible: false
    color: "transparent"

    // From "snap-drag show": x, y (the pill's top left), scale, card [w, h],
    // gap, pad [x, y], inset, layouts (zones in twelfths).
    property var geometry: ({ x: 0, y: 0, scale: 1, card: [64, 40], gap: 8, pad: [16, 8], inset: 3, layouts: [] })
    readonly property real s: geometry.scale
    property int hoverLayout: -1
    property int hoverZone: -1
    property bool shown: false // false while it slides away

    Connections {
        target: Shell
        function onSnapDragChanged(state, name, a, b, json) {
            if (state === "show") {
                root.show(name, json)
            } else if (state === "hover") {
                root.hoverLayout = a
                root.hoverZone = b
            } else {
                // Dropped on a zone: Snap Assist, coming next, offers the
                // layout's other zones (SnapAssist.freeZones).
                if (state === "drop" && a >= 0 && a < root.geometry.layouts.length) {
                    Menus.snapLayout = { window: name, zones: root.geometry.layouts[a] }
                }
                root.hide()
            }
        }
    }
    property string geometryJson: ""
    function show(output, json) {
        const screen = Qt.application.screens.find(s => s.name === output)
        if (screen && screen !== root.screen) {
            // Only while hidden: hiding and showing again at once breaks the
            // layer surface's configure handshake ("wrong configure serial").
            if (root.visible) {
                root.visible = false
                Qt.callLater(root.show, output, json)
                return
            }
            Shell.placeOnScreen(root, screen.name)
        }
        // Only when it changes: new layouts rebuild the cards, which the Row
        // places a frame later (a pill drawn half empty).
        if (json !== geometryJson) {
            geometryJson = json
            root.geometry = JSON.parse(json)
        }
        hoverLayout = -1
        hoverZone = -1
        const wasShown = shown
        shown = true
        disappear.stop()
        if (!visible) {
            // Slides down from just above where it stops, fading in.
            panel.opacity = 0
            panel.y = root.geometry.y - 8 * root.s
        }
        visible = true
        if (!wasShown || panel.opacity < 1 || panel.y !== root.geometry.y) {
            appear.restart() // from wherever it is: half gone, it comes back
        }
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
            Effects.setBlur(root, [Qt.rect(panel.x, root.geometry.y, panel.width, panel.height)], panel.radius)
        }
    }

    ParallelAnimation {
        id: appear
        NumberAnimation { target: panel; property: "opacity"; to: 1; duration: Theme.normal }
        NumberAnimation { target: panel; property: "y"; to: root.geometry.y; duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
    }
    SequentialAnimation {
        id: disappear
        ParallelAnimation {
            NumberAnimation { target: panel; property: "opacity"; to: 0; duration: Theme.normal }
            NumberAnimation { target: panel; property: "y"; to: root.geometry.y - 8 * root.s; duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.accelerate }
        }
        ScriptAction { script: if (!root.shown) root.visible = false }
    }

    Rectangle {
        id: panel
        // From the geometry, not from the Row: right from the first frame, for
        // the blur too.
        readonly property int count: root.geometry.layouts.length
        x: root.geometry.x
        width: (count * root.geometry.card[0] + Math.max(count - 1, 0) * root.geometry.gap + 2 * root.geometry.pad[0]) * root.s
        height: (root.geometry.card[1] + 2 * root.geometry.pad[1]) * root.s
        onXChanged: root.updateBlur()
        onWidthChanged: root.updateBlur()
        onHeightChanged: root.updateBlur()
        radius: Theme.radiusOverlay * root.s
        color: Theme.popup
        border.width: 1
        border.color: Theme.stroke

        // Placed by hand, as snap.c hits them: no Row to wait for.
        Repeater {
            model: root.geometry.layouts
            delegate: Rectangle {
                id: layout
                required property var modelData
                required property int index
                readonly property real inset: root.geometry.inset * root.s
                x: (root.geometry.pad[0] + index * (root.geometry.card[0] + root.geometry.gap)) * root.s
                y: root.geometry.pad[1] * root.s
                width: root.geometry.card[0] * root.s
                height: root.geometry.card[1] * root.s
                radius: Theme.radius * root.s
                color: Theme.light ? Qt.rgba(0, 0, 0, 0.03) : Qt.rgba(1, 1, 1, 0.05)
                border.width: 1
                border.color: Theme.stroke

                Repeater {
                    model: layout.modelData
                    delegate: Rectangle {
                        id: zone
                        required property var modelData
                        required property int index
                        readonly property real unitX: (layout.width - 2 * layout.inset) / 12
                        readonly property real unitY: (layout.height - 2 * layout.inset) / 12
                        readonly property bool hovered: root.hoverLayout === layout.index && root.hoverZone === zone.index
                        x: layout.inset + modelData[0] * unitX + 1
                        y: layout.inset + modelData[1] * unitY + 1
                        width: (modelData[2] - modelData[0]) * unitX - 2
                        height: (modelData[3] - modelData[1]) * unitY - 2
                        radius: 2 * root.s
                        color: hovered ? Theme.accent : Theme.light ? Qt.rgba(0, 0, 0, 0.16) : Qt.rgba(1, 1, 1, 0.22)
                        Behavior on color { ColorAnimation { duration: Theme.fast } }
                    }
                }
            }
        }
    }
}
