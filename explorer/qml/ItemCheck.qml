// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// File Explorer's item check box: on a selected or hovered item, a click adds
// the item to the selection or takes it out.
Rectangle {
    property var tab
    property int row: -1
    property bool checked: false
    property bool hovered: false

    visible: !!tab.win && !!tab.win.itemChecks && (checked || hovered || mouse.containsMouse)
    width: 16
    height: 16
    radius: 3
    color: checked ? Theme.accentFill : Theme.filesContent
    border.width: checked ? 0 : 1
    border.color: Theme.textSecondary
    Text {
        anchors.centerIn: parent
        visible: parent.checked
        text: "\u2713"
        color: Theme.background
        font.pixelSize: 12
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: tab.model.toggle(parent.row)
    }
}
