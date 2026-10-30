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
            icon: "window"
            title: qsTr("Remember window positions")
            description: qsTr("Apps open where you left them. Your rules below come first.")
            trailing: Switch {
                checked: Prefs.rememberWindowPositions
                onToggled: on => Prefs.rememberWindowPositions = on
            }
        }
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
            description: qsTr("A rule works when a window opens, not on windows that are already open. You can still move the window after.")
        }
        Card {
            icon: "security-high"
            title: qsTr("Hide from screen capture")
            description: qsTr("These windows don't show in screen sharing, recordings or screenshots. You still see them as usual. Their name and taskbar icon can still show.")
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
        // Settings steps aside, the pointer becomes a crosshair and the next
        // click on any window fills the field (Esc cancels).
        property bool picking: false
        function pick() {
            picking = true
            visible = false
            const w = page.Window.window
            if (w) w.showMinimized()
            Prefs.pickWindow()
        }
        Connections {
            target: Prefs
            function onWindowPicked(app, title) {
                if (!addDialog.picking) return
                addDialog.picking = false
                const w = page.Window.window
                if (w) { w.showNormal(); w.raise(); w.requestActivate() }
                if (app !== "") appId.text = app
                addDialog.visible = true
            }
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
        QtObject { id: centerToggle; property bool checked: false }
        QtObject { id: maximizedToggle; property bool checked: false }
        QtObject { id: topToggle; property bool checked: false }
        QtObject { id: captureToggle; property bool checked: false }
        // Scrolls when the window is too small for all the fields; the
        // buttons below always stay in view.
        Flickable {
            id: fields
            width: parent.width
            height: Math.min(fields.contentHeight, Math.max(120, addDialog.height - 250))
            contentWidth: width
            contentHeight: form.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
        Column {
            id: form
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
            Row {
                width: parent.width
                spacing: 8
                TextBox {
                    id: appId
                    width: parent.width - pickButton.width - parent.spacing
                    placeholderText: qsTr("App (e.g. discord)")
                }
                Button {
                    id: pickButton
                    text: qsTr("Pick a window")
                    onClicked: addDialog.pick()
                }
            }
            TextBox {
                id: outputBox
                width: parent.width
                placeholderText: qsTr("Screen: name (HDMI-A-1) or number (1, 2, ...)")
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
                placeholderText: qsTr("Position from the screen's corner: 100,80")
            }
            Grid {
                width: parent.width
                columns: 2
                columnSpacing: 16
                rowSpacing: 4
                Repeater {
                    model: [
                        { label: qsTr("Centered"), toggle: centerToggle },
                        { label: qsTr("Maximized"), toggle: maximizedToggle },
                        { label: qsTr("Always on top"), toggle: topToggle },
                        { label: qsTr("Hide from screen capture"), toggle: captureToggle }
                    ]
                    delegate: Item {
                        id: cell
                        required property var modelData
                        width: (form.width - 16) / 2
                        height: Math.max(36, cellText.implicitHeight + 8)
                        Text {
                            id: cellText
                            anchors { left: parent.left; right: cellToggle.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
                            text: cell.modelData.label
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            wrapMode: Text.Wrap
                        }
                        Switch {
                            id: cellToggle
                            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                            showLabel: false
                            checked: cell.modelData.toggle.checked
                            onToggled: on => cell.modelData.toggle.checked = on
                        }
                    }
                }
            }
        }
        }
    }
}
