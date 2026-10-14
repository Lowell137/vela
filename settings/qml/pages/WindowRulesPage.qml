// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Personalization > Window rules: how a new window of an app opens (output,
// desktop, position, size, always on top; window-rule= lines in vela.conf). A
// rule applies when the window opens, to new windows only: the user can move
// it afterwards, and a changed rule doesn't touch windows already open.
// Except "Hide from screen capture" (capture=no): a privacy rule reaches the
// open windows too (docs/renderer.md §15).
Page {
    id: page

    // "app|key=value,key=value" -> ["app", "key=value, ..."]
    function split(rule) {
        const bar = rule.indexOf("|")
        return bar < 0 ? [rule, ""] : [rule.slice(0, bar), rule.slice(bar + 1)]
    }

    CardGroup {
        Card {
            icon: "list-add"
            title: Prefs.windowRules.length === 0 ? qsTr("No rules") : Prefs.windowRules.length + (Prefs.windowRules.length === 1 ? qsTr(" rule") : qsTr(" rules"))
            trailing: Button {
                text: qsTr("Add a rule")
                onClicked: addDialog.show()
            }
        }
        Repeater {
            model: Prefs.windowRules
            delegate: Card {
                id: ruleCard
                required property string modelData
                required property int index
                icon: Apps.iconForAppId(page.split(modelData)[0])
                title: page.split(modelData)[0]
                description: page.split(modelData)[1].replace(/,/g, ", ")
                trailing: Button {
                    subtle: true
                    text: qsTr("Remove")
                    onClicked: Prefs.windowRules = Prefs.windowRules.filter((r, i) => i !== ruleCard.index)
                }
            }
        }
    }

    CardGroup {
        Card {
            icon: "dialog-information"
            title: qsTr("When rules apply")
            description: qsTr("Once, when a window opens: you can still move it afterwards. Changes affect windows opened from now on, not the ones already open. The first rule that matches an app wins.")
        }
        Card {
            icon: "security-high"
            title: qsTr("Hide from screen capture")
            description: qsTr("Applies at once to windows already open too, and follows title changes. The window stays on your screen, but screenshots, recordings and screen sharing show what is behind it. The window's title can still appear on the taskbar and in notifications.")
        }
    }

    Dialog {
        id: addDialog
        parent: root.contentItem
        property var windows: []
        title: qsTr("Add a window rule")
        primaryText: qsTr("Add")
        primaryEnabled: appId.text.trim() !== "" && sizeBox.valid && positionBox.valid
        dialogWidth: 520
        function show() {
            windows = Prefs.openWindows()
            appId.text = ""
            outputBox.text = ""
            desktopBox.text = ""
            sizeBox.text = ""
            positionBox.text = ""
            centerToggle.checked = false
            maximizedToggle.checked = false
            topToggle.checked = false
            captureToggle.checked = false
            open()
        }
        onAccepted: {
            const parts = []
            if (outputBox.text.trim() !== "") parts.push("output=" + outputBox.text.trim())
            if (/^[1-9][0-9]*$/.test(desktopBox.text.trim())) parts.push("desktop=" + desktopBox.text.trim())
            if (sizeBox.text.trim() !== "") parts.push("size=" + sizeBox.text.trim())
            if (positionBox.text.trim() !== "") parts.push("position=" + positionBox.text.trim().replace(/\s+/g, ""))
            if (centerToggle.checked) parts.push("center=yes")
            if (maximizedToggle.checked) parts.push("maximized=yes")
            if (topToggle.checked) parts.push("always-on-top=yes")
            if (captureToggle.checked) parts.push("capture=no")
            if (parts.length === 0) return
            Prefs.windowRules = Prefs.windowRules.concat([appId.text.trim().replace(/[|\n]/g, "") + "|" + parts.join(",")])
        }
        Column {
            width: parent.width
            spacing: 8
            Flow {
                width: parent.width
                spacing: 6
                Repeater {
                    model: addDialog.windows
                    delegate: Button {
                        required property var modelData
                        subtle: true
                        text: modelData.appId
                        onClicked: appId.text = modelData.appId
                    }
                }
            }
            TextBox {
                id: appId
                width: parent.width
                placeholderText: qsTr("App id (e.g. discord) or pick an open window above")
            }
            TextBox {
                id: outputBox
                width: parent.width
                placeholderText: qsTr("Output: name (HDMI-A-1) or number (1, 2, ...)")
            }
            TextBox {
                id: desktopBox
                width: parent.width
                placeholderText: qsTr("Desktop number (1, 2, ...)")
            }
            TextBox {
                id: sizeBox
                property bool valid: text.trim() === "" || /^[1-9][0-9]*x[1-9][0-9]*$/.test(text.trim())
                width: parent.width
                placeholderText: qsTr("Size: 900x600")
            }
            TextBox {
                id: positionBox
                property bool valid: text.trim() === "" || /^-?[0-9]+\s*,\s*-?[0-9]+$/.test(text.trim())
                width: parent.width
                placeholderText: qsTr("Position from the output's corner: 100,80")
            }
            Row {
                spacing: 12
                Text { text: qsTr("Centered"); color: Theme.text; font.pixelSize: Theme.fontBody; anchors.verticalCenter: parent.verticalCenter }
                Toggle { id: centerToggle; onToggled: on => centerToggle.checked = on }
                Text { text: qsTr("Maximized"); color: Theme.text; font.pixelSize: Theme.fontBody; anchors.verticalCenter: parent.verticalCenter }
                Toggle { id: maximizedToggle; onToggled: on => maximizedToggle.checked = on }
                Text { text: qsTr("Always on top"); color: Theme.text; font.pixelSize: Theme.fontBody; anchors.verticalCenter: parent.verticalCenter }
                Toggle { id: topToggle; onToggled: on => topToggle.checked = on }
            }
            Row {
                spacing: 12
                Text { text: qsTr("Hide from screen capture"); color: Theme.text; font.pixelSize: Theme.fontBody; anchors.verticalCenter: parent.verticalCenter }
                Toggle { id: captureToggle; onToggled: on => captureToggle.checked = on }
            }
        }
    }
}
