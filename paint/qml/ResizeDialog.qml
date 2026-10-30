// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Resize: the picture's new size in pixels; stretched to it, or cut and
// extended (the canvas' size). Loaded when first asked for.
Dialog {
    id: dialog
    property size current
    signal resized(int width, int height, bool scale)

    title: qsTr("Resize")
    dialogWidth: 360
    buttons: [{ text: qsTr("OK"), accent: true, value: "ok" }, { text: qsTr("Cancel"), value: "cancel" }]
    onChosen: value => {
        const w = parseInt(across.text)
        const h = parseInt(down.text)
        if (value === "ok" && w > 0 && h > 0 && w <= 16000 && h <= 16000) resized(w, h, stretch.checked)
    }
    function show(size) {
        current = size
        across.text = size.width
        down.text = size.height
        open()
        across.forceActiveFocus()
        across.selectAll()
    }

    Grid {
        columns: 2
        spacing: 12
        verticalItemAlignment: Grid.AlignVCenter
        Text { text: qsTr("Width (px)"); color: Theme.text; font.pixelSize: Theme.fontBody }
        TextBox {
            id: across
            width: 120
            validator: IntValidator { bottom: 1; top: 16000 }
            // Stretching keeps the picture's proportions.
            onTextEdited: if (stretch.checked && parseInt(text) > 0) down.text = Math.max(1, Math.round(parseInt(text) * dialog.current.height / dialog.current.width))
        }
        Text { text: qsTr("Height (px)"); color: Theme.text; font.pixelSize: Theme.fontBody }
        TextBox {
            id: down
            width: 120
            validator: IntValidator { bottom: 1; top: 16000 }
            onTextEdited: if (stretch.checked && parseInt(text) > 0) across.text = Math.max(1, Math.round(parseInt(text) * dialog.current.width / dialog.current.height))
        }
        Text { text: qsTr("Stretch the picture"); color: Theme.text; font.pixelSize: Theme.fontBody }
        Switch {
            id: stretch
            checked: true
        }
    }
}
