// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// A radio button like Fluent-Qt's (RadioButton.cpp, MIT): a 20 px circle;
// checked it fills with the accent around a 10 px dot that grows to 12 under
// the pointer and pops in when checked (150 ms OutCubic). Checking one is
// the caller's business (the others of the group uncheck through bindings).
Item {
    id: radio
    property bool checked: false
    property string text
    property bool usable: true
    signal clicked()

    implicitWidth: 20 + (text !== "" ? 8 + label.implicitWidth : 0)
    implicitHeight: 32
    opacity: usable ? 1 : 0.45
    activeFocusOnTab: usable

    Accessible.role: Accessible.RadioButton
    Accessible.name: text
    Accessible.checkable: true
    Accessible.checked: checked
    Accessible.onPressAction: if (usable) clicked()
    Keys.onSpacePressed: if (usable) clicked()

    property real progress: checked ? 1 : 0
    Behavior on progress { NumberAnimation { duration: Theme.fast; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate } }

    Rectangle {
        id: circle
        anchors.verticalCenter: parent.verticalCenter
        width: 20
        height: 20
        radius: 10
        color: radio.checked
            ? (mouse.pressed ? Theme.accentFillPressed : mouse.containsMouse ? Theme.accentFillHover : Theme.accentFill)
            : (mouse.pressed ? Theme.controlPressed : mouse.containsMouse ? Theme.controlHover : Theme.control)
        border.width: radio.checked ? 0 : 1
        border.color: Qt.rgba(Theme.controlStrokeStrong.r, Theme.controlStrokeStrong.g, Theme.controlStrokeStrong.b,
                              Theme.controlStrokeStrong.a * (mouse.containsMouse ? 1 : 0.65))

        Rectangle {
            anchors.centerIn: parent
            property real size: mouse.containsMouse || mouse.pressed ? 12 : 10
            width: size * radio.progress
            height: width
            radius: width / 2
            color: Theme.accentText
            Behavior on size { NumberAnimation { duration: Theme.fast; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate } }
        }
    }
    Text {
        id: label
        visible: radio.text !== ""
        x: 28
        anchors.verticalCenter: parent.verticalCenter
        text: radio.text
        color: Theme.text
        font.pixelSize: Theme.fontBody
    }
    Rectangle {
        visible: radio.activeFocus
        anchors { fill: circle; margins: -3 }
        radius: 13
        color: "transparent"
        border.width: 2
        border.color: Theme.text
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: radio.usable
        onClicked: radio.clicked()
    }
}
