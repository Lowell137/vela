// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// System > Storage, like Windows' Storage: the volumes at their fill
// levels, and the temporary files (cache, trash) to clean up by hand.
// Nothing happens by itself: only the buttons delete, and only in the home.
Page {
    id: page

    Component.onCompleted: Storage.refresh()

    readonly property real gigabyte: 1024 * 1024 * 1024
    readonly property real megabyte: 1024 * 1024

    // GB with one decimal, MB below, KB for what's left (the units are the
    // same in every language).
    function humanSize(bytes) {
        if (bytes >= page.gigabyte) {
            return (bytes / page.gigabyte).toFixed(1) + " GB"
        }
        if (bytes >= page.megabyte) {
            return Math.round(bytes / page.megabyte) + " MB"
        }
        return Math.ceil(bytes / 1024) + " KB"
    }

    // The volumes, one card and one bar each, like Windows' storage list.
    CardGroup {
        Repeater {
            model: Storage.volumes
            delegate: Card {
                required property var modelData
                icon: "drive-harddisk-symbolic"
                title: modelData.name
                description: qsTr("%1 of %2 used").arg(page.humanSize(modelData.used), page.humanSize(modelData.total))
                contentItem: LevelBar {
                    width: parent.width
                    level: modelData.total > 0 ? modelData.used / modelData.total : 0
                }
            }
        }
    }

    CardGroup {
        title: qsTr("Temporary files")
        Card {
            icon: "user-trash"
            title: qsTr("Trash")
            description: page.humanSize(Storage.trashSize)
            trailing: Button {
                text: qsTr("Empty")
                usable: Storage.trashSize > 0
                onClicked: Storage.emptyTrash()
            }
        }
        Card {
            icon: "edit-clear"
            title: qsTr("Cache")
            description: page.humanSize(Storage.cacheSize)
            trailing: Button {
                text: qsTr("Clear")
                onClicked: clearDialog.open()
            }
        }
    }

    Text {
        width: parent.width
        visible: Storage.message !== ""
        text: Storage.message
        color: Theme.textSecondary
        font.pixelSize: Theme.fontBody
        wrapMode: Text.Wrap
        topPadding: 12
        bottomPadding: 12
    }

    CardGroup {
        title: qsTr("Biggest folders")
        Card {
            icon: "folder"
            title: qsTr("Biggest folders")
            description: Storage.scanning ? qsTr("Measuring...") : qsTr("The biggest things right in the home folder")
            contentItem: Column {
                width: parent.width
                spacing: 2
                Repeater {
                    model: Storage.folders
                    delegate: Row {
                        id: entry
                        required property var modelData
                        width: parent.width
                        height: 32
                        spacing: 8
                        Text {
                            width: parent.width - 120
                            anchors.verticalCenter: parent.verticalCenter
                            text: entry.modelData.name
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            elide: Text.ElideRight
                        }
                        Text {
                            width: 112
                            anchors.verticalCenter: parent.verticalCenter
                            horizontalAlignment: Text.AlignRight
                            text: page.humanSize(entry.modelData.size)
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontBody
                        }
                    }
                }
                Text {
                    visible: Storage.folders.length === 0 && !Storage.scanning
                    width: parent.width
                    text: qsTr("Nothing could be measured.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                }
            }
        }
    }

    // The cache holds the apps' working data: removing it is safe but a
    // surprise, so it asks first (like Windows' cleanup).
    Dialog {
        id: clearDialog
        parent: root.contentItem
        title: qsTr("Clear the cache?")
        primaryText: qsTr("Clear")
        onAccepted: Storage.clearCache()
        Column {
            width: parent.width
            spacing: 8
            Text {
                width: parent.width
                text: qsTr("Everything in the cache (%1) is deleted right now. Apps build their cache again as they need it.").arg(page.humanSize(Storage.cacheSize))
                color: Theme.text
                font.pixelSize: Theme.fontBody
                wrapMode: Text.Wrap
            }
        }
    }
}
