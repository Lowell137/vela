// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Shapes

// A check box like Fluent-Qt's (CheckBox.cpp, MIT): a 20 px box with 4 px
// corners; checked it fills with the accent and the mark grows in (150 ms
// OutCubic, opacity and scale 0.8 -> 1).
Item {
    id: box
    property bool checked: false
    property string text
    property bool usable: true
    signal toggled(bool checked)

    implicitWidth: 20 + (text !== "" ? 8 + label.implicitWidth : 0)
    implicitHeight: 32
    opacity: usable ? 1 : 0.45
    activeFocusOnTab: usable

    Accessible.role: Accessible.CheckBox
    Accessible.name: text
    Accessible.checkable: true
    Accessible.checked: checked
    Accessible.onToggleAction: toggle()
    Keys.onSpacePressed: toggle()

    function toggle() {
        if (!usable) {
            return
        }
        checked = !checked
        toggled(checked)
    }

    property real progress: checked ? 1 : 0
    Behavior on progress { NumberAnimation { duration: Theme.fast; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate } }

    Rectangle {
        id: square
        anchors.verticalCenter: parent.verticalCenter
        width: 20
        height: 20
        radius: Theme.radius
        color: box.checked
            ? (mouse.pressed ? Theme.accentFillPressed : mouse.containsMouse ? Theme.accentFillHover : Theme.accentFill)
            : (mouse.pressed ? Theme.controlPressed : mouse.containsMouse ? Theme.controlHover : Theme.control)
        border.width: box.checked ? 0 : 1
        border.color: Qt.rgba(Theme.controlStrokeStrong.r, Theme.controlStrokeStrong.g, Theme.controlStrokeStrong.b,
                              Theme.controlStrokeStrong.a * (mouse.containsMouse ? 1 : 0.65))

        Shape {
            anchors.centerIn: parent
            width: 12
            height: 12
            opacity: box.progress
            scale: 0.8 + 0.2 * box.progress
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: Theme.accentText
                strokeWidth: 1.5
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                startX: 1.5; startY: 6.5
                PathLine { x: 4.5; y: 9.5 }
                PathLine { x: 10.5; y: 2.5 }
            }
        }
    }
    Text {
        id: label
        visible: box.text !== ""
        x: 28
        anchors.verticalCenter: parent.verticalCenter
        text: box.text
        color: Theme.text
        font.pixelSize: Theme.fontBody
    }
    // Keyboard focus: a ring, like the buttons'.
    Rectangle {
        visible: box.activeFocus
        anchors { fill: square; margins: -3 }
        radius: Theme.radius + 3
        color: "transparent"
        border.width: 2
        border.color: Theme.text
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: box.usable
        onClicked: box.toggle()
    }
}
