// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtCore
import QtQml
import Vela.Controls

// The notes list, the window of Sticky Notes that stays: search, every note
// with its first line and date, and "+" for a new one. The notes themselves
// are the small windows of NoteWindow.qml, one per note that is open.
Window {
    id: root
    flags: Qt.Window | Qt.FramelessWindowHint // no title bar from Vela
    width: 440
    height: 620
    minimumWidth: 320
    minimumHeight: 360
    visible: true
    color: Theme.background
    title: qsTr("Sticky Notes")

    onActiveChanged: Theme.windowActive = active

    Settings {
        id: settings
        property string defaultColor: "yellow"
        property bool openAtStartup: true
    }

    // Notes keep their color names; the labels are translated here.
    function colorLabel(name) {
        switch (name) {
        case "yellow": return qsTr("Yellow")
        case "green": return qsTr("Green")
        case "pink": return qsTr("Pink")
        case "purple": return qsTr("Purple")
        case "blue": return qsTr("Blue")
        case "gray": return qsTr("Gray")
        default: return qsTr("Charcoal")
        }
    }
    readonly property var palette_: Notes.palette()
    function swatch(name) { return palette_.find(s => s.name === name) || palette_[0] }

    function showList() {
        visible = true
        if (visibility === Window.Minimized) showNormal()
        raise()
        requestActivate()
    }
    function openNote(id) {
        Notes.setOpen(id, true)
        for (let i = 0; i < windows.count; i++) {
            const w = windows.objectAt(i)
            if (w && w.noteId === id) {
                w.raise()
                w.requestActivate()
            }
        }
    }
    function newNote() {
        Notes.create(settings.defaultColor)
    }
    function quit() {
        Notes.quitting = true // the notes stay open for the next start
        Qt.quit()
    }

    Component.onCompleted: {
        if (!settings.openAtStartup) Notes.closeAll()
    }
    Connections {
        target: Instance
        function onActivated() { root.showList() }
    }

    // One window per open note.
    Instantiator {
        id: windows
        model: Notes.openIds
        delegate: NoteWindow {
            required property var modelData
            noteId: modelData
            host: root
        }
    }

    function when(date) {
        const now = new Date()
        if (date.toDateString() === now.toDateString()) return date.toLocaleTimeString(Qt.locale(), Locale.ShortFormat)
        return date.toLocaleDateString(Qt.locale(), date.getFullYear() === now.getFullYear() ? "d MMMM" : "d MMM yyyy")
    }

    Shortcut { sequence: "Ctrl+N"; onActivated: root.newNote() }
    Shortcut { sequence: "Ctrl+F"; onActivated: search.forceActiveFocus() }
    Shortcut { sequence: "Ctrl+W"; onActivated: root.close() }

    // --- title bar ---
    Item {
        id: strip
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 44

        DragHandler {
            target: null
            onActiveChanged: if (active) root.startSystemMove()
        }
        Image {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            source: "image://appicon/knotes"
            sourceSize: Qt.size(width, height)
        }
        Text {
            x: 44
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.text
            opacity: root.active ? 1 : 0.5
            font.pixelSize: Theme.fontCaption
        }
        Row {
            anchors { right: parent.right; top: parent.top }
            spacing: 0
            ToolButton {
                y: 6
                height: 32
                icon: "application-menu"
                tooltip: qsTr("Settings")
                onClicked: {
                    const p = mapToItem(null, width, height + 2)
                    menuLayer.open([
                        { text: qsTr("Default color"), children: root.palette_.map(s => ({
                            text: root.colorLabel(s.name),
                            checked: settings.defaultColor === s.name,
                            action: () => settings.defaultColor = s.name
                        })) },
                        { text: qsTr("Open notes at startup"), checked: settings.openAtStartup,
                          action: () => settings.openAtStartup = !settings.openAtStartup },
                        { separator: true },
                        { text: qsTr("Quit"), action: () => root.quit() }
                    ], p.x - 220, p.y, {})
                }
            }
            WindowButton { glyph: "M 0 0 L 10 10 M 10 0 L 0 10"; closes: true; onClicked: root.close() }
        }
    }

    // --- search and "+" ---
    Item {
        id: bar
        anchors { left: parent.left; right: parent.right; top: strip.bottom; leftMargin: 16; rightMargin: 16 }
        height: 40

        Rectangle {
            anchors { left: parent.left; right: add.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
            height: 32
            radius: Theme.radius
            color: search.activeFocus ? Theme.layer : Theme.control
            border.width: 1
            border.color: search.activeFocus ? Theme.accentFill : Theme.controlStroke
            Image {
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                width: 14
                height: 14
                source: Theme.icons + "edit-find"
                sourceSize: Qt.size(width, height)
            }
            TextInput {
                id: search
                anchors { fill: parent; leftMargin: 32; rightMargin: 10 }
                verticalAlignment: TextInput.AlignVCenter
                clip: true
                color: Theme.text
                selectionColor: Theme.accent
                selectedTextColor: "white"
                font.pixelSize: Theme.fontBody
                onTextChanged: Notes.filter = text
                Keys.onEscapePressed: {
                    text = ""
                    focus = false
                }
                Text {
                    visible: search.text === ""
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Search notes")
                    color: Theme.textTertiary
                    font.pixelSize: Theme.fontBody
                }
            }
        }
        ToolButton {
            id: add
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            icon: "list-add"
            tooltip: qsTr("New note")
            onClicked: root.newNote()
        }
    }

    // --- the notes ---
    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: bar.bottom; bottom: parent.bottom; topMargin: 8 }
        clip: true
        model: Notes
        spacing: 6
        leftMargin: 16
        rightMargin: 16
        bottomMargin: 16
        boundsBehavior: Flickable.StopAtBounds
        delegate: Rectangle {
            id: card
            required property int noteId
            required property string preview
            required property string noteColor
            required property date modified
            width: list.width - 32
            height: 64
            radius: Theme.radiusCard
            border.width: 1
            border.color: Theme.cardStroke
            color: cardMouse.pressed ? Theme.cardPressed : cardMouse.containsMouse ? Theme.cardHover : Theme.card

            Rectangle {
                id: chip
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                width: 12
                height: 36
                radius: 3
                color: root.swatch(card.noteColor).body
                border.width: 1
                border.color: Theme.stroke
            }
            Column {
                anchors { left: chip.right; leftMargin: 12; right: del.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
                spacing: 2
                Text {
                    width: parent.width
                    text: card.preview !== "" ? card.preview : qsTr("Empty note")
                    color: card.preview !== "" ? Theme.text : Theme.textTertiary
                    font.pixelSize: Theme.fontBody
                    elide: Text.ElideRight
                }
                Text {
                    text: root.when(card.modified)
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                }
            }
            HoverHandler { id: cardHover }
            MouseArea {
                id: cardMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.openNote(card.noteId)
            }
            ToolButton {
                id: del
                anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                icon: "edit-delete"
                tooltip: qsTr("Delete note")
                opacity: cardHover.hovered ? 1 : 0
                onClicked: {
                    confirm.target = card.noteId
                    confirm.open()
                }
            }
        }
        ScrollBar {
            flickable: list
            anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
        }
        Text {
            anchors.centerIn: parent
            visible: list.count === 0
            text: Notes.count === 0 ? qsTr("No notes yet. Press + to start one.") : qsTr("Nothing matches your search.")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontBody
        }
    }

    Dialog {
        id: confirm
        property int target: 0
        title: qsTr("Delete note?")
        text: qsTr("This note will be deleted for good.")
        buttons: [
            { text: qsTr("Delete"), accent: true, value: "delete" },
            { text: qsTr("Cancel"), value: "cancel" }
        ]
        onChosen: value => { if (value === "delete") Notes.remove(target) }
    }

    ResizeEdges {}

    MenuLayer {
        id: menuLayer
        anchors.fill: parent
        z: 100
    }
}
