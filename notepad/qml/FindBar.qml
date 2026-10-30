// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Find and Replace, like Windows 11's Notepad: a bar floating over the top of
// the text; the arrow on the left opens the Replace row. Enter: next,
// Shift+Enter: previous, Esc: closes.
Rectangle {
    id: bar
    property var tab: null // the open tab (Editor.qml)
    property bool replacing: false
    property bool matchCase: false
    property bool missed: false // the last search found nothing
    readonly property bool hasText: findField.text !== ""
    signal closed()

    width: 420
    height: column.height + 16
    radius: Theme.radiusOverlay
    color: Theme.flyout
    border.width: 1
    border.color: Theme.stroke

    function show(withReplace) {
        replacing = withReplace
        visible = true
        if (tab && tab.edit.selectedText !== "" && tab.edit.selectedText.indexOf("\n") < 0) {
            findField.text = tab.edit.selectedText
        }
        findField.selectAll()
        findField.forceActiveFocus()
    }
    function hide() {
        visible = false
        closed()
    }
    function next(backward) {
        if (!tab || findField.text === "") return
        missed = !tab.find(findField.text, matchCase, backward)
    }

    // Clicks on the bar don't reach the text below.
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: {} }

    Column {
        id: column
        x: 8
        y: 8
        width: parent.width - 16
        spacing: 8

        Row {
            spacing: 4
            ToolButton {
                width: 32
                icon.name: bar.replacing ? "go-up" : "go-down"
                tooltip: qsTr("Replace")
                onClicked: bar.replacing = !bar.replacing
            }
            TextBox {
                id: findField
                width: column.width - 5 * 36
                placeholderText: qsTr("Find")
                color: bar.missed ? Theme.critical : Theme.text
                onTextEdited: bar.missed = false
                Keys.onReturnPressed: event => bar.next(!!(event.modifiers & Qt.ShiftModifier))
                Keys.onEnterPressed: event => bar.next(!!(event.modifiers & Qt.ShiftModifier))
                Keys.onEscapePressed: bar.hide()
            }
            ToolButton {
                width: 32
                icon.name: "go-down"
                tooltip: qsTr("Find next (F3)")
                onClicked: bar.next(false)
            }
            ToolButton {
                width: 32
                icon.name: "go-up"
                tooltip: qsTr("Find previous (Shift+F3)")
                onClicked: bar.next(true)
            }
            ToolButton {
                width: 32
                text: "Aa"
                checked: bar.matchCase
                tooltip: qsTr("Match case")
                onClicked: bar.matchCase = !bar.matchCase
            }
            ToolButton {
                width: 32
                icon.name: "window-close"
                onClicked: bar.hide()
            }
        }
        Row {
            visible: bar.replacing
            x: 36
            spacing: 4
            TextBox {
                id: replaceField
                width: column.width - 36 - replaceButton.width - replaceAllButton.width - 8
                placeholderText: qsTr("Replace")
                Keys.onReturnPressed: replaceButton.clicked()
                Keys.onEnterPressed: replaceButton.clicked()
                Keys.onEscapePressed: bar.hide()
            }
            ToolButton {
                id: replaceButton
                text: qsTr("Replace")
                onClicked: {
                    if (!bar.tab || findField.text === "") return
                    bar.missed = !bar.tab.replace(findField.text, replaceField.text, bar.matchCase)
                }
            }
            ToolButton {
                id: replaceAllButton
                text: qsTr("Replace all")
                onClicked: {
                    if (!bar.tab || findField.text === "") return
                    bar.missed = bar.tab.doc.replaceAll(findField.text, replaceField.text, bar.matchCase) === 0
                }
            }
        }
    }
}
