// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Dialogs
import Vela.Controls

// The Terminal's settings, in a tab of their own: the sections at the left, the
// chosen one at the right. How the text looks, the color schemes, the profiles.
Rectangle {
    id: page
    property var win // Main.qml
    property string section: "appearance"
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
    // One section in the list at the left.
    component NavItem: Rectangle {
        id: item
        property string key
        property string label
        property string icon // a Theme icon's name
        readonly property bool selected: page.section === key
        width: parent.width
        height: 36
        radius: Theme.radius
        onSelectedChanged: navIndicator.track(item, selected)
        Component.onCompleted: navIndicator.track(item, selected)
        color: selected ? Theme.subtleHover : navMouse.containsMouse ? Theme.subtleHover : "transparent"
        Image {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            source: Theme.icons + item.icon
            sourceSize: Qt.size(16, 16)
        }
        Text {
            x: 48
            anchors.verticalCenter: parent.verticalCenter
            text: item.label
            color: Theme.text
            font.pixelSize: Theme.fontBody
        }
        MouseArea {
            id: navMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: page.section = item.key
        }
    }
    // A color scheme to choose: its name, and a few of its colors.
    component SchemeCard: Rectangle {
        property string name
        readonly property var scheme: page.win ? page.win.schemes[name] : null
        readonly property bool chosen: page.win && page.win.schemeName === name
        width: parent.width
        height: 56
        radius: Theme.radiusCard
        color: Theme.card
        border.width: chosen ? 2 : 1
        border.color: chosen ? Theme.accent : Theme.cardStroke
        Text {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            text: name
            color: Theme.text
            font.pixelSize: Theme.fontBody
        }
        Row {
            anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
            spacing: 3
            Repeater {
                model: scheme ? scheme.colors.slice(1, 8) : []
                delegate: Rectangle {
                    required property string modelData
                    width: 14
                    height: 14
                    radius: 3
                    color: modelData
                }
            }
        }
        MouseArea {
            anchors.fill: parent
            onClicked: page.win.schemeName = name
        }
    }

    readonly property var order: ["appearance", "profiles"]
    property int shown: 0
    onSectionChanged: {
        const index = order.indexOf(section)
        slide.x = (index > shown ? -0.28 : 0.28) * sections.width
        shown = index
        enter.restart()
        fade.restart()
    }

    function changeProfile(index, key, value) {
        const list = page.win.profiles.map(p => ({ name: p.name, command: p.command, directory: p.directory }))
        if (list[index][key] === value) return
        list[index][key] = value
        page.win.setProfiles(list)
    }

    // The sections at the left.
    Column {
        id: nav
        x: 8
        y: 12
        width: 200
        spacing: 2
        NavItem { key: "appearance"; label: qsTr("Appearance"); icon: "draw-brush" }
        NavItem { key: "profiles"; label: qsTr("Profiles"); icon: "application-menu" }
    }
    // The selected section's pill: Vela's NavIndicator, as in Settings and Files; it slides between them.
    NavIndicator { id: navIndicator }

    Text {
        id: title
        x: 232
        y: 16
        text: ({ appearance: qsTr("Appearance"), profiles: qsTr("Profiles") })[page.section]
        color: Theme.text
        font.pixelSize: Theme.fontTitle
        font.weight: Font.DemiBold
    }

    Flickable {
        anchors { left: nav.right; leftMargin: 24; right: parent.right; rightMargin: 24; top: title.bottom; topMargin: 8; bottom: parent.bottom }
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentHeight: sections.height + 24

        Item {
            id: sections
            width: Math.min(640, parent.width)
            height: Math.max(appearance.height, profiles.height)

            // The sections lie on top of each other; the chosen one is shown and slides in
            // by 0.28 of the width from the side it lies on, as Settings' pages do.
            transform: Translate { id: slide }
            NumberAnimation {
                id: fade
                target: sections
                property: "opacity"
                from: 0.4
                to: 1
                duration: Theme.fast
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                id: enter
                target: slide
                property: "x"
                to: 0
                duration: Theme.normal
                easing.type: Easing.BezierSpline
                easing.bezierCurve: Theme.decelerate
            }
            Column {
                id: appearance
                width: parent.width
                spacing: 4
                visible: page.section === "appearance"
                Heading { text: qsTr("Preview") }
                Rectangle {
                    width: parent.width
                    height: 132
                    radius: Theme.radiusCard
                    color: page.win ? Qt.alpha(page.win.scheme.background, page.win.backgroundOpacity) : "black"
                    border.width: 1
                    border.color: Theme.cardStroke
                    Column {
                        x: 16
                        y: 14
                        spacing: 6
                        Text {
                            text: Host.userName + "@terminal:~$ ls --color"
                            color: page.win ? page.win.scheme.foreground : "white"
                            font.family: page.win ? page.win.fontFamily : "monospace"
                            font.pixelSize: page.win ? page.win.fontSize : 14
                        }
                        Row {
                            spacing: 14
                            Repeater {
                                model: [1, 2, 3, 4, 5, 6]
                                delegate: Text {
                                    required property int modelData
                                    text: ["red", "green", "yellow", "blue", "magenta", "cyan"][modelData - 1]
                                    color: page.win ? page.win.scheme.colors[modelData] : "white"
                                    font.family: page.win ? page.win.fontFamily : "monospace"
                                    font.pixelSize: page.win ? page.win.fontSize : 14
                                }
                            }
                        }
                    }
                }
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
                        spacing: 8
                        ToolButton { anchors.verticalCenter: parent.verticalCenter; icon.name: "list-remove"; usable: page.win && page.win.fontSize > 6; onClicked: page.win.fontSize-- }
                        Slider {
                            anchors.verticalCenter: parent.verticalCenter
                            from: 6
                            to: 72
                            stepSize: 1
                            value: page.win ? page.win.fontSize : 14
                            onMoved: value => page.win.fontSize = Math.round(value)
                        }
                        Text {
                            width: 32
                            anchors.verticalCenter: parent.verticalCenter
                            horizontalAlignment: Text.AlignHCenter
                            text: page.win ? page.win.fontSize : ""
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                        }
                        ToolButton { anchors.verticalCenter: parent.verticalCenter; icon.name: "list-add"; usable: page.win && page.win.fontSize < 72; onClicked: page.win.fontSize++ }
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
                            stepSize: 0.01
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
                Heading { text: qsTr("Color schemes") }
                Repeater {
                    model: page.win ? Object.keys(page.win.schemes) : []
                    delegate: SchemeCard { required property string modelData; name: modelData }
                }
                Heading { text: qsTr("Import") }
                Card {
                    title: qsTr("Settings or a color scheme")
                    description: qsTr("A .json file: a color scheme, or settings. A .terminal profile from macOS works too")
                    Button {
                        text: qsTr("Choose a file")
                        icon.name: "document-open"
                        onClicked: importDialog.open()
                    }
                }
                FileDialog {
                    id: importDialog
                    title: qsTr("Choose a settings or color scheme file")
                    nameFilters: [qsTr("Settings, color schemes and Terminal profiles (*.json *.terminal)"), qsTr("All files (*)")]
                    onAccepted: page.win.importJson(selectedFile.toString())
                }
                Heading { text: qsTr("Custom colors") }
                Repeater {
                    model: page.win ? [
                        { label: qsTr("Text"), key: "foreground", index: -1 },
                        { label: qsTr("Background"), key: "background", index: -1 },
                        { label: qsTr("Black"), key: "", index: 0 }, { label: qsTr("Red"), key: "", index: 1 },
                        { label: qsTr("Green"), key: "", index: 2 }, { label: qsTr("Yellow"), key: "", index: 3 },
                        { label: qsTr("Blue"), key: "", index: 4 }, { label: qsTr("Magenta"), key: "", index: 5 },
                        { label: qsTr("Cyan"), key: "", index: 6 }, { label: qsTr("White"), key: "", index: 7 }
                    ] : []
                    delegate: Card {
                        required property var modelData
                        readonly property string current: page.win ? (modelData.index >= 0 ? page.win.custom.colors[modelData.index] : page.win.custom[modelData.key]) : ""
                        title: modelData.label
                        Row {
                            spacing: 8
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 18
                                height: 18
                                radius: 4
                                color: current
                                border.width: 1
                                border.color: Theme.cardStroke
                            }
                            TextBox {
                                width: 110
                                text: current
                                onEditingFinished: {
                                    if (/^#[0-9a-fA-F]{6}$/.test(text)) page.win.setCustomColor(modelData.key, modelData.index, text)
                                    else text = current
                                }
                            }
                        }
                    }
                }
            }

            Column {
                id: profiles
                width: parent.width
                spacing: 4
                visible: page.section === "profiles"
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
                        width: profiles.width
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
                            icon.name: "edit-delete"
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
                    icon.name: "list-add"
                    onClicked: page.win.setProfiles(page.win.profiles.concat([{ name: qsTr("New profile"), command: page.win.profiles[0].command, directory: "" }]))
                }
            }
        }
    }
}
