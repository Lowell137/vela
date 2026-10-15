// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Audio widget: the output, its volume and the apps playing now (Audio, the
// same mixer as Quick Settings).
Column {
    id: body
    spacing: 10

    GameCaption {
        visible: !Audio.available
        text: qsTr("Sound isn't available.")
    }
    Rectangle {
        id: outputButton
        visible: Audio.available
        width: parent.width
        height: 36
        radius: Theme.radius
        color: outputMouse.containsMouse ? Theme.controlHover : Theme.control
        border.width: 1
        border.color: Theme.controlStroke
        Text {
            x: 12
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 48
            text: (Audio.outputs.find(o => o.isDefault) || { description: qsTr("Speakers") }).description
            color: Theme.text
            font.pixelSize: Theme.fontBody
            elide: Text.ElideRight
        }
        Image {
            anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
            width: 12
            height: 12
            source: Theme.icons + "go-down"
            sourceSize: Qt.size(width, height)
        }
        MouseArea {
            id: outputMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                const entries = Audio.outputs.map(o => ({ text: o.description.replace(/&/g, "&&"), icon: o.icon, radio: true,
                                                          checked: o.isDefault, action: () => Audio.setDefault("output", o.name) }))
                const p = outputButton.mapToItem(null, 0, outputButton.height + 4)
                Menus.open(entries, p.x, p.y, { minWidth: outputButton.width, screen: GameNumbers.editScreen })
            }
        }
        Accessible.role: Accessible.ComboBox
        Accessible.name: qsTr("Output device")
    }
    GameVolume {
        visible: Audio.available && Audio.defaultOutput !== ""
        icon: Audio.iconName
        value: Audio.volume
        muted: Audio.muted
        onMoved: value => {
            if (Audio.muted) Audio.setMuted("output", Audio.defaultOutput, false)
            Audio.setVolume("output", Audio.defaultOutput, value)
        }
        onMuteClicked: Audio.setMuted("output", Audio.defaultOutput, !Audio.muted)
    }
    Repeater {
        model: Audio.apps.filter(a => a.playing)
        delegate: Column {
            id: app
            required property var modelData
            width: body.width
            GameCaption {
                leftPadding: 36
                text: app.modelData.name
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
            }
            GameVolume {
                icon: app.modelData.icon
                appIcon: true
                value: app.modelData.volume
                muted: app.modelData.muted
                onMoved: value => Audio.setAppVolume(app.modelData.key, value)
                onMuteClicked: Audio.setAppMuted(app.modelData.key, !app.modelData.muted)
            }
        }
    }
    GameCaption {
        visible: Audio.available && !Audio.apps.some(a => a.playing)
        text: qsTr("No apps are playing sound.")
    }
}
