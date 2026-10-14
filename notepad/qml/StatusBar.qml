// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The bottom line, like Windows 11's Notepad: where the cursor is and how
// long the text is; on the right the zoom, the line endings and the encoding,
// which open their menu when clicked.
Item {
    id: bar
    property var tab: null // the open tab (Editor.qml)
    property int zoom: 100
    signal zoomMenuRequested(Item item)
    signal lineEndingMenuRequested(Item item)
    signal encodingMenuRequested(Item item)
    height: 32

    function lineEndingName(code) {
        return code === "CRLF" ? qsTr("Windows (CRLF)") : code === "CR" ? qsTr("Macintosh (CR)") : qsTr("Unix (LF)")
    }

    component Divider: Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: 1
        height: 16
        color: Theme.divider
    }
    // A column of fixed width, as in Windows: the text doesn't move the
    // others when it changes. With `menu`, it opens one when clicked.
    component Segment: Rectangle {
        id: segment
        property string text
        property bool menu: false
        signal clicked()
        height: 24
        anchors.verticalCenter: parent.verticalCenter
        radius: Theme.radius
        color: !menu ? "transparent" : mouse.pressed ? Theme.subtlePressed : mouse.containsMouse ? Theme.subtleHover : "transparent"
        Text {
            x: 10
            width: parent.width - 20
            anchors.verticalCenter: parent.verticalCenter
            text: segment.text
            color: Theme.text
            font.pixelSize: Theme.fontCaption
            elide: Text.ElideRight
        }
        MouseArea {
            id: mouse
            anchors.fill: parent
            enabled: segment.menu
            hoverEnabled: true
            onClicked: segment.clicked()
        }
    }

    Row {
        x: 6
        height: parent.height
        visible: bar.tab !== null
        spacing: 2
        Segment {
            width: Math.max(84, implicitTextWidth.advanceWidth(text) + 20)
            text: bar.tab ? qsTr("Ln %1, Col %2").arg(bar.tab.cursor.x).arg(bar.tab.cursor.y) : ""
        }
        Divider {}
        Segment {
            readonly property int count: bar.tab ? bar.tab.edit.length : 0
            width: 160
            text: count === 1 ? qsTr("1 character") : qsTr("%1 characters").arg(count)
        }
    }
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 6
        height: parent.height
        visible: bar.tab !== null
        spacing: 2
        Divider {}
        Segment {
            id: zoomSegment
            width: 64
            menu: true
            text: bar.zoom + "%"
            onClicked: bar.zoomMenuRequested(zoomSegment)
        }
        Divider {}
        Segment {
            id: lineEndingSegment
            width: 150
            menu: true
            text: bar.tab ? bar.lineEndingName(bar.tab.doc.lineEnding) : ""
            onClicked: bar.lineEndingMenuRequested(lineEndingSegment)
        }
        Divider {}
        Segment {
            id: encodingSegment
            width: 130
            menu: true
            text: bar.tab ? bar.tab.doc.encoding : ""
            onClicked: bar.encodingMenuRequested(encodingSegment)
        }
    }
    FontMetrics {
        id: implicitTextWidth
        font.pixelSize: Theme.fontCaption
    }
}
