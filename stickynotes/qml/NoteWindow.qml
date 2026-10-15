// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// One note: a small frameless window in its color, a strip with "+", the
// color and delete buttons and close, and the text. Plain text only
// (shortcut: no bold, italic or lists, when rich text is wanted use
// TextEdit.RichText and store HTML).
Window {
    id: win
    required property int noteId
    required property var host // the list window, for the palette and new notes

    flags: Qt.Window | Qt.FramelessWindowHint
    minimumWidth: 200
    minimumHeight: 160
    visible: true
    title: qsTr("Sticky Note")

    property var note: Notes.get(noteId)
    width: note.width
    height: note.height
    property bool ready: false
    property string colorName: note.color
    readonly property var swatch: host.swatch(colorName)
    readonly property bool dark: colorName === "charcoal"
    // The glyphs follow the note, not the theme: pastel or charcoal either way.
    readonly property string icons: dark ? "image://icon/w/" : "image://icon/l/"
    color: swatch.body

    Component.onCompleted: {
        // shortcut: x/y of a frameless window are the compositor's on Wayland,
        // so only the size is restored; they are stored for when it honors them.
        if (note.x !== 0 || note.y !== 0) {
            x = note.x
            y = note.y
        }
        body.text = note.text
        ready = true
    }
    // Only a focused window is being resized by its user: the compositor
    // also sizes windows when they open, which must not overwrite the note.
    // shortcut: it may open at the compositor's size, not the stored one, until
    // Vela honors a client's requested size for toplevels.
    function remember() { if (ready && active) Notes.setGeometry(noteId, x, y, width, height) }
    onWidthChanged: remember()
    onHeightChanged: remember()
    onXChanged: remember()
    onYChanged: remember()
    onClosing: Notes.setOpen(noteId, false)

    function colorMenu(x, y) {
        menuLayer.open(host.palette_.map(s => ({
            text: host.colorLabel(s.name),
            checked: s.name === win.colorName,
            action: () => { win.colorName = s.name; Notes.setColor(win.noteId, s.name) }
        })), x, y, {})
    }
    function askDelete() {
        // An empty note goes without asking.
        if (body.text.trim() === "") Notes.remove(noteId)
        else confirm.open()
    }

    Shortcut { sequence: "Ctrl+N"; onActivated: host.newNote() }
    Shortcut { sequence: "Ctrl+W"; onActivated: win.close() }

    component StripButton: Rectangle {
        id: sb
        property string icon
        property string tip
        signal clicked()
        width: 32
        height: 32
        color: m.pressed ? Qt.rgba(0, 0, 0, 0.16) : m.containsMouse ? Qt.rgba(0, 0, 0, 0.08) : "transparent"
        Image {
            anchors.centerIn: parent
            width: 16
            height: 16
            source: win.icons + sb.icon
            sourceSize: Qt.size(width, height)
        }
        MouseArea { id: m; anchors.fill: parent; hoverEnabled: true; onClicked: sb.clicked() }
    }

    Rectangle {
        id: strip
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 32
        color: win.swatch.strip

        DragHandler {
            target: null
            onActiveChanged: if (active) win.startSystemMove()
        }
        StripButton { icon: "list-add"; onClicked: win.host.newNote() }
        Row {
            anchors.right: parent.right
            StripButton {
                id: colorButton
                icon: "color-fill"
                onClicked: {
                    const p = mapToItem(null, 0, height)
                    win.colorMenu(p.x, p.y)
                }
            }
            StripButton { icon: "edit-delete"; onClicked: win.askDelete() }
            StripButton { icon: "tab-close"; onClicked: win.close() }
        }
    }

    Flickable {
        id: flick
        anchors { left: parent.left; right: parent.right; top: strip.bottom; bottom: parent.bottom }
        clip: true
        contentWidth: width
        contentHeight: Math.max(height, body.contentHeight + 24)
        boundsBehavior: Flickable.StopAtBounds
        // Keep the caret in view while typing.
        function follow() {
            const r = body.cursorRectangle
            if (r.y < contentY) contentY = r.y
            else if (r.y + r.height + 12 > contentY + height) contentY = r.y + r.height + 12 - height
        }
        TextEdit {
            id: body
            x: 12
            y: 8
            width: flick.width - 24
            height: Math.max(flick.height - 16, contentHeight)
            wrapMode: TextEdit.Wrap
            textFormat: TextEdit.PlainText
            color: win.swatch.text
            selectionColor: Theme.accent
            selectedTextColor: "white"
            font.pixelSize: Theme.fontBody
            focus: true
            onTextChanged: Notes.setText(win.noteId, text)
            onCursorRectangleChanged: flick.follow()
            Text {
                visible: body.text === ""
                text: qsTr("Take a note...")
                color: win.swatch.text
                opacity: 0.5
                font.pixelSize: Theme.fontBody
            }
        }
    }

    Dialog {
        id: confirm
        title: qsTr("Delete note?")
        text: qsTr("This note will be deleted for good.")
        buttons: [
            { text: qsTr("Delete"), accent: true, value: "delete" },
            { text: qsTr("Cancel"), value: "cancel" }
        ]
        onChosen: value => {
            if (value === "delete") Notes.remove(win.noteId)
            else body.forceActiveFocus()
        }
    }

    ResizeEdges {}

    MenuLayer {
        id: menuLayer
        anchors.fill: parent
        z: 100
    }
}
