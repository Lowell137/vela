// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// "Emoji and more", like Windows 11 (Win+V and Win+.): tabs for what was typed
// from here last, emoji (found by name), kaomoji, symbols and the clipboard
// history. In the history, most recent first (pinned first), a click or Enter
// pastes into the focused app; "…" to pin or delete; Del deletes. If history
// is off, the panel offers to turn it on. It opens next to the pointer, or at
// the bottom right (Settings > System > Clipboard).
Window {
    id: root
    visible: false
    width: 384
    height: panel.height + 24
    color: "transparent"

    property int current: 0
    property string tab: "clipboard" // recent, emoji, kaomoji, symbols, clipboard
    property int group: 0 // of the emoji
    property point corner: Qt.point(-1, -1) // the window's, on its output; negative: at the bottom right
    readonly property var tabs: [
        { id: "recent", label: "\u{1F552}", tip: qsTr("Most recently used") },
        { id: "emoji", label: "\u{1F642}", tip: qsTr("Emoji") },
        { id: "kaomoji", label: ";-)", tip: qsTr("Kaomoji") },
        { id: "symbols", label: "\u03A9", tip: qsTr("Symbols") },
        { id: "clipboard", label: "\u{1F4CB}", tip: qsTr("Clipboard history") }
    ]
    readonly property var kaomoji: [
        "(\u25D5\u203F\u25D5)", "(\u3065\uFF61\u25D5\u203F\u203F\u25D5\uFF61)\u3065", "\u00AF\\_(\u30C4)_/\u00AF", "(\u256F\u00B0\u25A1\u00B0)\u256F\uFE35 \u253B\u2501\u253B",
        "\u252C\u2500\u252C\u30CE( \u00BA _ \u00BA\u30CE)", "(\u0CA0_\u0CA0)", "(\u2022_\u2022)", "( \u2022_\u2022)>\u2310\u25A0-\u25A0", "(\u2310\u25A0_\u25A0)", "\u0295\u2022\u1D25\u2022\u0294",
        "(\u3063\u02D8\u06A1\u02D8\u03C2)", "(\u0E07 \u2022\u0300_\u2022\u0301)\u0E07", "(\u2565\uFE4F\u2565)", "(\u00AC_\u00AC)", "(\u2299_\u2299)", "(\uFF89\u25D5\u30EE\u25D5)\uFF89*:\uFF65\uFF9F\u2727",
        "\u30FD(\u00B4\u25BD`)/", "(\uFF61\u2665\u203F\u2665\uFF61)", "(\u00B0\u30ED\u00B0)\u261D", "(-_-) zzz", "\u10DA(\u0CA0\u76CA\u0CA0\u10DA)", "(\u0E51\u02C3\u1D17\u02C2)\uFEED",
        ":-)", ":-(", ";-)", ":-D", ":-P", ":-O", ":'(", "<3", "^_^", "T_T", "o_O", ">_<", "-_-", "xD"
    ]
    readonly property string symbols: "\u20AC\u00A3\u00A5\u20BA\u20BD\u20B9\u00A2\u20BF\u00A9\u00AE\u2122\u00A7\u00B6\u2020\u2021\u2022\u2026\u00B0\u2116"
        + "\u00B1\u00D7\u00F7\u2260\u2248\u2264\u2265\u221E\u221A\u2211\u220F\u222B\u2202\u0394\u03A9\u03C0\u03BC\u03B1\u03B2\u03B3\u03B8\u03BB\u03C3\u03C6"
        + "\u00BD\u00BC\u00BE\u2153\u2154\u00B9\u00B2\u00B3\u2030\u2031"
        + "\u2190\u2191\u2192\u2193\u2194\u2195\u21D0\u21D2\u21D4\u21B5\u21BA\u21BB\u27A4\u25B2\u25BC\u25C0\u25B6"
        + "\u2713\u2714\u2717\u2718\u2605\u2606\u2665\u2666\u2663\u2660\u266A\u266B\u263A\u2639\u2600\u2601\u2602\u2603"
        + "\u00AB\u00BB\u201C\u201D\u2018\u2019\u2014\u2013\u00BF\u00A1\u00B7\u25A0\u25A1\u25CF\u25CB\u25C6\u25C7"
        + "\u2318\u2325\u21E7\u2303\u238B\u23CE\u232B\u2326\u21E5"

    function open(which, x, y) {
        const atPointer = x >= 0 && Clip.opensAtPointer()
        // On another output a visible window is hidden first and shown on the
        // next tick (Menus.placeOnTargetScreen); asked again as it was.
        if (!atPointer && !Menus.placeOnTargetScreen(root)) {
            Qt.callLater(() => root.open(which, x, y))
            return
        }
        Menus.panelScreen = "" // the pointer chose the output; don't leave an old one for the next time
        tab = which === "emoji" ? "emoji" : "clipboard"
        current = 0
        corner = atPointer ? Shell.placeAtPoint(root, x - 12, y - 12) : Shell.placeAtPoint(root, -1, -1)
        visible = true
        panel.opacity = 0
        slide.y = Config.taskbarTop ? -12 : 12
        appear.restart()
        focusTab()
        updateBlur()
    }
    function focusTab() {
        if (tab === "emoji") search.forceActiveFocus()
        else panel.forceActiveFocus()
    }
    onTabChanged: if (visible) focusTab()
    // An emoji, a kaomoji or a symbol goes into the focused app.
    function typeText(text) {
        close()
        Emoji.used(text)
        Clip.type(text)
    }
    function close() {
        visible = false
    }
    function pasteAt(index) {
        const item = Clip.items[index]
        if (!item) return
        close()
        Clip.paste(item.id)
    }

    Connections {
        target: Shell
        function onClipboardRequested(tab, x, y) {
            const wanted = tab === "emoji" ? "emoji" : "clipboard"
            if (!root.visible) root.open(tab, x, y)
            else if (root.tab === wanted) root.close()
            else root.tab = wanted
        }
    }
    onActiveChanged: {
        if (!active && visible && !Menus.isOpen) close()
    }

    function updateBlur() {
        Effects.setBlur(root, [Qt.rect(panel.x, panel.y, panel.width, panel.height)])
    }

    // From panel to output coordinates (menus): next to the pointer, or at
    // the bottom right, above the taskbar.
    function screenPoint(item, x, y) {
        const p = item.mapToItem(null, x, y)
        if (corner.x >= 0) return Qt.point(corner.x + p.x, corner.y + p.y)
        return Qt.point(Screen.width - root.width + p.x, Config.taskbarTop ? Theme.taskbarHeight + p.y : Screen.height - Theme.taskbarHeight - root.height + p.y)
    }

    ParallelAnimation {
        id: appear
        NumberAnimation { target: panel; property: "opacity"; to: 1; duration: Theme.fast; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        NumberAnimation { target: slide; property: "y"; to: 0; duration: Theme.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
    }

    Rectangle {
        id: panel
        x: 12
        y: 12
        width: root.width - 24
        height: content.height + 32
        radius: Theme.radiusOverlay
        color: Theme.surface
        border.width: 1
        border.color: Theme.stroke
        transform: Translate { id: slide }
        focus: true
        onHeightChanged: root.updateBlur()

        Keys.onPressed: event => {
            const count = root.tab === "clipboard" ? Clip.items.length : 0
            if (event.key === Qt.Key_Escape) {
                root.close()
            } else if (event.key === Qt.Key_Down && count > 0) {
                root.current = Math.min(count - 1, root.current + 1)
                list.positionAt(root.current)
            } else if (event.key === Qt.Key_Up && count > 0) {
                root.current = Math.max(0, root.current - 1)
                list.positionAt(root.current)
            } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && count > 0) {
                root.pasteAt(root.current)
            } else if (event.key === Qt.Key_Delete && count > 0) {
                Clip.remove(Clip.items[root.current].id)
                root.current = Math.max(0, Math.min(root.current, Clip.items.length - 1))
            } else {
                return
            }
            event.accepted = true
        }

        Column {
            id: content
            x: 16
            y: 16
            width: parent.width - 32
            spacing: 12

            // --- the tabs ---
            Row {
                spacing: 4
                Repeater {
                    model: root.tabs
                    delegate: Rectangle {
                        id: tabButton
                        required property var modelData
                        readonly property bool current: root.tab === modelData.id
                        width: 44
                        height: 36
                        radius: Theme.radius
                        color: tabMouse.containsMouse || current ? Theme.subtleHover : "transparent"
                        Text {
                            anchors.centerIn: parent
                            text: tabButton.modelData.label
                            color: Theme.text
                            font.pixelSize: 16
                        }
                        Rectangle {
                            visible: tabButton.current
                            anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom }
                            width: 16
                            height: 3
                            radius: 1.5
                            color: Theme.accent
                        }
                        MouseArea {
                            id: tabMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: root.tab = tabButton.modelData.id
                        }
                    }
                }
            }

            // --- what was typed from here last ---
            Text {
                visible: root.tab === "recent" && Emoji.recent.length === 0
                width: parent.width
                text: qsTr("The emoji, kaomoji and symbols you use will show up here.")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                wrapMode: Text.WordWrap
            }
            // Buttons of any width, one after the other: recent, kaomoji and symbols.
            Flickable {
                visible: root.tab === "kaomoji" || root.tab === "symbols" || (root.tab === "recent" && Emoji.recent.length > 0)
                width: parent.width
                height: 300
                contentHeight: pieces.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                Flow {
                    id: pieces
                    width: parent.width
                    spacing: 4
                    Repeater {
                        model: root.tab === "kaomoji" ? root.kaomoji : root.tab === "symbols" ? Array.from(root.symbols) : root.tab === "recent" ? Emoji.recent : []
                        delegate: Rectangle {
                            id: piece
                            required property string modelData
                            width: Math.max(40, pieceText.implicitWidth + 20)
                            height: 40
                            radius: Theme.radius
                            color: pieceMouse.containsMouse ? Theme.subtleHover : Theme.control
                            Text {
                                id: pieceText
                                anchors.centerIn: parent
                                text: piece.modelData
                                textFormat: Text.PlainText
                                color: Theme.text
                                font.pixelSize: piece.modelData.length > 2 ? Theme.fontBody : 20
                            }
                            MouseArea {
                                id: pieceMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: root.typeText(piece.modelData)
                            }
                        }
                    }
                }
            }

            // --- emoji: found by name, or by group ---
            Rectangle {
                visible: root.tab === "emoji"
                width: parent.width
                height: 32
                radius: Theme.radius
                color: Theme.control
                border.width: 1
                border.color: search.activeFocus ? Theme.accent : Theme.stroke
                TextInput {
                    id: search
                    anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                    verticalAlignment: TextInput.AlignVCenter
                    clip: true
                    color: Theme.text
                    selectionColor: Theme.accent
                    selectedTextColor: "white"
                    font.pixelSize: Theme.fontBody
                    Keys.onEscapePressed: root.close()
                    Keys.onReturnPressed: if (emojiGrid.count > 0) root.typeText(emojiGrid.model[0].emoji)
                    Text {
                        visible: search.text === ""
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Search emoji")
                        color: Theme.textTertiary
                        font.pixelSize: Theme.fontBody
                    }
                }
            }
            Row {
                visible: root.tab === "emoji" && search.text === ""
                spacing: 2
                Repeater {
                    model: ["\u{1F642}", "\u{1F9D1}", "\u{1F43B}", "\u{1F354}", "\u2708\uFE0F", "\u26BD", "\u{1F4A1}", "\u{1F523}", "\u{1F3F3}\uFE0F"]
                    delegate: Rectangle {
                        required property string modelData
                        required property int index
                        width: 36
                        height: 32
                        radius: Theme.radius
                        color: groupMouse.containsMouse || root.group === index ? Theme.subtleHover : "transparent"
                        Text {
                            anchors.centerIn: parent
                            text: parent.modelData
                            font.pixelSize: 16
                        }
                        MouseArea {
                            id: groupMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: root.group = parent.index
                        }
                    }
                }
            }
            GridView {
                id: emojiGrid
                visible: root.tab === "emoji"
                width: parent.width
                height: 264
                cellWidth: 44
                cellHeight: 44
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                // Asked only while the tab is shown: the list is large.
                model: visible ? Emoji.list(root.group, search.text) : []
                delegate: Rectangle {
                    id: cell
                    required property var modelData
                    width: 42
                    height: 42
                    radius: Theme.radius
                    color: cellMouse.containsMouse ? Theme.subtleHover : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: cell.modelData.emoji
                        font.pixelSize: 24
                    }
                    MouseArea {
                        id: cellMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: root.typeText(cell.modelData.emoji)
                    }
                }
            }
            Text {
                visible: root.tab === "emoji" && search.text !== "" && emojiGrid.count === 0
                width: parent.width
                text: qsTr("No emoji found.")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
            }

            // --- the clipboard ---
            Item {
                visible: root.tab === "clipboard"
                width: parent.width
                height: 32
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Clipboard")
                    color: Theme.text
                    font.pixelSize: Theme.fontBody + 2
                    font.weight: Font.DemiBold
                }
                Rectangle {
                    visible: Clip.enabled && Clip.items.some(i => !i.pinned)
                    anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                    width: clearText.implicitWidth + 20
                    height: 30
                    radius: Theme.radius
                    color: clearMouse.containsMouse ? Theme.subtleHover : Theme.control
                    border.width: 1
                    border.color: Theme.stroke
                    Text {
                        id: clearText
                        anchors.centerIn: parent
                        text: qsTr("Clear all")
                        color: Theme.text
                        font.pixelSize: Theme.fontCaption
                    }
                    MouseArea {
                        id: clearMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: { Clip.clear(); root.current = 0 }
                    }
                }
            }

            // Off: like Windows, it's turned on from here.
            Column {
                visible: root.tab === "clipboard" && !Clip.enabled
                width: parent.width
                spacing: 12
                Text {
                    width: parent.width
                    text: qsTr("Clipboard history")
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.weight: Font.DemiBold
                }
                Text {
                    width: parent.width
                    text: qsTr("You can't see your clipboard history. Turn it on to find what you copy here and paste it again.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontBody
                    wrapMode: Text.WordWrap
                }
                Button {
                    text: qsTr("Turn on")
                    accent: true
                    onClicked: Clip.enabled = true
                }
            }
            Text {
                visible: root.tab === "clipboard" && Clip.enabled && Clip.items.length === 0
                width: parent.width
                text: qsTr("There's nothing here. When you copy something, you'll find it here to paste it again.")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                wrapMode: Text.WordWrap
            }

            Flickable {
                id: list
                visible: root.tab === "clipboard" && Clip.enabled && Clip.items.length > 0
                width: parent.width
                height: Math.min(contentHeight, 420)
                contentHeight: cards.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                function positionAt(index) {
                    const card = cards.children[index]
                    if (!card) return
                    if (card.y < contentY) contentY = card.y
                    else if (card.y + card.height > contentY + height) contentY = card.y + card.height - height
                }

                Column {
                    id: cards
                    width: parent.width
                    spacing: 8
                    Repeater {
                        model: Clip.items
                        delegate: Rectangle {
                            id: card
                            required property var modelData
                            required property int index
                            readonly property bool selected: index === root.current
                            width: cards.width
                            height: Math.max(56, (modelData.kind === "image" ? thumb.height : body.height) + 24)
                            radius: Theme.radius
                            color: cardMouse.containsMouse ? Theme.subtleHover : Theme.control
                            border.width: selected ? 2 : 1
                            border.color: selected ? Theme.accent : Theme.stroke

                            Text {
                                id: body
                                visible: card.modelData.kind === "text"
                                x: 12
                                y: 12
                                width: parent.width - 56
                                text: card.modelData.text
                                textFormat: Text.PlainText
                                color: Theme.text
                                font.pixelSize: Theme.fontBody
                                wrapMode: Text.WrapAnywhere
                                maximumLineCount: 4
                                elide: Text.ElideRight
                            }
                            Image {
                                id: thumb
                                visible: card.modelData.kind === "image"
                                x: 12
                                y: 12
                                width: parent.width - 56
                                height: 96
                                fillMode: Image.PreserveAspectFit
                                horizontalAlignment: Image.AlignLeft
                                source: visible ? "image://clipboard/" + card.modelData.id : ""
                                sourceSize: Qt.size(width, height) // logical: Qt turns it into pixels
                                asynchronous: true
                            }
                            MouseArea {
                                id: cardMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: root.pasteAt(card.index)
                            }
                            // Pinned: the pin; "…" to pin or delete.
                            Text {
                                visible: card.modelData.pinned
                                anchors { right: more.left; rightMargin: 2; verticalCenter: more.verticalCenter }
                                text: "\u{1F4CC}"
                                font.pixelSize: 11
                                opacity: 0.7
                            }
                            Rectangle {
                                id: more
                                anchors { right: parent.right; top: parent.top; margins: 8 }
                                width: 28
                                height: 28
                                radius: Theme.radius
                                color: moreMouse.containsMouse ? Theme.subtleHover : "transparent"
                                Text {
                                    anchors.centerIn: parent
                                    text: "…"
                                    color: Theme.text
                                    font.pixelSize: Theme.fontBody
                                }
                                MouseArea {
                                    id: moreMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        const id = card.modelData.id
                                        const p = root.screenPoint(more, 0, more.height)
                                        Menus.open([
                                            { text: card.modelData.pinned ? qsTr("&Unpin") : qsTr("&Pin"), icon: card.modelData.pinned ? "window-unpin" : "window-pin", action: () => Clip.togglePin(id) },
                                            { text: qsTr("&Delete"), icon: "edit-delete", shortcut: qsTr("Del"), action: () => Clip.remove(id) }
                                        ], p.x, p.y, { screen: root.screen ? root.screen.name : "" })
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
