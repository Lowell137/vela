// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls
import Vela.Notepad.Backend

// One tab: a file (Document) and the text being edited.
Item {
    id: page
    property var win // Main.qml: the font, the zoom, word wrap, the menus
    readonly property alias doc: doc
    readonly property alias edit: edit
    readonly property string title: doc.name !== "" ? doc.name : qsTr("Untitled")
    // (line, column) of the cursor, for the status bar and Go to.
    readonly property point cursor: doc.lineColumn(edit.cursorPosition)

    function focusView() { edit.forceActiveFocus() }

    // The next or previous occurrence, selected and scrolled into view.
    function find(needle, caseSensitive, backward) {
        const from = backward ? edit.selectionStart : edit.selectionEnd
        const found = doc.find(needle, from, caseSensitive, backward)
        if (found.x < 0) return false
        edit.select(found.x, found.y)
        return true
    }
    // Replaces the selected occurrence, if the selection is one, and goes to
    // the next.
    function replace(needle, replacement, caseSensitive) {
        const found = doc.find(needle, edit.selectionStart, caseSensitive, false)
        const replaced = found.x >= 0 && found.x === edit.selectionStart && found.y === edit.selectionEnd
        if (replaced) {
            edit.remove(found.x, found.y)
            edit.insert(found.x, replacement)
        }
        return find(needle, caseSensitive, false) || replaced
    }
    function goToLine(line) {
        edit.cursorPosition = doc.positionOfLine(line)
        edit.forceActiveFocus()
    }

    function contextMenu(x, y) {
        const p = edit.mapToItem(null, x, y)
        const selected = edit.selectedText !== ""
        page.win.openMenu([
            { text: qsTr("&Undo"), icon: "edit-undo", shortcut: "Ctrl+Z", enabled: edit.canUndo, action: () => edit.undo() },
            { separator: true },
            { text: qsTr("Cu&t"), icon: "edit-cut", shortcut: "Ctrl+X", enabled: selected, action: () => edit.cut() },
            { text: qsTr("&Copy"), icon: "edit-copy", shortcut: "Ctrl+C", enabled: selected, action: () => edit.copy() },
            { text: qsTr("&Paste"), icon: "edit-paste", shortcut: "Ctrl+V", enabled: edit.canPaste, action: () => edit.paste() },
            { text: qsTr("&Delete"), icon: "edit-delete", shortcut: "Del", enabled: selected, action: () => edit.remove(edit.selectionStart, edit.selectionEnd) },
            { separator: true },
            { text: qsTr("Select &all"), icon: "edit-select-all", shortcut: "Ctrl+A", action: () => edit.selectAll() }
        ], p.x, p.y)
    }

    Document {
        id: doc
        textDocument: edit.textDocument
    }

    FontMetrics {
        id: metrics
        font: edit.font
    }

    Flickable {
        id: flick
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: page.win.wordWrap ? width : Math.max(width, Math.ceil(edit.contentWidth) + edit.leftPadding + edit.rightPadding)
        contentHeight: Math.max(height, edit.implicitHeight)

        function ensureVisible(r) {
            if (contentX >= r.x) contentX = Math.max(0, r.x - edit.leftPadding)
            else if (contentX + width <= r.x + r.width) contentX = r.x + r.width - width + edit.rightPadding
            if (contentY >= r.y) contentY = Math.max(0, r.y - edit.topPadding)
            else if (contentY + height <= r.y + r.height) contentY = r.y + r.height - height + edit.bottomPadding
        }

        TextEdit {
            id: edit
            width: flick.contentWidth
            height: flick.contentHeight
            focus: true
            leftPadding: 14
            rightPadding: 18 // clear of the scroll bar
            topPadding: 8
            bottomPadding: 8
            textFormat: TextEdit.PlainText
            wrapMode: page.win.wordWrap ? TextEdit.Wrap : TextEdit.NoWrap
            font.family: page.win.fontFamily
            font.pixelSize: Math.max(1, Math.round(page.win.fontSize * page.win.zoom / 100))
            tabStopDistance: metrics.advanceWidth(" ") * 8
            color: Theme.text
            selectionColor: Theme.accent
            selectedTextColor: "white"
            selectByMouse: true
            persistentSelection: true // Find keeps the focus while the match shows
            onCursorRectangleChanged: flick.ensureVisible(cursorRectangle)

            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: point => page.contextMenu(point.position.x, point.position.y)
            }
            // Ctrl+wheel zooms, like Windows.
            WheelHandler {
                acceptedModifiers: Qt.ControlModifier
                onWheel: wheel => page.win.zoomBy(wheel.angleDelta.y > 0 ? 10 : -10)
            }
        }
    }
    ScrollBar {
        flickable: flick
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom; rightMargin: 2; topMargin: 2; bottomMargin: 2 }
    }
    // Without word wrap the lines go past the edge: the same bar, lying
    // down (the Explorer's only stands).
    Item {
        id: across
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 2; rightMargin: 14; bottomMargin: 2 }
        height: 10
        visible: flick.contentWidth > flick.width
        Rectangle {
            id: thumb
            anchors.bottom: parent.bottom
            x: flick.contentX / flick.contentWidth * across.width
            width: Math.max(24, flick.width / flick.contentWidth * across.width)
            height: acrossMouse.containsMouse || acrossMouse.pressed ? 6 : 2
            radius: height / 2
            color: acrossMouse.containsMouse || acrossMouse.pressed ? Theme.scrollThumbHover : Theme.scrollThumb
        }
        MouseArea {
            id: acrossMouse
            anchors.fill: parent
            hoverEnabled: true
            property real grab: 0
            onPressed: mouse => {
                grab = mouse.x - thumb.x
                if (grab < 0 || grab > thumb.width) {
                    grab = thumb.width / 2
                    move(mouse.x)
                }
            }
            onPositionChanged: mouse => { if (pressed) move(mouse.x) }
            function move(x) {
                const left = Math.max(0, Math.min(across.width - thumb.width, x - grab))
                flick.contentX = Math.min(flick.contentWidth - flick.width, left / across.width * flick.contentWidth)
            }
        }
    }
}
