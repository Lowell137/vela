// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// A labelled switch.
Item {
    id: row
    property string text
    property bool checked
    property bool usable: true
    signal toggled(bool on)
    width: parent.width
    height: 32
    Text {
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width - 52
        text: row.text
        color: row.usable ? Theme.text : Theme.textSecondary
        font.pixelSize: Theme.fontBody
        elide: Text.ElideRight
    }
    Toggle {
        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
        showLabel: false
        usable: row.usable
        checked: row.checked
        onToggled: on => row.toggled(on)
    }
}
