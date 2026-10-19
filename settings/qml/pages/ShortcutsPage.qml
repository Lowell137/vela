// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Time & language > Keyboard > Keyboard shortcuts: the shortcuts Vela has, to change, and
// your own, each running a command. They are kept in vela.conf (shortcut-*).
Page {
    id: page

    // What the dialog is editing: a built-in action (id), a new custom
    // shortcut, or an existing one (number).
    property string editKind: "builtin" // builtin, custom-new, custom-edit, app
    property string editId: ""
    property int editNumber: 0
    property string editError: ""

    function comboFromEvent(event) {
        const modifierKeys = [Qt.Key_Shift, Qt.Key_Control, Qt.Key_Alt, Qt.Key_AltGr, Qt.Key_Meta, Qt.Key_Super_L, Qt.Key_Super_R]
        if (modifierKeys.indexOf(event.key) >= 0) {
            return ""
        }
        let name = ""
        if (event.key >= Qt.Key_A && event.key <= Qt.Key_Z) {
            name = String.fromCharCode(event.key)
        } else if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
            name = String.fromCharCode(event.key)
        } else if (event.key >= Qt.Key_F1 && event.key <= Qt.Key_F24) {
            name = "F" + (event.key - Qt.Key_F1 + 1)
        } else {
            const named = {}
            named[Qt.Key_Return] = "Return"
            named[Qt.Key_Enter] = "Return"
            named[Qt.Key_Tab] = "Tab"
            named[Qt.Key_Space] = "space"
            named[Qt.Key_Left] = "Left"
            named[Qt.Key_Right] = "Right"
            named[Qt.Key_Up] = "Up"
            named[Qt.Key_Down] = "Down"
            named[Qt.Key_Escape] = "Escape"
            named[Qt.Key_Print] = "Print"
            named[Qt.Key_Home] = "Home"
            named[Qt.Key_End] = "End"
            named[Qt.Key_PageUp] = "Prior"
            named[Qt.Key_PageDown] = "Next"
            named[Qt.Key_Delete] = "Delete"
            named[Qt.Key_Insert] = "Insert"
            named[Qt.Key_Plus] = "plus"
            named[Qt.Key_Minus] = "minus"
            name = named[event.key] || ""
        }
        if (name === "") {
            return ""
        }
        const mods = []
        if (event.modifiers & Qt.MetaModifier) mods.push("Super")
        if (event.modifiers & Qt.ControlModifier) mods.push("Ctrl")
        if (event.modifiers & Qt.AltModifier) mods.push("Alt")
        if (event.modifiers & Qt.ShiftModifier) mods.push("Shift")
        if (mods.length === 0 && !/^F\d+$/.test(name) && name !== "Print") {
            return ""
        }
        return mods.concat([name]).join("+")
    }

    function edit(kind, id, number, combo, command) {
        editError = ""
        editKind = kind
        editId = id
        editNumber = number
        comboField.text = combo
        commandField.text = command
        shortcutDialog.title = kind === "custom-new" ? qsTr("New shortcut") : kind === "custom-edit" ? qsTr("Change shortcut") : kind === "app" ? qsTr("Change ") + command : qsTr("Change ") + Prefs.shortcuts.filter(s => s.id === id)[0].title
        shortcutDialog.open()
        comboField.forceActiveFocus()
    }

    function apply() {
        let error = ""
        if (editKind === "builtin") {
            error = Prefs.setShortcut(editId, comboField.text)
        } else if (editKind === "app") {
            error = Prefs.setAppShortcut(editNumber, comboField.text)
        } else if (editKind === "custom-new") {
            error = Prefs.addCustomShortcut(comboField.text, commandField.text)
        } else {
            error = Prefs.updateCustomShortcut(editNumber, comboField.text, commandField.text)
        }
        if (error !== "") {
            editError = error
            shortcutDialog.open() // stays open with the reason
        } else {
            editError = ""
        }
    }

    CardGroup {
        title: qsTr("Shortcuts")
        Repeater {
            model: Prefs.shortcuts
            delegate: Card {
                id: row
                required property var modelData
                icon: "input-keyboard"
                title: modelData.title
                description: (modelData.combo !== "" ? modelData.combo : qsTr("Off"))
                    + (modelData.changed ? "   " + qsTr("Default: ") + modelData.defaultCombo : "")
                trailing: Row {
                    spacing: 4
                    Button {
                        subtle: true
                        text: qsTr("Change")
                        onClicked: page.edit("builtin", row.modelData.id, 0, row.modelData.combo, "")
                    }
                    Button {
                        visible: row.modelData.changed
                        subtle: true
                        text: qsTr("Reset")
                        onClicked: Prefs.resetShortcut(row.modelData.id)
                    }
                }
            }
        }
    }

    CardGroup {
        title: qsTr("Your shortcuts")
        Card {
            icon: "list-add"
            title: qsTr("Add a shortcut")
            description: qsTr("Run a program or a command with a key combination")
            trailing: Button {
                text: qsTr("Add")
                onClicked: page.edit("custom-new", "", 0, "", "")
            }
        }
        Repeater {
            model: Prefs.customShortcuts
            delegate: Card {
                id: customRow
                required property var modelData
                icon: "utilities-terminal"
                title: modelData.command
                description: modelData.combo !== "" ? modelData.combo : qsTr("Not valid")
                trailing: Row {
                    spacing: 4
                    Button {
                        subtle: true
                        text: qsTr("Change")
                        onClicked: page.edit("custom-edit", "", customRow.modelData.number, customRow.modelData.combo, customRow.modelData.command)
                    }
                    Button {
                        subtle: true
                        text: qsTr("Remove")
                        onClicked: Prefs.removeCustomShortcut(customRow.modelData.number)
                    }
                }
            }
        }
    }

    CardGroup {
        title: qsTr("Asked for by apps")
        visible: Prefs.appShortcuts.length > 0
        Repeater {
            model: Prefs.appShortcuts
            delegate: Card {
                id: appRow
                required property var modelData
                icon: "input-keyboard"
                title: modelData.description
                description: modelData.app + "   " + (modelData.combo !== "" ? modelData.combo : qsTr("No key yet"))
                trailing: Row {
                    spacing: 4
                    Button {
                        subtle: true
                        text: qsTr("Change")
                        onClicked: page.edit("app", "", appRow.modelData.number, appRow.modelData.combo, appRow.modelData.description)
                    }
                    Button {
                        subtle: true
                        text: qsTr("Remove")
                        onClicked: Prefs.removeAppShortcut(appRow.modelData.number)
                    }
                }
            }
        }
    }

    CardGroup {
        title: qsTr("Also available")
        Repeater {
            model: Prefs.fixedShortcuts()
            delegate: Card {
                required property var modelData
                icon: "input-keyboard"
                title: modelData.title
                description: modelData.combo
            }
        }
    }

    Dialog {
        id: shortcutDialog
        parent: root.contentItem
        primaryText: qsTr("Save")
        primaryEnabled: Prefs.normalizeShortcut(comboField.text) !== ""
            && (page.editKind === "builtin" || page.editKind === "app" || commandField.text.trim() !== "")
            || ((page.editKind === "builtin" || page.editKind === "app") && comboField.text.trim() === "")
        dialogWidth: 520
        onAccepted: page.apply()
        Column {
            width: parent.width
            spacing: 10
            Text {
                width: parent.width
                text: page.editKind === "builtin" || page.editKind === "app"
                    ? qsTr("Click the box and press the keys. Leave it empty to turn this shortcut off.")
                    : qsTr("Click the first box and press the keys, then type the command to run.")
                color: Theme.text
                font.pixelSize: Theme.fontBody
                wrapMode: Text.Wrap
            }
            TextBox {
                id: comboField
                width: parent.width
                placeholderText: qsTr("For example Super+Q")
                Keys.onPressed: event => {
                    const combo = page.comboFromEvent(event)
                    if (combo !== "") {
                        text = combo
                        event.accepted = true
                    }
                }
            }
            TextBox {
                id: commandField
                width: parent.width
                visible: page.editKind === "custom-new" || page.editKind === "custom-edit"
                placeholderText: qsTr("Command, for example foot or xdg-open https://velade.org")
            }
            Text {
                width: parent.width
                visible: page.editError !== ""
                text: page.editError
                color: Theme.text
                font.pixelSize: Theme.fontBody
                wrapMode: Text.Wrap
            }
        }
    }
}
