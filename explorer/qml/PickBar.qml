// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The file chooser's bottom (vela-files --pick): the name, the type filter,
// and Open / Save / Cancel. What an app asked for is in Pick (picker.h).
Rectangle {
    id: bar
    property var win
    readonly property var tab: win ? win.current : null
    property int filter: Pick.currentFilter
    readonly property bool saving: Pick.mode !== "open"
    readonly property bool namesGiven: Pick.mode === "savefiles"
    readonly property string folder: tab && tab.isFolder ? tab.location : ""
    readonly property string acceptText: Pick.acceptLabel !== "" ? Pick.acceptLabel.replace("_", "")
        : saving ? qsTr("Save") : Pick.directory ? qsTr("Select Folder") : qsTr("Open")

    height: 64
    color: Theme.layer
    Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top } height: 1; color: Theme.divider }

    function applyFilter() {
        const f = Pick.filters[filter]
        if (tab) tab.setPickFilter(f ? f.globs : [])
    }
    onFilterChanged: applyFilter()
    onTabChanged: applyFilter()

    // A name is typed or taken from the selection.
    Connections {
        target: bar.tab ? bar.tab.model : null
        function onSelectionChanged() {
            const paths = bar.tab.model.selectedPaths()
            if (paths.length === 0 || (bar.saving && Pick.kind(paths[0]) === "dir")) return
            if (!Pick.directory && !(bar.saving && paths.length > 1))
                nameBox.text = paths.length === 1 ? paths[0].substring(paths[0].lastIndexOf("/") + 1) : ""
        }
    }

    function accept() {
        if (folder === "") return
        if (Pick.mode === "savefiles") { Pick.accept([folder], filter); return }
        const typed = nameBox.text.trim()
        const selected = tab.model ? tab.model.selectedPaths() : []
        const resolve = name => name.startsWith("/") ? name : folder + "/" + name
        if (Pick.directory) {
            const dirs = selected.filter(p => Pick.kind(p) === "dir")
            Pick.accept(dirs.length > 0 ? (Pick.multiple ? dirs : [dirs[0]]) : [folder], filter)
            return
        }
        if (saving) {
            if (typed === "") return
            const path = resolve(typed)
            const kind = Pick.kind(path)
            if (kind === "dir") { tab.navigate(path); nameBox.text = ""; return }
            if (kind === "file") {
                win.confirm(qsTr("Confirm Save As"), qsTr("\"%1\" already exists. Do you want to replace it?").arg(path.substring(path.lastIndexOf("/") + 1)),
                    qsTr("Replace"), () => Pick.accept([path], filter))
                return
            }
            Pick.accept([path], filter)
            return
        }
        let files = selected.filter(p => Pick.kind(p) === "file")
        if (files.length === 0 && typed !== "") {
            const path = resolve(typed)
            const kind = Pick.kind(path)
            if (kind === "dir") { tab.navigate(path); nameBox.text = ""; return }
            if (kind === "file") files = [path]
        }
        if (files.length === 0) return
        Pick.accept(Pick.multiple ? files : [files[0]], filter)
    }

    Row {
        anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
        spacing: 12
        Text {
            visible: !bar.namesGiven && !Pick.directory
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("File name:")
            color: Theme.text
            font.pixelSize: Theme.fontBody
        }
        TextBox {
            id: nameBox
            visible: !bar.namesGiven && !Pick.directory
            width: Math.max(160, bar.width - 520)
            text: Pick.currentName
            Keys.onReturnPressed: bar.accept()
            Keys.onEnterPressed: bar.accept()
        }
        Text {
            visible: bar.namesGiven
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("%n file(s) go in this folder", "", Pick.names.length)
            color: Theme.text
            font.pixelSize: Theme.fontBody
        }
    }
    Row {
        anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
        spacing: 8
        ToolButton {
            id: filterButton
            visible: Pick.filters.length > 1 && !Pick.directory
            anchors.verticalCenter: parent.verticalCenter
            width: 180
            text: Pick.filters[bar.filter] ? Pick.filters[bar.filter].name : ""
            menu: true
            onClicked: {
                const p = filterButton.mapToItem(null, 0, 0)
                bar.tab.menus.open(Pick.filters.map((f, i) => ({ text: f.name, action: () => { bar.filter = i } })), p.x, p.y - 4 - 32 * Pick.filters.length)
            }
        }
        Button { text: bar.acceptText; accent: true; usable: bar.folder !== ""; onClicked: bar.accept() }
        Button { text: qsTr("Cancel"); onClicked: Pick.cancel() }
    }
}
