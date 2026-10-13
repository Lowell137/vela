// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The hot corner (interact.c): a soft glow in the top-left corner while the
// pointer is near it, and two small ripples when it opens Task View.
// The compositor sends nothing when the corner is off ("hot-corner=no"), while
// dragging a window or over a fullscreen one.
Window {
    id: root
    visible: false
    width: 160
    height: 160
    color: "transparent"

    property bool near: false
    property real progress: 0 // how close to the corner while near, 0..1
    property bool fired: false // no glow again until the pointer leaves
    readonly property real level: near && !fired ? progress : 0

    Connections {
        target: Shell
        function onHotCornerChanged(state, output, progress) {
            if (state === "away") {
                root.near = false
                root.fired = false
                return
            }
            if (state === "fire") {
                root.fired = true
            } else {
                root.near = true
                root.progress = progress
            }
            if (!root.visible) {
                if (state !== "fire" && root.level === 0) {
                    return // nothing to show yet
                }
                // Only while hidden: hiding and showing again at once breaks
                // the layer surface's configure handshake ("wrong configure serial").
                Shell.placeOnScreen(root, output)
            }
            root.visible = true
            if (state === "fire") {
                for (let i = 0; i < ripples.count; i++) {
                    ripples.itemAt(i).play()
                }
                settle.restart()
            }
        }
    }

    // Gone once nothing shows: a hidden panel gives its surface back.
    function hideIfIdle() {
        if (level === 0 && glow.opacity === 0 && !settle.running) {
            visible = false
        }
    }

    // The ripples end together with Task View's "appear".
    Timer {
        id: settle
        interval: Theme.slow + 110
        onTriggered: root.hideIfIdle()
    }

    Item {
        id: glow
        anchors.fill: parent
        // Grows and brightens with the pointer's approach; a short smoothing
        // between the compositor's steps, a slower fade once it is gone.
        opacity: 0.9 * root.level
        scale: 0.4 + 0.6 * root.level
        transformOrigin: Item.TopLeft
        Behavior on opacity { NumberAnimation { duration: root.level > 0 ? 70 : Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate } }
        Behavior on scale { NumberAnimation { duration: root.level > 0 ? 70 : Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate } }
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

    // Two quiet ripples, no bigger than the glow's reach (like GNOME's).
    Repeater {
        id: ripples
        model: 2
        Rectangle {
            id: rip
            required property int index
            x: -width / 2
            y: -height / 2
            width: 2 * 72
            height: width
            radius: width / 2
            color: Qt.alpha(Theme.accent, 0.12)
            border.width: 2
            border.color: Theme.accent
            opacity: 0
            scale: 0.2
            function play() { anim.restart() }
            SequentialAnimation {
                id: anim
                PauseAnimation { duration: index * 110 }
                ParallelAnimation {
                    NumberAnimation { target: rip; property: "scale"; from: 0.2; to: 1; duration: Theme.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
                    NumberAnimation { target: rip; property: "opacity"; from: 0.7; to: 0; duration: Theme.slow }
                }
            }
        }
    }
}
