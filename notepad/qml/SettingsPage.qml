// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Notepad's settings, like Windows 11's: a page over the tab with the text's
// font and what happens when Notepad starts. The same choices are in the
// menus; here they're together.
Rectangle {
    id: page
    property var win // Main.qml
    signal closed()
    color: Theme.dialogFooter
    focus: true
    Keys.onEscapePressed: closed()

    MouseArea { anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.AllButtons; onWheel: {} } // the text below isn't reached

    // One setting: its name, a line about it, the control at the right.
    component Card: Rectangle {
        property string title
        property string description
        default property alias control: slot.data
        width: parent.width
        height: 64
        radius: Theme.radiusCard
        color: Theme.card
        border.width: 1
        border.color: Theme.cardStroke
        Column {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text {
                text: title
                color: Theme.text
                font.pixelSize: Theme.fontBody
            }
            Text {
                visible: description !== ""
                text: description
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }
        }
        Item {
            id: slot
            anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
            width: childrenRect.width
            height: childrenRect.height
        }
    }
    component Heading: Text {
        topPadding: 12
        color: Theme.text
        font.pixelSize: Theme.fontBody
        font.weight: Font.DemiBold
    }

    Row {
        id: header
        x: 8
        y: 8
        spacing: 8
        ToolButton {
            anchors.verticalCenter: parent.verticalCenter
            icon: "go-previous"
            tooltip: qsTr("Back (Esc)")
            onClicked: page.closed()
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Settings")
            color: Theme.text
            font.pixelSize: Theme.fontTitle
            font.weight: Font.DemiBold
        }
    }

    Flickable {
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom; topMargin: 8 }
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentHeight: cards.height + 24

        Column {
            id: cards
            x: Math.max(24, (parent.width - width) / 2)
            width: Math.min(720, page.width - 48)
            spacing: 4

            Heading { text: qsTr("Text formatting") }
            Card {
                title: qsTr("Font")
                description: page.win ? page.win.fontFamily : ""
                Button { text: qsTr("Change..."); onClicked: page.win.chooseFont() }
            }
            Card {
                title: qsTr("Font size")
                Row {
                    spacing: 4
                    ToolButton { icon: "list-remove"; usable: page.win && page.win.fontSize > 8; onClicked: page.win.fontSize-- }
                    Text {
                        width: 32
                        anchors.verticalCenter: parent.verticalCenter
                        horizontalAlignment: Text.AlignHCenter
                        text: page.win ? page.win.fontSize : ""
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                    }
                    ToolButton { icon: "list-add"; usable: page.win && page.win.fontSize < 72; onClicked: page.win.fontSize++ }
                }
            }
            Card {
                title: qsTr("Word wrap")
                description: qsTr("Long lines continue on the next one instead of past the edge")
                Toggle { checked: page.win ? page.win.wordWrap : false; onToggled: on => page.win.wordWrap = on }
            }

            Heading { text: qsTr("Window") }
            Card {
                title: qsTr("Status bar")
                description: qsTr("The cursor's place, the zoom, the line endings and the encoding")
                Toggle { checked: page.win ? page.win.statusBarVisible : false; onToggled: on => page.win.statusBarVisible = on }
            }

            Heading { text: qsTr("When Notepad starts") }
            Card {
                title: qsTr("Continue the previous session")
                description: qsTr("Reopen the files that were open when it was closed")
                Toggle { checked: page.win ? page.win.restoreSession : false; onToggled: on => page.win.restoreSession = on }
            }

            Heading { text: qsTr("About") }
            Card {
                title: qsTr("Notepad")
                description: qsTr("Part of the Vela desktop")
            }
        }
    }
}
