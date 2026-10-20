// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Shapes
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
    // The app's progress (0..1, -1: none): the indicator under the icon becomes
    // a small bar that fills, like the Windows taskbar.
    property real progress: -1
    property bool draggable: false
    readonly property bool dragging: mouse.dragging
    default property alias content: holder.data

    signal clicked()
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

    width: 44
    height: Config.dock ? 48 : 40

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius
        color: mouse.pressed ? Theme.subtlePressed : Theme.subtleHover
        opacity: mouse.containsMouse || root.active ? 1 : 0

        Behavior on opacity {
            NumberAnimation { duration: Theme.fast; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        }
    }

    Item {
        id: holder
        anchors.centerIn: parent
        width: 26
        height: 26
        scale: (mouse.pressed ? 0.84 : 1) * root.zoom

        Behavior on scale {
            NumberAnimation { duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        }
    }

    // Indicator under the icon (at the screen's edge side along a vertical
    // taskbar): app open / active (or Start menu open)
    Rectangle {
        // Along a horizontal taskbar the active app has a short arc (below),
        // the others running in the background a 4 px dot.
        property real length: root.progress >= 0 ? 24 : root.vertical ? (root.active ? 16 : root.running ? 6 : 0) : root.running ? 4 : 0
        // x/y, not anchors: anchors switched to undefined at run time keep
        // their old edge when the taskbar's position changes live.
        x: root.vertical ? (Config.taskbarLeft ? 2 : parent.width - width - 2) : (parent.width - width) / 2
        y: root.vertical ? (parent.height - height) / 2 : parent.height - height - 2
        width: root.vertical ? 3 : length
        height: root.vertical ? length : root.progress >= 0 ? 3 : 4
        radius: Math.min(width, height) / 2
        color: root.progress >= 0 ? Theme.textSecondary : root.active ? Theme.accent : Theme.textSecondary
        opacity: root.progress >= 0 ? 0.4 : (root.vertical ? root.active || root.running : root.running && !root.active) ? 1 : 0

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

    // The open (active) app: a short curve, like the logo's trace.
    Shape {
        x: (parent.width - 16) / 2
        y: parent.height - 6
        width: 16
        height: 5
        visible: opacity > 0
        opacity: !root.vertical && root.active && root.progress < 0 ? 1 : 0
        // It draws itself from the middle outwards when the app becomes active.
        scale: root.active ? 1 : 0.1
        transformOrigin: Item.Center
        preferredRendererType: Shape.CurveRenderer
        Behavior on opacity { NumberAnimation { duration: Theme.normal } }
        Behavior on scale {
            NumberAnimation { duration: Theme.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        }
        ShapePath {
            strokeColor: Theme.accent
            strokeWidth: 2
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            startX: 1; startY: 1
            PathQuad { x: 15; y: 1; controlX: 8; controlY: 5 }
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
                root.clicked()
            }
        }
    }
}
