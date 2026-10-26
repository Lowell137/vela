// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Shapes

// An expander like Fluent-Qt's (Expander.cpp, MIT): a 44 px header (an icon,
// a title, a description, a chevron) on a card; a click unfolds the content
// below a divider, the chevron turning 180 degrees with it (150-250 ms
// InOutSine, longer the farther it goes).
Rectangle {
    id: expander
    property string title
    property string description
    property string icon
    property bool expanded: false
    default property alias content: body.data

    implicitWidth: 480
    implicitHeight: header.height + clipper.height
    radius: Theme.radius
    color: Theme.card
    border.width: 1
    border.color: Theme.cardStroke
    clip: true

    property real fraction: expanded ? 1 : 0
    Behavior on fraction {
        NumberAnimation {
            duration: Theme.motion(150 + 100 * Math.abs(expander.fraction - (expander.expanded ? 1 : 0)))
            easing.type: Easing.BezierSpline
            easing.bezierCurve: Theme.standard
        }
    }

    Accessible.role: Accessible.Grouping
    Accessible.name: title

    Rectangle {
        id: header
        width: parent.width
        height: Math.max(44, texts.implicitHeight + 16)
        radius: expander.radius
        color: mouse.pressed ? Theme.cardPressed : mouse.containsMouse ? Theme.cardHover : "transparent"
        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: expander.title
        Accessible.onPressAction: expander.expanded = !expander.expanded
        Keys.onSpacePressed: expander.expanded = !expander.expanded
        Keys.onReturnPressed: expander.expanded = !expander.expanded

        Image {
            id: headerIcon
            visible: expander.icon !== ""
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            width: 20
            height: 20
            source: expander.icon !== "" ? Theme.icons + encodeURIComponent(expander.icon) : ""
            sourceSize: Qt.size(width, height)
        }
        Column {
            id: texts
            anchors { left: parent.left; leftMargin: headerIcon.visible ? 52 : 16; right: chevronBox.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
            Text {
                width: parent.width
                text: expander.title
                color: Theme.text
                font.pixelSize: Theme.fontBody
                elide: Text.ElideRight
            }
            Text {
                visible: expander.description !== ""
                width: parent.width
                text: expander.description
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                elide: Text.ElideRight
            }
        }
        Item {
            id: chevronBox
            anchors { right: parent.right; rightMargin: 14; verticalCenter: parent.verticalCenter }
            width: 22
            height: 22
            Shape {
                anchors.centerIn: parent
                width: 10
                height: 6
                rotation: 180 * expander.fraction
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: Theme.text
                    strokeWidth: 1.2
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    startX: 1; startY: 1
                    PathLine { x: 5; y: 5 }
                    PathLine { x: 9; y: 1 }
                }
            }
        }
        Rectangle {
            visible: header.activeFocus
            anchors { fill: parent; margins: 2 }
            radius: expander.radius
            color: "transparent"
            border.width: 2
            border.color: Theme.text
        }
        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: expander.expanded = !expander.expanded
        }
    }

    // The content: its height times the fraction, cut at the bottom.
    Item {
        id: clipper
        y: header.height
        width: parent.width
        height: (body.childrenRect.height + 33) * expander.fraction
        clip: true
        visible: height > 0
        Rectangle { width: parent.width; height: 1; color: Theme.cardStroke }
        Item {
            id: body
            x: 16
            y: 17
            width: parent.width - 32
            height: childrenRect.height
        }
    }
}
