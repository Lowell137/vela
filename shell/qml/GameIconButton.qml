// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// A small icon button of the Game Bar (pin, close, settings, the toolbar's).
Rectangle {
    id: button
    property string icon
    property string tip
    property bool checked: false
    property int glyph: 16
    signal clicked()
    width: 28
    height: 28
    radius: Theme.radius
    color: mouse.containsMouse ? Theme.subtleHover : checked ? Theme.control : "transparent"
    Image {
        anchors.centerIn: parent
        width: button.glyph
        height: button.glyph
        source: Theme.icons + button.icon
        sourceSize: Qt.size(width, height)
        opacity: button.checked || mouse.containsMouse ? 1 : 0.7
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: button.clicked()
    }
    Accessible.role: Accessible.Button
    Accessible.name: tip
}
