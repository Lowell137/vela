// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Dialogs
import Vela.Controls

// The Video Editor's settings, a page over the window like Notepad's: where
// exports go, their usual quality and whether the graphics card encodes.
Rectangle {
    id: page
    property var win // Main.qml
    signal closed()
    color: Theme.dialogFooter
    focus: true
    Keys.onEscapePressed: closed()

    MouseArea { anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.AllButtons; onWheel: {} }

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
            width: parent.width - slot.width - 48
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text { text: title; color: Theme.text; font.pixelSize: Theme.fontBody }
            Text {
                width: parent.width
                visible: description !== ""
                text: description
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                elide: Text.ElideMiddle
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

    FolderDialog {
        id: folderDialog
        title: qsTr("Export folder")
        currentFolder: page.win ? "file://" + page.win.exportFolder : ""
        onAccepted: page.win.setExportFolder(page.win.localPath(selectedFolder))
    }

    Row {
        id: header
        x: 8
        y: 8
        spacing: 8
        ToolButton {
            anchors.verticalCenter: parent.verticalCenter
            icon.name: "go-previous"
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

            Heading { text: qsTr("Export") }
            Card {
                title: qsTr("Export folder")
                description: page.win ? page.win.exportFolder : ""
                Button { text: qsTr("Change..."); onClicked: folderDialog.open() }
            }
            Card {
                title: qsTr("Quality")
                description: qsTr("What the export starts with")
                Row {
                    spacing: 6
                    Repeater {
                        model: [{ value: "high", text: qsTr("High") }, { value: "medium", text: qsTr("Medium") }, { value: "small", text: qsTr("Small file") }]
                        delegate: Chip {
                            required property var modelData
                            text: modelData.text
                            checked: page.win && page.win.defaultQuality === modelData.value
                            onClicked: page.win.defaultQuality = modelData.value
                        }
                    }
                }
            }
            Card {
                title: qsTr("Hardware encoding")
                description: qsTr("MP4 is encoded by the graphics card (NVIDIA) when it can; otherwise by the processor")
                Switch { checked: page.win ? page.win.hardwareEncoder : false; onToggled: on => page.win.hardwareEncoder = on }
            }

            Heading { text: qsTr("About") }
            Card {
                title: qsTr("Video Editor")
                description: qsTr("Part of the Vela desktop. Plays with mpv, exports with ffmpeg")
            }
        }
    }
}
