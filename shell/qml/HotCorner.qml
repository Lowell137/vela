// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The hot corner (interact.c): a soft glow in the top-left corner while the
// pointer is near it, and a ring growing out of it when it opens Task View.
// The compositor sends nothing when the corner is off ("hot-corner=no"), while
// dragging a window or over a fullscreen one.
Window {
    id: root
    visible: false
    width: 160
    height: 160
    color: "transparent"

    property bool near: false

    Connections {
        target: Shell
        function onHotCornerChanged(state, output) {
            if (state === "away") {
                root.near = false
                return
            }
            // Only while hidden: hiding and showing again at once breaks the
            // layer surface's configure handshake ("wrong configure serial").
            if (!root.visible) {
                Shell.placeOnScreen(root, output)
            }
            root.visible = true
            root.near = state === "near"
            if (state === "fire") {
                pulse.restart()
            }
        }
    }

    // Gone once nothing shows: a hidden panel gives its surface back.
    function hideIfIdle() {
        if (!near && glow.opacity === 0 && !pulse.running) {
            visible = false
        }
    }

    Item {
        id: glow
        anchors.fill: parent
        opacity: root.near ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate } }
        onOpacityChanged: root.hideIfIdle()

        // Quarter circles: centered on the corner, the rest is off the output.
        Rectangle {
            x: -width / 2
            y: -height / 2
            width: 112
            height: 112
            radius: width / 2
            color: Theme.accent
            opacity: 0.16
        }
        Rectangle {
            x: -width / 2
            y: -height / 2
            width: 56
            height: 56
            radius: width / 2
            color: Theme.accent
            opacity: 0.32
        }
    }

    Rectangle {
        id: ring
        x: -width / 2
        y: -height / 2
        width: 2 * root.width - 8
        height: width
        radius: width / 2
        color: "transparent"
        border.width: 3
        border.color: Theme.accent
        opacity: 0
    }

    // As long as Task View's "appear", so the two end together.
    ParallelAnimation {
        id: pulse
        NumberAnimation { target: ring; property: "scale"; from: 0.2; to: 1; duration: Theme.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        NumberAnimation { target: ring; property: "opacity"; from: 0.9; to: 0; duration: Theme.slow }
        onFinished: root.hideIfIdle()
    }
}
