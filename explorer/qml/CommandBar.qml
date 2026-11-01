// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The Windows 11 command bar: New; Cut, Copy, Paste, Rename, Share, Delete;
// Sort and View; "…" with the rest. In the Recycle Bin: Empty Recycle Bin and
// Restore.
Item {
    id: bar
    property var tab
    height: 48

    readonly property bool hasSelection: tab.model && tab.model.selectionCount > 0
    readonly property bool writable: tab.isFolder && !tab.isTrash

    // The one selected file, when it is a picture: File Explorer's picture commands.
    readonly property string picture: {
        if (!tab.model || tab.model.selectionCount !== 1) return ""
        const path = tab.model.selectedPaths()[0]
        return Ops.isImage(path) ? path : ""
    }
    function rotate(degrees) {
        if (Ops.rotateImage(picture, degrees)) tab.refresh()
    }

    function openMenu(entries, item) {
        const p = item.mapToItem(null, 0, item.height)
        tab.menus.open(entries, p.x, p.y + 2)
    }

    Row {
        x: 6
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0

        ToolButton {
            id: newButton
            visible: !bar.tab.isTrash
            icon.name: "list-add"
            text: qsTr("New")
            menu: true
            usable: bar.writable
            onClicked: bar.openMenu(bar.tab.newEntries(), newButton)
        }
        Item { visible: !bar.tab.isTrash; width: 11; height: 24; Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 1; height: 24; anchors.verticalCenter: parent.verticalCenter; color: Theme.divider } }

        ToolButton { visible: !bar.tab.isTrash; icon.name: "edit-cut"; tooltip: qsTr("Cut (Ctrl+X)"); usable: bar.hasSelection && bar.writable; onClicked: bar.tab.cutSelection() }
        Item { visible: !bar.tab.isTrash; width: 8; height: 1 }
        ToolButton { visible: !bar.tab.isTrash; icon.name: "edit-copy"; tooltip: qsTr("Copy (Ctrl+C)"); usable: bar.hasSelection; onClicked: bar.tab.copySelection() }
        Item { visible: !bar.tab.isTrash; width: 8; height: 1 }
        ToolButton { visible: !bar.tab.isTrash; icon.name: "edit-paste"; tooltip: qsTr("Paste (Ctrl+V)"); usable: Ops.canPaste && bar.writable; onClicked: bar.tab.paste() }
        Item { visible: !bar.tab.isTrash; width: 8; height: 1 }
        ToolButton { visible: !bar.tab.isTrash; icon.name: "edit-rename"; tooltip: qsTr("Rename (F2)"); usable: bar.tab.model && bar.tab.model.selectionCount === 1 && bar.writable; onClicked: bar.tab.renameSelection() }
        Item { visible: !bar.tab.isTrash; width: 8; height: 1 }
        ToolButton {
            visible: !bar.tab.isTrash
            icon.name: "document-share"
            tooltip: qsTr("Share")
            usable: bar.hasSelection
            onClicked: {
                const m = bar.tab.model
                const files = m.selectedPaths().filter(p => !m.isDirAt(m.indexOf(p)))
                if (files.length > 0) Ops.share(files)
            }
        }
        Item { visible: !bar.tab.isTrash; width: 8; height: 1 }
        ToolButton { visible: !bar.tab.isTrash; icon.name: "edit-delete"; tooltip: qsTr("Delete (Del)"); usable: bar.hasSelection && bar.writable; onClicked: bar.tab.deleteSelection(false) }

        // In the Recycle Bin.
        ToolButton { visible: bar.tab.isTrash; icon.name: "trash-empty"; text: qsTr("Empty Recycle Bin"); usable: bar.tab.model && bar.tab.model.count > 0; onClicked: bar.tab.askEmptyTrash() }
        ToolButton { visible: bar.tab.isTrash; icon.name: "edit-undo"; text: qsTr("Restore the selected items"); usable: bar.hasSelection; onClicked: Ops.restoreFromTrash(bar.tab.model.selectedPaths()) }

        Item { width: 11; height: 24; Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 1; height: 24; anchors.verticalCenter: parent.verticalCenter; color: Theme.divider } }

        ToolButton {
            id: sortButton
            icon.name: "view-sort"
            text: qsTr("Sort")
            menu: true
            usable: bar.tab.isFolder
            onClicked: bar.openMenu(bar.tab.sortEntries(), sortButton)
        }
        Item { width: 4; height: 1 }
        ToolButton {
            id: viewButton
            // File Explorer's: the icon of the view in use.
            icon.name: ({ extralarge: "view-extra-large", large: "view-large", medium: "view-medium", small: "view-small",
                          list: "view-list-text", details: "view-list-details", tiles: "view-tiles", content: "view-content" })[bar.tab.viewMode] || "view-list-details"
            text: qsTr("View")
            menu: true
            usable: bar.tab.isFolder
            onClicked: bar.openMenu(bar.tab.viewEntries(), viewButton)
        }
        Item { visible: bar.picture !== ""; width: 11; height: 24; Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 1; height: 24; anchors.verticalCenter: parent.verticalCenter; color: Theme.divider } }
        ToolButton {
            visible: bar.picture !== ""
            icon.name: "preferences-desktop-wallpaper"
            text: qsTr("Set as background")
            onClicked: Ops.setWallpaper(bar.picture)
        }
        Item { visible: bar.picture !== ""; width: 8; height: 1 }
        ToolButton {
            visible: bar.picture !== ""
            icon.name: "object-rotate-left"
            text: qsTr("Rotate left")
            onClicked: bar.rotate(-90)
        }
        Item { width: 11; height: 24; Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 1; height: 24; anchors.verticalCenter: parent.verticalCenter; color: Theme.divider } }
        ToolButton {
            id: moreButton
            icon.name: "view-more-horizontal-symbolic"
            icon.width: 20 // File Explorer's dots are 16 wide
            icon.height: 20
            tooltip: qsTr("See more")
            usable: bar.tab.isFolder
            onClicked: bar.openMenu((bar.picture !== "" ? [
                { text: qsTr("Rotate &right"), icon: "object-rotate-right", action: () => bar.rotate(90) },
                { separator: true }
            ] : []).concat(bar.tab.moreEntries()), moreButton)
        }
    }

    // On the right, like Windows 11: the pane in use, Preview or Details.
    ToolButton {
        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
        readonly property bool preview: bar.tab.win.pane !== "details"
        icon.name: preview ? "view-preview" : "help-about"
        text: preview ? qsTr("Preview") : qsTr("Details")
        tooltip: preview ? qsTr("Preview pane (Alt+P)") : qsTr("Details pane (Alt+Shift+P)")
        checked: bar.tab.win.pane !== ""
        usable: bar.tab.isFolder
        onClicked: bar.tab.win.togglePane(preview ? "preview" : "details")
    }
}
