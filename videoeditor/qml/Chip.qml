// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// One of a few choices side by side (a crop, a quality): the chosen one is
// filled with the accent.
Rectangle {
    id: chip
    property string text
    property bool checked
    signal clicked()
    width: label.implicitWidth + 20
    height: 28
    radius: Theme.radius
    color: checked ? Theme.accentFill : mouse.containsMouse ? Theme.controlHover : Theme.control
    border.width: 1
    border.color: Theme.controlStroke
    Accessible.role: Accessible.RadioButton
    Accessible.name: text
    Accessible.checked: checked
    Text {
        id: label
        anchors.centerIn: parent
        text: chip.text
        color: chip.checked ? Theme.accentText : Theme.text
        font.pixelSize: Theme.fontCaption
    }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: chip.clicked() }
}
