// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// System > Updates, like Windows 11's Windows Update: whether there's a
// newer Vela, what it brings, where the updates come from, whether the page
// checks by itself, and the packages to go back to. The updater is the
// vela-update script, the same one the command line uses.
Page {
    id: page

    Component.onCompleted: {
        Updates.checkIfDue()
        Updates.checkFlatpak()
    }

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
        if (Updates.status === "recently-installed") {
            return Updates.message
        }
        if (Updates.status === "updating") {
            return Updates.message
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
                text: Updates.busy ? qsTr("Checking...") : qsTr("Check for updates")
                usable: !Updates.busy
                onClicked: Updates.check()
            }
            contentItem: Column {
                width: parent.width
                spacing: 8

                Text {
                    width: parent.width
                    visible: Updates.paused
                    // Like Windows' "You've paused updates on {date}".
                    text: qsTr("Updates are paused until %1.").arg(Updates.pausedUntil)
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    wrapMode: Text.Wrap
                }
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
                    visible: Updates.lastCheck !== ""
                    text: qsTr("Last checked: %1").arg(Updates.lastCheck)
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
                Row {
                    visible: Updates.status === "updating"
                    spacing: 8
                    Button {
                        text: qsTr("Update in another terminal")
                        onClicked: Updates.install()
                    }
                    Button {
                        // The restarter: the update works from the next
                        // session, so end this one (like Windows Update's
                        // "Restart now").
                        accent: true
                        text: qsTr("Log out to finish")
                        onClicked: Updates.logOut()
                    }
                }
                Text {
                    width: parent.width
                    visible: Updates.status === "updating"
                    text: qsTr("The package manager asks for your password in the terminal.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    wrapMode: Text.Wrap
                }

                // What's new: the commits the update brings (or brought, when
                // it was installed since the last look).
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
            icon: "calendar" // "plate"? The schedule of the automatic checks.
            title: qsTr("Update options")
            description: qsTr("How often the page checks by itself, and the pause")
            trailing: Choice {
                model: [qsTr("Daily"), qsTr("Weekly"), qsTr("Never")]
                currentIndex: Updates.autoCheck === "weekly" ? 1 : Updates.autoCheck === "never" ? 2 : 0
                onChosen: index => Updates.autoCheck = index === 1 ? "weekly" : index === 2 ? "never" : "daily"
            }
        }
        Card {
            icon: "media-playback-pause"
            title: qsTr("Pause updates")
            description: Updates.paused ? qsTr("Resumed on %1.").arg(Updates.pausedUntil) : qsTr("No checks for seven days, then they start again")
            trailing: Button {
                text: Updates.paused ? qsTr("Resume") : qsTr("For a week")
                onClicked: {
                    if (Updates.paused) {
                        Updates.pausedUntil = ""
                        return
                    }
                    const day = new Date()
                    day.setDate(day.getDate() + 7)
                    const month = day.getMonth() + 1
                    Updates.pausedUntil = day.getFullYear() + "-" + (month < 10 ? "0" : "") + month + "-"
                        + (day.getDate() < 10 ? "0" : "") + day.getDate()
                }
            }
        }
    }

    CardGroup {
        visible: Updates.flatpakInstalled
        Card {
            icon: "flatpak-discover"
            title: qsTr("Flatpak apps")
            description: Updates.flatpakChecking ? qsTr("Checking...")
                : Updates.flatpakUpdates.length === 0 ? qsTr("Your Flatpak apps are up to date")
                : qsTr("%n update(s): %1", "", Updates.flatpakUpdates.length).arg(Updates.flatpakUpdates.slice(0, 4).join(", "))
            trailing: Row {
                spacing: 8
                Button {
                    text: qsTr("Check")
                    usable: !Updates.flatpakChecking
                    onClicked: Updates.checkFlatpak()
                }
                Button {
                    visible: Updates.flatpakUpdates.length > 0
                    accent: true
                    text: qsTr("Update")
                    onClicked: Updates.updateFlatpak()
                }
            }
        }
    }

    CardGroup {
        visible: Updates.rollbackTargets.length > 0
        Card {
            icon: "edit-undo"
            title: qsTr("Rollback")
            description: qsTr("Go back to a package the updater keeps in its cache")
            contentItem: Column {
                width: parent.width
                spacing: 4
                Repeater {
                    model: Updates.rollbackTargets.slice(0, 5)
                    delegate: Row {
                        id: row
                        required property var modelData
                        width: parent.width
                        Text {
                            width: parent.width - goBack.width - 8
                            text: row.modelData
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideRight
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Button {
                            id: goBack
                            text: qsTr("Roll back")
                            onClicked: Updates.rollback(row.modelData)
                        }
                    }
                }
                Text {
                    width: parent.width
                    visible: Updates.rollbackTargets.length > 5
                    text: qsTr("and %1 more").arg(Updates.rollbackTargets.length - 5)
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
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
