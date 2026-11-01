// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Files.Backend
import Vela.Controls

// Network: the shares that are mounted, and connecting to one by its address.
// It doesn't scan the network for computers.
Item {
    id: page
    property var tab
    property var locations: Ops.networkLocations()
    Connections {
        target: Ops
        function onNetworkChanged() { page.locations = Ops.networkLocations() }
    }
    Column {
        x: 24
        y: 16
        width: page.width - 48
        spacing: 4
        ToolButton {
            icon.name: "network-map"
            text: qsTr("Map network drive")
            onClicked: page.tab.win.askMapDrive()
        }
        Text {
            visible: page.locations.length === 0
            topPadding: 12
            text: qsTr("No network locations are connected.")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
        }
        Repeater {
            model: page.locations
            delegate: Rectangle {
                id: share
                required property var modelData
                width: parent.width
                height: 32
                color: shareMouse.containsMouse ? Theme.filesItemHover : "transparent"
                Text {
                    x: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: share.modelData.name
                    color: Theme.text
                    font.pixelSize: Theme.fontCaption
                }
                MouseArea {
                    id: shareMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onDoubleClicked: page.tab.navigate(share.modelData.path)
                }
                ToolButton {
                    anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                    icon.name: "media-eject"
                    tooltip: qsTr("Disconnect")
                    onClicked: Ops.disconnectNetwork(share.modelData.path)
                }
            }
        }
    }
}
