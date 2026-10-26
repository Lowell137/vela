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
    // Farther from the top only the lower edge shows ("snap-drag peek"), as in
    // Windows; nearer ("show") the panel slides down in full.
    property bool peeking: false
    property bool appearPending: false
    Connections {
        target: root
        enabled: root.appearPending
        function onFrameSwapped() {
            root.appearPending = false
            appear.restart()
        }
    }
    readonly property real peekY: -panel.height + 12 * s
    readonly property real targetY: peeking ? peekY : geometry.y

    Connections {
        target: Shell
        function onSnapDragChanged(state, name, a, b, json) {
            if (state === "show" || state === "peek") {
                root.show(name, json, state === "peek")
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
    property string placedOutput: ""
    function show(output, json, asPeek) {
        // On another output than the last time (not "than root.screen": that
        // is never the same object as the screen of the list).
        const screen = output !== placedOutput ? Qt.application.screens.find(s => s.name === output) : null
        if (screen) {
            // Only while hidden: hiding and showing again at once breaks the
            // layer surface's configure handshake ("wrong configure serial").
            if (root.visible) {
                root.visible = false
                Qt.callLater(root.show, output, json, asPeek)
                return
            }
            Shell.placeOnScreen(root, screen.name)
            placedOutput = output
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
        peeking = asPeek
        disappear.stop()
        if (!visible) {
            // Slides down from above the output, once the window is on screen:
            // the first frames of a layer surface come late, the slide would
            // be half over.
            panel.y = -panel.height
            appearPending = true
        }
        visible = true
        if (visible && !appearPending && (!wasShown || panel.y !== root.targetY)) {
            appear.restart() // from wherever it is: half gone, it comes back
        }
        updateBlur()
    }
    function hide() {
        if (!shown) {
            return
        }
        shown = false
        appearPending = false
        appear.stop()
        disappear.restart()
    }
    function updateBlur() {
        if (visible) {
            // Where the panel is now, the part on the output: the blur goes down with it
            const top = Math.max(0, panel.y)
            const height = panel.y + panel.height - top
            Effects.setBlur(root, height > 0 ? [Qt.rect(panel.x, top, panel.width, height)] : [], panel.radius)
        }
    }

    NumberAnimation {
        id: appear
        target: panel; property: "y"; to: root.targetY
        // Windows 11's, from a recording: the strip in ~100 ms, the panel from it in ~230 ms
        duration: root.peeking ? 120 : 230
        easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate
    }
    SequentialAnimation {
        id: disappear
        NumberAnimation { target: panel; property: "y"; to: -panel.height; duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.accelerate }
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
        onYChanged: root.updateBlur()
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
