// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The Terminal's settings, like Notepad's: a page over the tab with how the
// text looks and the profiles, each a program to start in a folder.
Rectangle {
    id: page
    property var win // Main.qml
    signal closed()
    color: Theme.dialogFooter
    focus: true
    Keys.onEscapePressed: closed()

    MouseArea { anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.AllButtons; onWheel: {} } // the terminal below isn't reached

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
    // A button with what's chosen, opening the list to choose from.
    component Choice: ToolButton {
        id: choice
        property var options: [] // texts
        property string value
        signal picked(string value, int index)
        text: value
        menu: true
        color: Theme.control
        border.width: 1
        border.color: Theme.controlStroke
        onClicked: page.win.openBelow(options.map((o, i) => ({ text: o, radio: true, checked: o === value, action: () => choice.picked(o, i) })), choice)
    }

    function changeProfile(index, key, value) {
        const list = page.win.profiles.map(p => ({ name: p.name, command: p.command, directory: p.directory }))
        if (list[index][key] === value) return
        list[index][key] = value
        page.win.setProfiles(list)
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

            Heading { text: qsTr("Appearance") }
            Card {
                title: qsTr("Font")
                Choice {
                    options: Host.fonts()
                    value: page.win ? page.win.fontFamily : ""
                    onPicked: value => page.win.fontFamily = value
                }
            }
            Card {
                title: qsTr("Font size")
                description: qsTr("Also with Ctrl and +, - or 0")
                Row {
                    spacing: 4
                    ToolButton { icon: "list-remove"; usable: page.win && page.win.fontSize > 6; onClicked: page.win.fontSize-- }
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
                title: qsTr("Color scheme")
                description: qsTr("Vela follows the light or dark mode")
                Choice {
                    options: page.win ? Object.keys(page.win.schemes) : []
                    value: page.win ? page.win.schemeName : ""
                    onPicked: value => page.win.schemeName = value
                }
            }
            Card {
                title: qsTr("Background opacity")
                description: qsTr("Lower lets what's behind the window show through")
                Row {
                    spacing: 8
                    Slider {
                        anchors.verticalCenter: parent.verticalCenter
                        from: 0.3
                        to: 1
                        stepSize: 0.05
                        value: page.win ? page.win.backgroundOpacity : 1
                        onMoved: value => page.win.backgroundOpacity = value
                    }
                    Text {
                        width: 40
                        anchors.verticalCenter: parent.verticalCenter
                        text: page.win ? Math.round(page.win.backgroundOpacity * 100) + "%" : ""
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                    }
                }
            }

            Heading { text: qsTr("Profiles") }
            Card {
                title: qsTr("Default profile")
                description: qsTr("What a new tab starts")
                Choice {
                    options: page.win ? page.win.profiles.map(p => p.name) : []
                    value: page.win && page.win.profiles[page.win.defaultProfile] ? page.win.profiles[page.win.defaultProfile].name : ""
                    onPicked: (value, index) => page.win.defaultProfile = index
                }
            }
            Repeater {
                model: page.win ? page.win.profiles : []
                delegate: Rectangle {
                    id: profileCard
                    required property var modelData
                    required property int index
                    width: cards.width
                    height: 124
                    radius: Theme.radiusCard
                    color: Theme.card
                    border.width: 1
                    border.color: Theme.cardStroke
                    Grid {
                        x: 16
                        y: 12
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 4
                        verticalItemAlignment: Grid.AlignVCenter
                        Text { text: qsTr("Name"); color: Theme.textSecondary; font.pixelSize: Theme.fontCaption }
                        TextBox {
                            width: profileCard.width - 200
                            text: profileCard.modelData.name
                            onEditingFinished: page.changeProfile(profileCard.index, "name", text)
                        }
                        Text { text: qsTr("Command"); color: Theme.textSecondary; font.pixelSize: Theme.fontCaption }
                        TextBox {
                            width: profileCard.width - 200
                            text: profileCard.modelData.command
                            onEditingFinished: page.changeProfile(profileCard.index, "command", text)
                        }
                        Text { text: qsTr("Starts in"); color: Theme.textSecondary; font.pixelSize: Theme.fontCaption }
                        TextBox {
                            width: profileCard.width - 200
                            text: profileCard.modelData.directory
                            placeholderText: qsTr("The home folder")
                            onEditingFinished: page.changeProfile(profileCard.index, "directory", text)
                        }
                    }
                    ToolButton {
                        anchors { right: parent.right; top: parent.top; margins: 12 }
                        icon: "edit-delete"
                        tooltip: qsTr("Remove this profile")
                        usable: page.win.profiles.length > 1
                        onClicked: {
                            const list = page.win.profiles.slice()
                            list.splice(profileCard.index, 1)
                            page.win.setProfiles(list)
                        }
                    }
                }
            }
            Button {
                text: qsTr("Add a profile")
                icon: "list-add"
                onClicked: page.win.setProfiles(page.win.profiles.concat([{ name: qsTr("New profile"), command: page.win.profiles[0].command, directory: "" }]))
            }
            Item { width: 1; height: 12 }
        }
    }
}
