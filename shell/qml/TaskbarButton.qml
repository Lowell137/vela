// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// A taskbar button: the background fades on hover and a slight "bounce" when
// pressed. Under the icon, like on Windows 11, a gray dash if the app is open,
// longer and colored if it's the active one. With `draggable` it can be
// dragged sideways to move it (dropped says by how much).
Item {
    id: root

    property string tooltip: ""
    readonly property bool hovered: mouse.containsMouse
    property bool active: false
    property bool running: false
    // An app's button (hops): clicked while the app is open, it bounces.
    property bool hops: false
    property real hop: 0 // px away from the screen's edge
    // The app's progress (0..1, -1: none): the indicator under the icon becomes
    // a small bar that fills, like the Windows taskbar.
    property real progress: -1
    property bool draggable: false
    readonly property bool dragging: mouse.dragging
    default property alias content: holder.data

    signal clicked()
    // Windows 11's click, measured frame by frame from a recording: pressed, the
    // icon shrinks at once to 0.8 and grows back in ~200 ms on release. Meanwhile
    // it lifts 3 px (2.5 px down for a window going away) in 100 ms, holds 100 ms,
    // crosses without stopping to 1 px the other way, a little bigger, and settles
    // at ~460 ms.
    readonly property bool clickMotion: hops && Theme.taskbarActivateHop
    // After a click that hides the window the hover highlight stays off until
    // the pointer leaves.
    property bool quiet: false
    property real press: 1
    property real pulse: 1
    function bounce(down) {
        settleAnim.stop()
        bounceAnim.stop()
        bounceAnim.dir = down ? -1 : 1
        bounceAnim.start()
    }
    NumberAnimation { id: pressAnim; target: root; property: "press"; easing.type: Easing.OutCubic }
    // A press during a bounce ends it where it is, without a jump.
    ParallelAnimation {
        id: settleAnim
        NumberAnimation { target: root; property: "hop"; to: 0; duration: Theme.motion(120); easing.type: Easing.InOutQuad }
        NumberAnimation { target: root; property: "pulse"; to: 1; duration: Theme.motion(120); easing.type: Easing.InOutQuad }
    }
    ParallelAnimation {
        id: bounceAnim
        property int dir: 1
        SequentialAnimation {
            NumberAnimation { target: root; property: "hop"; to: (bounceAnim.dir < 0 ? 2.5 : 3) * bounceAnim.dir; duration: Theme.motion(100); easing.type: Easing.OutQuad }
            PauseAnimation { duration: Theme.motion(100) }
            NumberAnimation { target: root; property: "hop"; to: -bounceAnim.dir; duration: Theme.motion(100); easing.type: Easing.InOutQuad }
            PauseAnimation { duration: Theme.motion(60) }
            NumberAnimation { target: root; property: "hop"; to: 0; duration: Theme.motion(100); easing.type: Easing.OutQuad }
        }
        SequentialAnimation {
            PauseAnimation { duration: Theme.motion(200) }
            NumberAnimation { target: root; property: "pulse"; to: 1.03; duration: Theme.motion(100); easing.type: Easing.InOutQuad }
            PauseAnimation { duration: Theme.motion(60) }
            NumberAnimation { target: root; property: "pulse"; to: 1; duration: Theme.motion(100); easing.type: Easing.OutQuad }
        }
    }
    signal middleClicked()
    signal rightClicked(bool shift) // Shift+right click: the window menu
    signal dropped(real dx)
    signal wheeled(real delta) // angleDelta.y

    // While dragged it follows the mouse, above the other buttons.
    z: mouse.dragging ? 10 : 0
    // Along a vertical taskbar it is dragged up and down (dropped: by how far).
    readonly property bool vertical: Config.taskbarVertical
    // The dock style: bigger icons, bigger still under the pointer.
    readonly property real zoom: Config.dock ? (hovered ? 1.35 : 1.2) : 1
    transform: Translate { x: !root.vertical && mouse.dragging ? mouse.dragX : 0; y: root.vertical && mouse.dragging ? mouse.dragX : 0 }

    width: Theme.taskbarButtonWidth
    height: Config.dock ? 48 : Theme.taskbarButtonHeight

    // taskbar-tile-image: the theme's pictures, with them the button is not
    // drawn the Fluent way (no hover plate, no dash under the icon).
    readonly property bool pictured: Theme.taskbarTileImage !== ""
    BorderImage {
        anchors.fill: parent
        source: root.running
            ? (root.active ? Theme.taskbarTileImageActive : root.hovered ? Theme.taskbarTileImageHover : Theme.taskbarTileImage)
            : ""
        visible: source != ""
        border { left: 3; right: 3; top: 3; bottom: 3 }
    }
    // An app that is not open, under the pointer: the picture's glow without
    // its two side lines (4 px cut off each side).
    Image {
        anchors.fill: parent
        visible: root.pictured && !root.running && root.hovered && source != ""
        source: Theme.taskbarTileImageIdleHover
        sourceClipRect: Qt.rect(4, 0, Math.max(1, sourceSize.width - 8), sourceSize.height)
        fillMode: Image.Stretch
    }

    // A tile under each open app (Windows 7's glass buttons), if the theme has one.
    Rectangle {
        id: tile
        anchors.fill: parent
        visible: root.running && Theme.taskbarTileImage === "" && (Theme.taskbarButtonFill.a > 0 || Theme.taskbarButtonStroke.a > 0)
        radius: Theme.taskbarButtonRadius
        readonly property color low: root.active ? Theme.taskbarButtonFillActive : Theme.taskbarButtonFill
        color: Theme.taskbarButtonGloss ? "transparent" : low
        gradient: Theme.taskbarButtonGloss ? glossGradient : null
        border.width: Theme.taskbarButtonStroke.a > 0 ? 1 : 0
        border.color: Theme.taskbarButtonStroke
        Gradient {
            id: glossGradient
            GradientStop { position: 0; color: Qt.lighter(tile.low, 1.6) }
            GradientStop { position: 0.5; color: Qt.lighter(tile.low, 1.25) }
            GradientStop { position: 0.51; color: tile.low }
            GradientStop { position: 1; color: Qt.darker(tile.low, 1.1) }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: !root.pictured
        radius: Theme.taskbarButtonRadius
        color: mouse.pressed ? Theme.subtlePressed : Theme.subtleHover
        opacity: (mouse.containsMouse && !root.quiet) || root.active ? 1 : 0

        Behavior on opacity {
            NumberAnimation { duration: Theme.fast; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        }
    }

    Item {
        id: holder
        anchors.centerIn: parent
        width: Theme.taskbarIconSize
        height: Theme.taskbarIconSize
        scale: (mouse.pressed && Theme.taskbarBounce && !root.clickMotion ? 0.84 : 1) * root.zoom
        // Transforms, not anchors: centered anchors round to whole pixels.
        transform: [
            Scale { origin.x: holder.width / 2; origin.y: holder.height / 2; xScale: root.press * root.pulse; yScale: root.press * root.pulse },
            Translate { x: root.vertical ? (Config.taskbarLeft ? root.hop : -root.hop) : 0; y: root.vertical ? 0 : -root.hop }
        ]

        Behavior on scale {
            NumberAnimation { duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        }
    }

    // Indicator under the icon (at the screen's edge side along a vertical
    // taskbar): app open / active (or Start menu open)
    Rectangle {
        visible: !root.pictured
        property real length: root.progress >= 0 ? 24 : root.active ? 16 : root.running ? 6 : 0
        // x/y, not anchors: anchors switched to undefined at run time keep
        // their old edge when the taskbar's position changes live.
        x: root.vertical ? (Config.taskbarLeft ? 2 : parent.width - width - 2) : (parent.width - width) / 2
        y: root.vertical ? (parent.height - height) / 2 : parent.height - height - 2
        width: root.vertical ? 3 : length
        height: root.vertical ? length : 3
        radius: 1.5
        color: root.progress >= 0 ? Theme.textSecondary : root.active ? Theme.accent : Theme.textSecondary
        opacity: root.progress >= 0 ? 0.4 : root.active || root.running ? 1 : 0

        Behavior on length {
            NumberAnimation { duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        }
        Behavior on color {
            ColorAnimation { duration: Theme.fast }
        }
        Behavior on opacity {
            NumberAnimation { duration: Theme.fast }
        }
    }

    Rectangle {
        property real length: 24 * Math.min(1, Math.max(0, root.progress))
        visible: root.progress >= 0
        x: root.vertical ? (Config.taskbarLeft ? 2 : parent.width - 5) : (parent.width - 24) / 2
        y: root.vertical ? (parent.height - 24) / 2 : parent.height - 5
        width: root.vertical ? 3 : length
        height: root.vertical ? length : 3
        radius: 1.5
        color: Theme.accent
        Behavior on length { NumberAnimation { duration: Theme.fast } }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        property real pressX: 0
        property real dragX: 0
        property bool dragging: false
        property bool dragged: false // releasing a drag isn't a click
        onContainsMouseChanged: if (!containsMouse) root.quiet = false
        onPressedChanged: {
            if (!root.clickMotion) return
            if (pressed && bounceAnim.running) {
                bounceAnim.stop()
                settleAnim.start()
            }
            pressAnim.stop()
            pressAnim.to = pressed ? 0.8 : 1
            pressAnim.duration = Theme.motion(pressed ? 40 : 200)
            pressAnim.start()
        }
        // In window coordinates: the button moves with the mouse, they don't.
        onPressed: mouse => {
            pressX = root.vertical ? mapToItem(null, mouse.x, mouse.y).y : mapToItem(null, mouse.x, mouse.y).x
            dragX = 0
            dragged = false
        }
        onPositionChanged: mouse => {
            if (!pressed || !root.draggable || !(pressedButtons & Qt.LeftButton)) return
            dragX = (root.vertical ? mapToItem(null, mouse.x, mouse.y).y : mapToItem(null, mouse.x, mouse.y).x) - pressX
            if (!dragging && Math.abs(dragX) > 8) dragging = true
        }
        onReleased: mouse => {
            if (dragging) {
                dragging = false
                dragged = true
                root.dropped(dragX)
            }
        }
        onWheel: wheel => root.wheeled(wheel.angleDelta.y)
        onClicked: mouse => {
            if (dragged) {
                dragged = false
                return
            }
            if (mouse.button === Qt.MiddleButton) {
                root.middleClicked()
            } else if (mouse.button === Qt.RightButton) {
                root.rightClicked(mouse.modifiers & Qt.ShiftModifier)
            } else {
                if (root.clickMotion && root.running) {
                    // The active app's button minimizes it; any other brings its window.
                    root.bounce(root.active)
                    root.quiet = root.active
                }
                root.clicked()
            }
        }
    }
}
