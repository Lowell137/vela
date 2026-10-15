// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Edit > Font: the installed families, in a list.
Dialog {
    id: dialog
    property var families: []
    property string family
    signal accepted(string family)

    title: qsTr("Font")
    buttons: [{ text: qsTr("OK"), accent: true, value: "ok" }, { text: qsTr("Cancel"), value: "cancel" }]
    onChosen: value => { if (value === "ok") accepted(family) }

    function ask(current) {
        family = current
        open()
        list.positionViewAtIndex(Math.max(0, families.indexOf(current)), ListView.Center)
    }

    ListView {
        id: list
        width: parent.width
        height: 288
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: dialog.families
        delegate: Rectangle {
            id: row
            required property string modelData
            width: list.width - 14
            height: 32
            radius: Theme.radius
            color: modelData === dialog.family ? Theme.selection : rowMouse.containsMouse ? Theme.subtleHover : "transparent"
            Text {
                x: 12
                width: parent.width - 24
                anchors.verticalCenter: parent.verticalCenter
                text: row.modelData
                color: Theme.text
                font.pixelSize: Theme.fontBody
                elide: Text.ElideRight
            }
            MouseArea {
                id: rowMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: dialog.family = row.modelData
                onDoubleClicked: dialog.close("ok")
            }
        }
    }
    ScrollBar {
        flickable: list
        anchors { right: list.right; top: list.top; bottom: list.bottom }
    }
}
