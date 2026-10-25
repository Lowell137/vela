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

    // The state of a background update (Updates.jobs): running, the error (and
    // what it said) or done, and a way to see the terminal output.
    component JobStatus: Column {
        id: job
        property string name
        // False where the page already shows its own Show terminal.
        property bool showTerminal: true
        readonly property var info: Updates.jobs[name] || ({ state: "idle", message: "" })
        width: parent ? parent.width : 0
        visible: info.state !== "idle"
        spacing: 4
        Text {
            width: parent.width
            text: job.info.state === "running" ? qsTr("Updating in the background...")
                : job.info.state === "done" ? qsTr("Done.")
                : qsTr("It failed: %1").arg(job.info.message)
            color: job.info.state === "error" ? Theme.critical : Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            wrapMode: Text.Wrap
        }
        Button {
            visible: job.showTerminal
            subtle: true
            text: qsTr("Show terminal")
            onClicked: Updates.showJob(job.name)
        }
    }

    Component.onCompleted: {
        Updates.checkIfDue()
        Updates.checkFlatpak()
        Updates.checkPackages()
    }

    // A label and its value, one under the other at the right: the versions.
    component VersionRow: Row {
        property string label
        property string value
        property string detail
        width: parent ? parent.width : 0
        spacing: 12
        Text {
            width: 110
            text: parent.label
            color: Theme.textSecondary
            font.pixelSize: Theme.fontBody
        }
        Column {
            width: parent.width - 122
            Text {
                width: parent.width
                text: parent.parent.value
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                visible: text !== ""
                text: parent.parent.detail
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                elide: Text.ElideRight
            }
        }
    }

    // "0.1.0.r420.g618a4d7-1": Vela 0.1.0, build 420 (the commit 618a4d7).
    function describe(version) {
        const m = /^([0-9.]+)\.r([0-9]+)\.g([0-9a-f]+)/.exec(version || "")
        return m ? qsTr("Vela %1, build %2 (%3)").arg(m[1]).arg(m[2]).arg(m[3]) : version
    }

    readonly property string status: {
        if (Updates.status === "checking") {
            return qsTr("Checking for updates...")
        }
        if (Updates.status === "available") {
            return qsTr("A new version is ready to install")
        }
        if (Updates.restartNeeded && Updates.status !== "updating") {
            return qsTr("Installed: log out to start using it")
        }
        if (Updates.status === "up-to-date") {
            return qsTr("You have the latest version")
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
                VersionRow {
                    visible: Updates.installedVersion !== ""
                    label: qsTr("Installed")
                    value: page.describe(Updates.installedVersion)
                    detail: Updates.installedAt !== "" ? qsTr("Installed on %1").arg(Updates.installedAt) : ""
                }
                VersionRow {
                    visible: Updates.available && Updates.newRevision !== ""
                    label: qsTr("New version")
                    value: qsTr("Commit %1").arg(Updates.newRevision.slice(0, 7))
                    detail: Updates.changes.length > 0 ? qsTr("%n change(s) since yours", "", Updates.changes.length) : ""
                }
                VersionRow {
                    visible: Updates.lastCheck !== ""
                    label: qsTr("Last checked")
                    value: Updates.lastCheck
                }

                // Installed after this session started: what runs is still
                // the previous Vela (the update works from the next login).
                Rectangle {
                    width: parent.width
                    visible: Updates.restartNeeded && Updates.status !== "updating"
                    height: restartColumn.implicitHeight + 24
                    radius: Theme.radius
                    color: Theme.card
                    border.width: 1
                    border.color: Theme.cardStroke
                    Column {
                        id: restartColumn
                        x: 12
                        y: 12
                        width: parent.width - 24
                        spacing: 8
                        Text {
                            width: parent.width
                            text: qsTr("This session started before Vela was installed, so you are still using the previous version. Log out and back in to use the new one.")
                            color: Theme.text
                            font.pixelSize: Theme.fontCaption
                            wrapMode: Text.Wrap
                        }
                        Button {
                            accent: true
                            text: qsTr("Log out")
                            onClicked: Updates.logOut()
                        }
                    }
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
                        text: qsTr("Show terminal")
                        onClicked: Updates.showJob("vela")
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
                    text: qsTr("The system asks for your password in its own window when it is needed.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    wrapMode: Text.Wrap
                }

                JobStatus { name: "vela"; showTerminal: Updates.status !== "updating" }

                // What's new: the commits the update brings (or brought, when
                // it was installed since the last look).
                Column {
                    width: parent.width
                    spacing: 4
                    visible: Updates.changes.length > 0
                    Text {
                        text: Updates.available ? qsTr("What's new in this update") : qsTr("What the last update brought")
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
        Card {
            visible: Updates.pacmanAvailable
            icon: "system-software-update"
            title: qsTr("System packages (pacman)")
            contentItem: JobStatus { name: "pacman" }
            description: Updates.packagesChecking ? qsTr("Checking...")
                : Updates.pacmanUpdates.length === 0 ? qsTr("Up to date")
                : qsTr("%n update(s): %1", "", Updates.pacmanUpdates.length).arg(Updates.pacmanUpdates.slice(0, 3).map(l => l.split(" ")[0]).join(", "))
            trailing: Row {
                spacing: 8
                Button {
                    text: qsTr("Check")
                    usable: !Updates.packagesChecking
                    onClicked: Updates.checkPackages()
                }
                Button {
                    visible: Updates.pacmanUpdates.length > 0
                    accent: true
                    text: qsTr("Update")
                    onClicked: Updates.updatePacman()
                }
            }
        }
        Card {
            visible: Updates.aurAvailable
            icon: "applications-other"
            title: qsTr("AUR packages")
            contentItem: JobStatus { name: "aur" }
            description: Updates.packagesChecking ? qsTr("Checking...")
                : Updates.aurUpdates.length === 0 ? qsTr("Up to date")
                : qsTr("%n update(s): %1", "", Updates.aurUpdates.length).arg(Updates.aurUpdates.slice(0, 3).map(l => l.split(" ")[0]).join(", "))
            trailing: Row {
                spacing: 8
                Button {
                    text: qsTr("Check")
                    usable: !Updates.packagesChecking
                    onClicked: Updates.checkPackages()
                }
                Button {
                    visible: Updates.aurUpdates.length > 0
                    accent: true
                    text: qsTr("Update")
                    onClicked: Updates.updateAur()
                }
            }
        }
    }

    CardGroup {
        visible: Updates.flatpakInstalled
        Card {
            icon: "flatpak-discover"
            title: qsTr("Flatpak apps")
            contentItem: JobStatus { name: "flatpak" }
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
                JobStatus { name: "rollback" }
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
                        onEditingFinished: Updates.source = text
                    }
                    Button {
                        id: saveButton
                        text: qsTr("Save")
                        onClicked: {
                            Updates.source = sourceField.text
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
