// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtTest
import Vela.Controls
import "../../shell/qml"

// A menu fades in where it will stay, as on Windows 11 (renderer.md §14).
Item {
    id: root
    width: 700
    height: 500

    Component {
        id: panelComponent
        MenuPanel { fill: Theme.flyout; entries: [{ text: "One" }]; anchorX: 40; anchorY: 40 }
    }
    Component {
        id: iconRowComponent
        MenuPanel {
            fill: Theme.flyout
            anchorX: 40
            anchorY: 40
            entries: [{ iconRow: [{ icon: "edit-cut", text: "Cut", shortcut: "Ctrl+X" }, { icon: "edit-copy", text: "Copy" },
                                  { icon: "edit-rename", text: "Rename" }, { icon: "document-share", text: "Share" },
                                  { icon: "edit-delete", text: "Delete" }] }, { text: "One" }]
        }
    }

    TestCase {
        name: "MenuPanel"
        when: windowShown

        function test_fadesInWithoutMoving() {
            const panel = createTemporaryObject(panelComponent, root)
            const y = panel.y
            verify(panel.opacity < 1)
            compare(panel.transform.length, 0)
            tryCompare(panel, "opacity", 1, 2000)
            compare(panel.y, y)
        }

        // Sizes measured on Windows 11 at 100%.
        function test_iconRowFitsItsLabeledButtons() {
            const panel = createTemporaryObject(iconRowComponent, root)
            compare(panel.rowHeightOf(panel.entries[0]), 59)
            compare(panel.iconButtonWidth, 56)
            compare(panel.iconButtonHeight, 48)
            verify(panel.width >= 2 * panel.iconRowMargin + 5 * 56 + 4 * 9)
        }

        function test_submenuIsThinner() {
            const panel = createTemporaryObject(panelComponent, root, { level: 1 })
            compare(panel.padding, 3)
            compare(panel.height, 32 + 6)
        }
    }
}
