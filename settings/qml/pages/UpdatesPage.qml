// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// System > Updates, like Windows 11's Windows Update: whether there's a
// newer Vela, what it brings, and where the updates come from. The updater
// is the vela-update script, the same one the command line uses.
Page {
    id: page

    readonly property string status: {
        if (Updates.status === "checking") {
            return qsTr("Checking for updates...")
        }
        if (Updates.status === "up-to-date") {
            return qsTr("You're up to date")
        }
        if (Updates.status === "available") {
            return qsTr("Update available: %1").arg(Updates.newRevision)
        }
        if (Updates.status === "updating") {
            return qsTr("The update is running in a terminal")
        }
        if (Updates.status === "error") {
            return Updates.message
        }
        return qsTr("Look for a newer Vela")
    }

    CardGroup {
        Card {
            icon: "system-software-update"
            title: qsTr("Vela Update")
            description: page.status
            trailing: Button {
                text: qsTr("Check for updates")
                usable: !Updates.busy
                onClicked: Updates.check()
            }
            contentItem: Column {
                width: parent.width
                spacing: 8

                Text {
                    width: parent.width
                    visible: Updates.installedVersion !== ""
                    text: qsTr("Installed: %1").arg(Updates.installedVersion)
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    visible: Updates.lastChecked !== ""
                    text: qsTr("Last checked: %1").arg(Updates.lastChecked)
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    elide: Text.ElideRight
                }
                Button {
                    visible: Updates.available
                    text: qsTr("Download and install")
                    accent: true
                    onClicked: Updates.install()
                }
                Text {
                    width: parent.width
                    visible: Updates.available
                    text: qsTr("The package manager asks for your password in the terminal.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    wrapMode: Text.Wrap
                }

                // What's new: the commits the update brings.
                Column {
                    width: parent.width
                    spacing: 4
                    visible: Updates.changes.length > 0
                    Text {
                        text: qsTr("What's new")
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.DemiBold
                    }
                    Repeater {
                        model: Updates.changes.slice(0, 8)
                        delegate: Text {
                            required property var modelData
                            width: parent.width
                            text: "• " + modelData
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideRight
                        }
                    }
                    Text {
                        width: parent.width
                        visible: Updates.changes.length > 8
                        text: qsTr("and %1 more").arg(Updates.changes.length - 8)
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                    }
                }
            }
        }
    }

    CardGroup {
        Card {
            icon: "folder-download"
            title: qsTr("Update source")
            description: qsTr("The Git repository or the folder the updates come from")
            contentItem: Column {
                width: parent.width
                spacing: 8
                Row {
                    width: parent.width
                    spacing: 8
                    TextBox {
                        id: sourceField
                        width: parent.width - saveButton.width - 8
                        text: Updates.source
                        placeholderText: "https://github.com/vela-desktop/vela.git"
                        onEditingFinished: Updates.setSource(text)
                    }
                    Button {
                        id: saveButton
                        text: qsTr("Save")
                        onClicked: {
                            Updates.setSource(sourceField.text)
                            sourceField.focus = false
                        }
                    }
                }
                Text {
                    width: parent.width
                    visible: Updates.source === ""
                    text: qsTr("The upstream repository, unless it is set.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
