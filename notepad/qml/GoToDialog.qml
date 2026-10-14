// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Edit > Go to (Ctrl+G): asks for a line number.
Dialog {
    id: dialog
    property int lines: 1
    property int line: 1
    signal accepted(int line)

    title: qsTr("Go to line")
    buttons: [{ text: qsTr("Go to"), accent: true, value: "go" }, { text: qsTr("Cancel"), value: "cancel" }]
    onChosen: value => {
        const n = parseInt(field.text)
        if (value === "go" && n >= 1) accepted(Math.min(n, lines))
    }

    function ask() {
        open()
        field.text = line
        field.selectAll()
        field.forceActiveFocus()
    }

    TextBox {
        id: field
        width: parent.width
        validator: IntValidator { bottom: 1 }
        inputMethodHints: Qt.ImhDigitsOnly
        Keys.onReturnPressed: dialog.close("go")
        Keys.onEnterPressed: dialog.close("go")
        Keys.onEscapePressed: dialog.close("cancel")
    }
}
