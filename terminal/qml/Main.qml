// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtCore
import Vela.Controls

// The Terminal's window, like Windows 11's: the tabs are in the title bar,
// with "+" for another and the arrow for the profiles; below, the open tab,
// whose terminal can be split into panes. What's inside each is Pane.qml.
Window {
    id: root
    flags: Qt.Window | Qt.FramelessWindowHint // no title bar from Vela
    width: 960
    height: 600
    minimumWidth: 360
    minimumHeight: 200
    visible: true
    color: "transparent" // the terminals paint their background, see-through or not
    title: current && current.title !== "" ? current.title : qsTr("Terminal")

    onActiveChanged: Theme.windowActive = active

    readonly property bool maximized: visibility === Window.Maximized
    function toggleMaximized() { maximized ? showNormal() : showMaximized() }

    // --- what's chosen in the settings ---
    Settings {
        id: settings
        property string fontFamily: Host.defaultFont
        property int fontSize: 14
        property string scheme: "Vela"
        property real backgroundOpacity: 1
        property string profiles: "" // JSON; empty: the machine's shells
        property int defaultProfile: 0
    }
    property alias fontFamily: settings.fontFamily
    property alias fontSize: settings.fontSize
    property alias schemeName: settings.scheme
    property alias backgroundOpacity: settings.backgroundOpacity
    property alias defaultProfile: settings.defaultProfile

    readonly property var profiles: {
        try {
            const list = JSON.parse(settings.profiles)
            if (Array.isArray(list) && list.length > 0) return list
        } catch (e) {}
        return Host.shells()
    }
    function setProfiles(list) {
        settings.profiles = JSON.stringify(list)
        settings.defaultProfile = Math.max(0, Math.min(list.length - 1, settings.defaultProfile))
    }

    // The 16 colors of ANSI, then what's written and what's behind.
    readonly property var campbell: ["#0c0c0c", "#c50f1f", "#13a10e", "#c19c00", "#0037da", "#881798", "#3a96dd", "#cccccc",
        "#767676", "#e74856", "#16c60c", "#f9f1a5", "#3b78ff", "#b4009e", "#61d6d6", "#f2f2f2"]
    readonly property var oneHalfDark: ["#282c34", "#e06c75", "#98c379", "#e5c07b", "#61afef", "#c678dd", "#56b6c2", "#dcdfe4",
        "#5a6374", "#e06c75", "#98c379", "#e5c07b", "#61afef", "#c678dd", "#56b6c2", "#dcdfe4"]
    readonly property var oneHalfLight: ["#383a42", "#e45649", "#50a14f", "#c18301", "#0184bc", "#a626a4", "#0997b3", "#fafafa",
        "#4f525d", "#df6c75", "#98c379", "#e4c07a", "#61afef", "#c577dd", "#56b5c1", "#ffffff"]
    readonly property var solarized: ["#002b36", "#dc322f", "#859900", "#b58900", "#268bd2", "#d33682", "#2aa198", "#eee8d5",
        "#073642", "#cb4b16", "#586e75", "#657b83", "#839496", "#6c71c4", "#93a1a1", "#fdf6e3"]
    readonly property var schemes: ({
        "Vela": Theme.light ? { foreground: "#383a42", background: "#fafafa", colors: oneHalfLight }
                            : { foreground: "#e6e6e6", background: "#181818", colors: oneHalfDark },
        "Campbell": { foreground: "#cccccc", background: "#0c0c0c", colors: campbell },
        "One Half Dark": { foreground: "#dcdfe4", background: "#282c34", colors: oneHalfDark },
        "One Half Light": { foreground: "#383a42", background: "#fafafa", colors: oneHalfLight },
        "Solarized Dark": { foreground: "#839496", background: "#002b36", colors: solarized }
    })
    readonly property var scheme: {
        const s = schemes[settings.scheme] || schemes["Vela"]
        const a = Theme.accentFill
        return { foreground: s.foreground, background: s.background, cursor: s.foreground,
            selection: Qt.rgba(a.r, a.g, a.b, 0.45).toString(), colors: s.colors }
    }

    // --- tabs and their panes ---
    property var tabs: []
    property int currentIndex: 0
    readonly property var current: tabs[currentIndex] || null
    onCurrentChanged: if (current) current.focusView()

    function newPane(parent, page, profile, command, directory) {
        const p = profile || profiles[settings.defaultProfile] || profiles[0]
        const pane = paneComponent.createObject(parent, { win: root, page: page, profile: p })
        // Once it has its size: the program is told the right one from the start.
        Qt.callLater(() => pane.term.start(command || p.command, directory || p.directory)) // the first tab gets the folder it was started in
        return pane
    }
    function newTab(profile, command, directory) {
        const page = tabComponent.createObject(pages)
        const pane = newPane(page, page, profile, command, directory)
        page.content = pane
        page.active = pane
        tabs = tabs.concat([page])
        currentIndex = tabs.length - 1
        page.focusView()
    }
    function removeTab(index) {
        const page = tabs[index]
        if (!page) return
        const rest = tabs.slice()
        rest.splice(index, 1)
        tabs = rest
        page.destroy()
        if (tabs.length === 0) {
            root.close()
            return
        }
        currentIndex = Math.min(currentIndex, tabs.length - 1)
    }
    // Puts something where something else was, in a tab or in a split.
    function replace(parent, old, now) {
        if (!parent.isSplit) parent.content = now
        else if (parent.first === old) parent.first = now
        else parent.second = now
    }
    function firstPane(item) {
        while (item.isSplit) item = item.first
        return item
    }
    function panesOf(item, out) {
        if (item.isSplit) {
            panesOf(item.first, out)
            panesOf(item.second, out)
        } else {
            out.push(item)
        }
        return out
    }
    // The pane typed in makes room for another, which starts in its folder.
    function split(stacked) {
        if (!current || !current.active || settingsLoader.active) return
        const pane = current.active
        const parent = pane.parent
        const holder = splitComponent.createObject(parent, { stacked: stacked })
        replace(parent, pane, holder)
        pane.parent = holder
        holder.first = pane
        const other = newPane(holder, current, pane.profile, "", pane.term.directory())
        holder.second = other
        other.term.forceActiveFocus()
    }
    // The wider side is the one that's divided.
    function splitAuto() {
        if (current && current.active) split(current.active.height > current.active.width)
    }
    function closePane(pane) {
        const page = pane.page
        const parent = pane.parent
        if (!parent.isSplit) {
            removeTab(tabs.indexOf(page))
            return
        }
        const other = parent.first === pane ? parent.second : parent.first
        const above = parent.parent
        other.parent = above
        replace(above, parent, other)
        parent.first = null
        parent.second = null
        parent.destroy()
        pane.destroy()
        page.active = firstPane(other)
        if (page === current) page.focusView()
    }
    // The nearest pane on that side.
    function focusToward(dx, dy) {
        const from = current.active
        const c = from.mapToItem(pages, from.width / 2, from.height / 2)
        let best = null
        let bestDistance = Infinity
        for (const pane of panesOf(current.content, [])) {
            if (pane === from) continue
            const q = pane.mapToItem(pages, pane.width / 2, pane.height / 2)
            if ((q.x - c.x) * dx + (q.y - c.y) * dy <= 0) continue
            const distance = Math.abs(q.x - c.x) + Math.abs(q.y - c.y)
            if (distance < bestDistance) {
                best = pane
                bestDistance = distance
            }
        }
        if (best) best.term.forceActiveFocus()
    }
    readonly property bool hasPanes: current !== null && current.content !== null && current.content.isSplit

    Component.onCompleted: newTab(null, Host.command, Host.directory)

    Component {
        id: tabComponent
        Item {
            id: page
            readonly property bool isSplit: false
            property Item content // a pane, or a split of them
            property Item active // the pane typed in
            readonly property string title: active ? active.title : ""
            anchors.fill: parent
            visible: root.current === page
            onContentChanged: fit()
            onWidthChanged: fit()
            onHeightChanged: fit()
            function fit() {
                if (!content) return
                content.x = 0
                content.y = 0
                content.width = width
                content.height = height
            }
            function focusView() { if (active) active.term.forceActiveFocus() }
        }
    }
    Component { id: paneComponent; Pane {} }
    Component { id: splitComponent; Split {} }

    // --- menus ---
    function profileEntries() {
        const entries = profiles.map((p, i) => ({ text: p.name, shortcut: i < 9 ? "Ctrl+Shift+" + (i + 1) : "", action: () => root.newTab(p) }))
        return entries.concat([
            { separator: true },
            { text: qsTr("Split &right"), shortcut: "Alt+Shift++", action: () => root.split(false) },
            { text: qsTr("Split &down"), shortcut: "Alt+Shift+-", action: () => root.split(true) },
            { text: qsTr("&Close pane"), shortcut: "Ctrl+Shift+W", action: () => root.closePane(root.current.active) },
            { separator: true },
            { text: qsTr("&New window"), shortcut: "Ctrl+Shift+N", action: () => Host.newWindow() },
            { text: qsTr("&Settings"), icon: "configure", shortcut: "Ctrl+,", action: () => root.openSettings() }
        ])
    }
    function openMenu(entries, x, y) { menuLayer.open(entries, x, y, {}) }
    function openBelow(entries, button) {
        const p = button.mapToItem(null, 0, button.height + 2)
        openMenu(entries, p.x, p.y)
    }
    function openSettings() { settingsLoader.active = true }

    Shortcut { sequence: "Ctrl+Shift+T"; onActivated: root.newTab() }
    Shortcut { sequence: "Ctrl+Shift+W"; onActivated: if (root.current) root.closePane(root.current.active) }
    Shortcut { sequence: "Ctrl+Shift+N"; onActivated: Host.newWindow() }
    Shortcut { sequence: "Ctrl+Tab"; onActivated: root.currentIndex = (root.currentIndex + 1) % root.tabs.length }
    Shortcut { sequence: "Ctrl+Shift+Tab"; onActivated: root.currentIndex = (root.currentIndex + root.tabs.length - 1) % root.tabs.length }
    Shortcut { sequences: ["Alt+Shift++", "Alt+Shift+="]; onActivated: root.split(false) }
    Shortcut { sequences: ["Alt+Shift+-", "Alt+Shift+_"]; onActivated: root.split(true) }
    Shortcut { sequence: "Ctrl+Shift+D"; onActivated: root.splitAuto() }
    // Between panes, only while there's more than one: Alt and the arrows are the shell's otherwise.
    Shortcut { sequence: "Alt+Left"; enabled: root.hasPanes; onActivated: root.focusToward(-1, 0) }
    Shortcut { sequence: "Alt+Right"; enabled: root.hasPanes; onActivated: root.focusToward(1, 0) }
    Shortcut { sequence: "Alt+Up"; enabled: root.hasPanes; onActivated: root.focusToward(0, -1) }
    Shortcut { sequence: "Alt+Down"; enabled: root.hasPanes; onActivated: root.focusToward(0, 1) }
    Shortcut { sequences: ["Ctrl++", "Ctrl+="]; onActivated: settings.fontSize = Math.min(72, settings.fontSize + 1) }
    Shortcut { sequence: "Ctrl+-"; onActivated: settings.fontSize = Math.max(6, settings.fontSize - 1) }
    Shortcut { sequence: "Ctrl+0"; onActivated: settings.fontSize = 14 }
    Shortcut { sequence: "Ctrl+,"; onActivated: root.openSettings() }
    Shortcut { sequence: "F11"; onActivated: root.visibility === Window.FullScreen ? root.showNormal() : root.showFullScreen() }
    Repeater {
        model: 9
        Item {
            required property int index
            Shortcut { sequence: "Ctrl+Shift+" + (index + 1); onActivated: if (root.profiles[index]) root.newTab(root.profiles[index]) }
        }
    }

    // --- the title bar, with the tabs ---
    Rectangle {
        id: strip
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: root.visibility === Window.FullScreen ? 0 : 40
        visible: height > 0
        color: Theme.background

        DragHandler {
            target: null
            onActiveChanged: if (active) root.startSystemMove()
        }
        TapHandler {
            onDoubleTapped: root.toggleMaximized()
        }

        Row {
            id: tabRow
            x: 8
            anchors.bottom: parent.bottom
            spacing: 2
            // Room is left for "+", the arrow, the window buttons and a piece of bar to drag.
            readonly property real tabWidth: Math.max(80, Math.min(220, (strip.width - x - 72 - 3 * 46 - 48) / Math.max(1, root.tabs.length)))

            Repeater {
                model: root.tabs
                delegate: Rectangle {
                    id: tabButton
                    required property var modelData
                    required property int index
                    readonly property bool active: root.currentIndex === index
                    readonly property bool hovered: tabMouse.containsMouse || closeHover.hovered
                    width: tabRow.tabWidth
                    height: 34
                    topLeftRadius: Theme.radiusOverlay
                    topRightRadius: Theme.radiusOverlay
                    color: active ? root.scheme.background : hovered ? Theme.subtleHover : "transparent"
                    Image {
                        x: 10
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        source: "image://appicon/utilities-terminal"
                        sourceSize: Qt.size(width, height)
                    }
                    Text {
                        x: 34
                        width: parent.width - 66
                        anchors.verticalCenter: parent.verticalCenter
                        text: tabButton.modelData.title
                        color: tabButton.active ? root.scheme.foreground : Theme.text
                        font.pixelSize: Theme.fontCaption + 1
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        id: tabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                        onClicked: mouse => {
                            if (mouse.button === Qt.MiddleButton) root.removeTab(tabButton.index)
                            else root.currentIndex = tabButton.index
                        }
                    }
                    ToolButton {
                        anchors { right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
                        width: 24
                        height: 24
                        icon: "window-close"
                        visible: tabButton.hovered || tabButton.active
                        onClicked: root.removeTab(tabButton.index)
                        HoverHandler { id: closeHover }
                    }
                }
            }
            ToolButton {
                anchors.verticalCenter: parent.verticalCenter
                width: 32
                height: 28
                icon: "list-add"
                tooltip: qsTr("New tab (Ctrl+Shift+T)")
                onClicked: root.newTab()
            }
            ToolButton {
                id: profilesButton
                anchors.verticalCenter: parent.verticalCenter
                width: 28
                height: 28
                icon: "go-down"
                tooltip: qsTr("Profiles, panes and settings")
                onClicked: root.openBelow(root.profileEntries(), profilesButton)
            }
        }
        Row {
            anchors { right: parent.right; top: parent.top }
            WindowButton { glyph: "M 0 5.5 H 10"; onClicked: root.showMinimized() }
            WindowButton {
                glyph: root.maximized ? "M 0.5 2.5 H 7.5 V 9.5 H 0.5 Z M 2.5 2.5 V 0.5 H 9.5 V 7.5 H 7.5" : "M 0.5 0.5 H 9.5 V 9.5 H 0.5 Z"
                onClicked: root.toggleMaximized()
            }
            WindowButton { glyph: "M 0 0 L 10 10 M 10 0 L 0 10"; closes: true; onClicked: root.close() }
        }
    }

    // --- the open tab ---
    Item {
        id: pages
        anchors { left: parent.left; right: parent.right; top: strip.bottom; bottom: parent.bottom }
    }

    // Settings, over the tab; loaded when first opened.
    Loader {
        id: settingsLoader
        anchors.fill: pages
        z: 50
        active: false
        source: "SettingsPage.qml"
        onLoaded: {
            item.win = root
            item.forceActiveFocus()
            item.closed.connect(() => {
                active = false
                if (root.current) root.current.focusView()
            })
        }
    }

    MenuLayer {
        id: menuLayer
        anchors.fill: parent
        z: 100
        onIsOpenChanged: if (!isOpen && root.current && !settingsLoader.active) root.current.focusView()
    }

    ResizeEdges {}
}
