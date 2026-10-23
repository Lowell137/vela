// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Spotlight (Win+Space, or the search button of the top panel): a centered
// search for apps and files. Enter opens the selection, Esc or a click
// elsewhere closes it.
Window {
    id: root
    visible: false
    width: 640
    height: 56 + 5 * 48 + 3 * 48 + 3 * 48 + 24
    color: "transparent"

    readonly property int appRows: Math.min(apps.count, 5)
    readonly property int fileRows: Math.min(FileSearch.results.length, 3)
    // Settings pages that match, below the apps.
    readonly property var settingsHits: {
        const q = field.text.trim().toLowerCase()
        if (q.length === 0) return []
        return Menus.settingsIndex.filter(e => e.text.toLowerCase().indexOf(q) >= 0 || e.keys.toLowerCase().indexOf(q) >= 0).slice(0, 3)
    }
    readonly property int settingsRows: settingsHits.length
    readonly property int total: appRows + settingsRows + fileRows
    property int sel: 0

    function open() {
        field.text = ""
        sel = 0
        visible = true
        field.forceActiveFocus()
        updateBlur()
    }
    function close() {
        visible = false
        Apps.query = ""
        FileSearch.query = ""
    }
    function accept() {
        if (total === 0) return
        if (sel < appRows) {
            if (Apps.launch(sel)) close()
        } else if (sel < appRows + settingsRows) {
            System.openSettingsPage(settingsHits[sel - appRows].page)
            close()
        } else {
            System.openUrl(FileSearch.results[sel - appRows - settingsRows].path)
            close()
        }
    }
    function updateBlur() {
        Effects.setBlur(root, [Qt.rect(card.x, card.y, card.width, card.height)], card.radius)
    }
    onActiveChanged: if (!active && visible) close()

    Connections {
        target: Shell
        function onSpotlightRequested() {
            if (root.visible) root.close()
            else root.open()
        }
    }

    Rectangle {
        id: card
        width: parent.width
        height: 56 + (root.total > 0 ? 8 + root.total * 48 : 0) + 8
        radius: 18
        color: Theme.popup
        border.width: 1
        border.color: Theme.stroke
        onHeightChanged: root.updateBlur()
        Behavior on height { NumberAnimation { duration: Theme.fast } }

        Image {
            x: 20
            y: 20
            width: 16
            height: 16
            source: Theme.icons + "search"
            sourceSize: Qt.size(width, height)
        }
        TextInput {
            id: field
            x: 48
            y: 0
            width: parent.width - 68
            height: 56
            verticalAlignment: TextInput.AlignVCenter
            color: Theme.text
            font.pixelSize: Theme.fontBody + 3
            selectByMouse: true
            clip: true
            onTextChanged: {
                Apps.query = text
                FileSearch.query = text
                root.sel = 0
            }
            Keys.onEscapePressed: root.close()
            Keys.onReturnPressed: root.accept()
            Keys.onEnterPressed: root.accept()
            Keys.onDownPressed: root.sel = Math.min(root.total - 1, root.sel + 1)
            Keys.onUpPressed: root.sel = Math.max(0, root.sel - 1)
            Text {
                visible: field.text.length === 0
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Spotlight search")
                color: Theme.textSecondary
                font: field.font
            }
        }

        Column {
            y: 64
            width: parent.width
            // The app list only measures and feeds the rows.
            ListView {
                id: apps
                width: parent.width
                height: root.appRows * 48
                interactive: false
                model: Apps
                delegate: Item {
                    id: row
                    required property int index
                    required property string name
                    required property string iconName
                    required property string comment
                    visible: index < 5
                    width: apps.width
                    height: index < 5 ? 48 : 0
                    Rectangle {
                        anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                        radius: Theme.radius
                        color: Theme.selection
                        opacity: root.sel === row.index ? 1 : (rowMouse.containsMouse ? 0.5 : 0)
                    }
                    Image {
                        x: 20
                        anchors.verticalCenter: parent.verticalCenter
                        width: 28
                        height: 28
                        source: Theme.icons + encodeURIComponent(row.iconName)
                        sourceSize: Qt.size(width, height)
                    }
                    Text {
                        x: 60
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 80
                        text: row.name
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: { root.sel = row.index; root.accept() }
                    }
                }
            }
            Repeater {
                model: root.settingsRows
                delegate: Item {
                    id: srow
                    required property int index
                    readonly property var hit: root.settingsHits[index]
                    width: card.width
                    height: 48
                    Rectangle {
                        anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                        radius: Theme.radius
                        color: Theme.selection
                        opacity: root.sel === root.appRows + srow.index ? 1 : (settingsMouse.containsMouse ? 0.5 : 0)
                    }
                    Image {
                        x: 20
                        anchors.verticalCenter: parent.verticalCenter
                        width: 20
                        height: 20
                        source: Theme.icons + "configure"
                        sourceSize: Qt.size(width, height)
                    }
                    Text {
                        x: 60
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 80
                        text: srow.hit ? srow.hit.text + "  ·  " + qsTr("Settings") : ""
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        id: settingsMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: { root.sel = root.appRows + srow.index; root.accept() }
                    }
                }
            }
            Repeater {
                model: root.fileRows
                delegate: Item {
                    id: frow
                    required property int index
                    readonly property var file: FileSearch.results[index]
                    width: card.width
                    height: 48
                    Rectangle {
                        anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                        radius: Theme.radius
                        color: Theme.selection
                        opacity: root.sel === root.appRows + root.settingsRows + frow.index ? 1 : (fileMouse.containsMouse ? 0.5 : 0)
                    }
                    Column {
                        x: 20
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 40
                        Text {
                            width: parent.width
                            text: frow.file ? frow.file.name : ""
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: frow.file ? frow.file.path.replace(System.homeDirectory(), "~") : ""
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideMiddle
                        }
                    }
                    MouseArea {
                        id: fileMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: { root.sel = root.appRows + root.settingsRows + frow.index; root.accept() }
                    }
                }
            }
        }
    }
}
