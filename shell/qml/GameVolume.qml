// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// A volume: the mute button (the icon) and the slider.
Row {
    id: volume
    property string icon
    property bool appIcon: false
    property real value
    property bool muted
    signal moved(real value)
    signal muteClicked()
    width: parent.width
    spacing: 4
    Item {
        width: 32
        height: 32
        anchors.verticalCenter: parent.verticalCenter
        Rectangle { anchors.fill: parent; radius: Theme.radius; color: Theme.subtleHover; opacity: muteMouse.containsMouse ? 1 : 0 }
        Image {
            anchors.centerIn: parent
            width: 18
            height: 18
            opacity: volume.muted && volume.appIcon ? 0.45 : 1
            source: Theme.icons + encodeURIComponent(volume.muted && !volume.appIcon ? "audio-volume-muted" : volume.icon)
            sourceSize: Qt.size(width, height)
        }
        MouseArea {
            id: muteMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: volume.muteClicked()
        }
        Accessible.role: Accessible.Button
        Accessible.name: volume.muted ? qsTr("Unmute") : qsTr("Mute")
    }
    BarSlider {
        width: parent.width - 36
        anchors.verticalCenter: parent.verticalCenter
        value: volume.value
        onMoved: value => volume.moved(value)
    }
}
