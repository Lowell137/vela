// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtCore
import Vela.Controls

// Notepad's window, like Windows 11's: the tabs are in the title bar, which
// Notepad draws itself (the compositor still rounds the corners and casts the
// shadow); below, the File, Edit and View menus, the open tab's text and the
// status bar. Menus, dialogs and the "save changes?" questions live here.
Window {
    id: root
    flags: Qt.Window | Qt.FramelessWindowHint // no title bar from Vela
    width: 900
    height: 640
    minimumWidth: 420
    minimumHeight: 280
    visible: true
    color: Theme.background
    title: current ? current.title + qsTr(" - Notepad") : qsTr("Notepad")

    onActiveChanged: Theme.windowActive = active

    readonly property bool maximized: visibility === Window.Maximized
    function toggleMaximized() { maximized ? showNormal() : showMaximized() }

    // --- preferences, remembered across launches ---
    Settings {
        id: settings
        property bool wordWrap: true
        property bool statusBar: true
        property string fontFamily: "" // none chosen: the system's fixed-width font
        property int fontSize: 15 // pixels: Windows' 11 points
        property bool restoreSession: true // reopen the files that were open
        property string session: "[]" // their URLs, as JSON
    }
    property alias restoreSession: settings.restoreSession
    property alias wordWrap: settings.wordWrap
    property alias statusBarVisible: settings.statusBar
    readonly property string fontFamily: settings.fontFamily !== "" ? settings.fontFamily : App.fixedFont()
    property alias fontSize: settings.fontSize
    property int zoom: 100 // percent; like Windows, not remembered
    function zoomBy(delta) { zoom = Math.max(10, Math.min(500, zoom + delta)) }

    // --- tabs ---
    property var tabs: []
    property int currentIndex: 0
    readonly property var current: tabs[currentIndex] || null

    function newTab() {
        const tab = tabComponent.createObject(pages)
        tabs = tabs.concat([tab])
        currentIndex = tabs.length - 1
        tab.focusView()
        return tab
    }
    function removeTab(index) {
        const tab = tabs[index]
        const rest = tabs.slice()
        rest.splice(index, 1)
        tabs = rest
        tab.destroy()
        if (tabs.length === 0) {
            Qt.quit()
            return
        }
        currentIndex = Math.min(currentIndex, tabs.length - 1)
    }
    // Each file in its tab. A file that's already open is shown, and an
    // empty untouched tab is filled rather than left behind.
    function openFiles(urls) {
        for (const url of urls) {
            const open = tabs.findIndex(t => t.doc.url.toString() === url.toString())
            if (open >= 0) {
                currentIndex = open
                continue
            }
            const blank = current && current.doc.name === "" && !current.doc.modified && current.edit.length === 0
            const tab = blank ? current : newTab()
            const error = tab.doc.load(url)
            if (error !== "") {
                if (!blank) removeTab(tabs.indexOf(tab))
                showError(error)
            } else {
                tab.edit.cursorPosition = 0
            }
        }
    }
    // Closes a tab, asking first if it has unsaved changes; `then` runs once
    // it's closed (not if the question is cancelled).
    function closeTab(index, then) {
        const tab = tabs[index]
        const finish = () => {
            removeTab(tabs.indexOf(tab))
            if (then) then()
        }
        if (!tab.doc.modified) {
            finish()
            return
        }
        currentIndex = index
        saveDialog.text = qsTr("Do you want to save changes to %1?").arg(tab.title)
        saveDialog.answer = value => {
            if (value === "save") save(tab, false, saved => { if (saved) finish() })
            else if (value === "discard") finish()
        }
        saveDialog.open()
    }
    function closeAll() {
        if (tabs.length > 0) closeTab(tabs.length - 1, closeAll)
    }
    onClosing: close => {
        // shortcut: the files' names are remembered, not text that was never
        // saved (Windows keeps that too); write drafts here if it's missed.
        settings.session = JSON.stringify(tabs.filter(t => t.doc.name !== "").map(t => t.doc.url.toString()))
        if (tabs.some(t => t.doc.modified)) {
            close.accepted = false
            closeAll()
        }
    }
    onCurrentIndexChanged: if (current) current.focusView()
    Component.onCompleted: {
        if (StartFiles.length > 0) openFiles(StartFiles)
        else if (restoreSession && !FreshWindow) openFiles(JSON.parse(settings.session))
        if (tabs.length === 0) newTab()
    }

    Component {
        id: tabComponent
        Editor {
            anchors.fill: parent
            win: root
            visible: root.current === this
        }
    }

    // --- files ---
    property var picker: null
    function pick(saving, tab, done) {
        if (!picker) {
            const component = Qt.createComponent("FilePicker.qml")
            picker = component.createObject(root, { parentWindow: root })
            if (!picker) {
                showError(component.errorString())
                return
            }
        }
        const url = tab ? tab.doc.url.toString() : ""
        const folder = url !== "" ? url.slice(0, url.lastIndexOf("/")) : StandardPaths.writableLocation(StandardPaths.DocumentsLocation).toString()
        picker.pick(saving, folder, tab ? (tab.doc.name !== "" ? tab.doc.name : tab.title + ".txt") : "", done)
    }
    function open() { pick(false, current, files => openFiles(files)) }
    // `then(saved)`: false if it failed or the dialog was cancelled.
    function save(tab, saveAs, then) {
        const write = url => {
            const error = tab.doc.save(url)
            if (error !== "") showError(error)
            if (then) then(error === "")
        }
        if (tab.doc.name !== "" && !saveAs) {
            write(tab.doc.url)
            return
        }
        currentIndex = tabs.indexOf(tab)
        pick(true, tab, files => {
            if (files.length > 0) write(files[0])
            else if (then) then(false)
        })
    }
    function saveAll(from) {
        const index = tabs.findIndex((t, i) => i >= (from || 0) && t.doc.modified)
        if (index >= 0) save(tabs[index], false, saved => { if (saved) saveAll(index + 1) })
    }

    // --- menus ---
    function openMenu(entries, x, y, options) { menuLayer.open(entries, x, y, options) }
    function openBelow(entries, item, keyboard) {
        const p = item.mapToItem(null, 0, item.height)
        openMenu(entries, p.x, p.y + 2, { keyboard: !!keyboard })
    }
    function openAbove(entries, item) {
        const p = item.mapToItem(null, 0, 0)
        openMenu(entries, p.x, p.y - 2, { above: true })
    }
    function radio(values, currentValue, label, choose) {
        return values.map(v => ({ text: label(v), checked: v === currentValue, radio: true, action: () => choose(v) }))
    }
    function insertTimeDate() {
        const edit = current.edit
        edit.remove(edit.selectionStart, edit.selectionEnd)
        edit.insert(edit.cursorPosition, Qt.formatDateTime(new Date(), "HH:mm dd.MM.yyyy"))
    }
    function fileEntries() {
        return [
            { text: qsTr("&New tab"), icon: "tab-new", shortcut: "Ctrl+N", action: () => newTab() },
            { text: qsTr("New &window"), icon: "window-new", shortcut: "Ctrl+Shift+N", action: () => App.newWindow() },
            { text: qsTr("&Open..."), icon: "document-open", shortcut: "Ctrl+O", action: () => open() },
            { text: qsTr("&Save"), icon: "document-save", shortcut: "Ctrl+S", action: () => save(current, false) },
            { text: qsTr("Save &as..."), icon: "document-save-as", shortcut: "Ctrl+Shift+S", action: () => save(current, true) },
            { text: qsTr("Save a&ll"), icon: "document-save-all", shortcut: "Ctrl+Alt+S", action: () => saveAll() },
            { separator: true },
            { text: qsTr("&Print..."), icon: "document-print", shortcut: "Ctrl+P", action: () => current.doc.print(fontFamily) },
            { separator: true },
            { text: qsTr("&Close tab"), icon: "tab-close", shortcut: "Ctrl+W", action: () => closeTab(currentIndex) },
            { text: qsTr("Clos&e window"), icon: "window-close", shortcut: "Ctrl+Shift+W", action: () => root.close() }
        ]
    }
    function editEntries() {
        const edit = current.edit
        const selected = edit.selectedText !== ""
        return [
            { text: qsTr("&Undo"), icon: "edit-undo", shortcut: "Ctrl+Z", enabled: edit.canUndo, action: () => edit.undo() },
            { text: qsTr("&Redo"), icon: "edit-redo", shortcut: "Ctrl+Y", enabled: edit.canRedo, action: () => edit.redo() },
            { separator: true },
            { text: qsTr("Cu&t"), icon: "edit-cut", shortcut: "Ctrl+X", enabled: selected, action: () => edit.cut() },
            { text: qsTr("&Copy"), icon: "edit-copy", shortcut: "Ctrl+C", enabled: selected, action: () => edit.copy() },
            { text: qsTr("&Paste"), icon: "edit-paste", shortcut: "Ctrl+V", enabled: edit.canPaste, action: () => edit.paste() },
            { text: qsTr("&Delete"), icon: "edit-delete", shortcut: "Del", enabled: selected, action: () => edit.remove(edit.selectionStart, edit.selectionEnd) },
            { separator: true },
            { text: qsTr("&Find"), icon: "edit-find", shortcut: "Ctrl+F", action: () => showFind(false) },
            { text: qsTr("Find &next"), shortcut: "F3", action: () => findNext(false) },
            { text: qsTr("Find pre&vious"), shortcut: "Shift+F3", action: () => findNext(true) },
            { text: qsTr("R&eplace"), icon: "edit-find-replace", shortcut: "Ctrl+H", action: () => showFind(true) },
            { text: qsTr("&Go to"), icon: "go-jump", shortcut: "Ctrl+G", action: () => goTo() },
            { separator: true },
            { text: qsTr("Select &all"), icon: "edit-select-all", shortcut: "Ctrl+A", action: () => edit.selectAll() },
            { text: qsTr("Time/&Date"), shortcut: "F5", action: () => insertTimeDate() },
            { separator: true },
            { text: qsTr("F&ont..."), icon: "preferences-desktop-font", action: () => chooseFont() },
            { text: qsTr("Font si&ze"),
              children: radio([11, 12, 13, 14, 15, 16, 18, 20, 24, 28, 36], fontSize, s => String(s), s => fontSize = s) }
        ]
    }
    function zoomEntries() {
        return [
            { text: qsTr("Zoom &in"), icon: "zoom-in", shortcut: "Ctrl++", action: () => zoomBy(10) },
            { text: qsTr("Zoom &out"), icon: "zoom-out", shortcut: "Ctrl+-", action: () => zoomBy(-10) },
            { text: qsTr("&Restore default zoom"), icon: "zoom-original", shortcut: "Ctrl+0", action: () => zoom = 100 }
        ]
    }
    function viewEntries() {
        return [
            { text: qsTr("&Zoom"), children: zoomEntries() },
            { text: qsTr("&Status bar"), checked: statusBarVisible, action: () => statusBarVisible = !statusBarVisible },
            { text: qsTr("&Word wrap"), checked: wordWrap, action: () => wordWrap = !wordWrap }
        ]
    }

    // --- find, replace, go to: loaded when first used ---
    function showFind(replacing) {
        findLoader.active = true
        findLoader.item.show(replacing)
    }
    function findNext(backward) {
        if (findLoader.item && findLoader.item.hasText) findLoader.item.next(backward)
        else showFind(false)
    }
    function goTo() {
        goToLoader.active = true
        goToLoader.item.lines = current.doc.lineCount()
        goToLoader.item.line = current.cursor.x
        goToLoader.item.ask()
    }
    function chooseFont() {
        fontLoader.active = true
        fontLoader.item.families = App.fontFamilies()
        fontLoader.item.ask(fontFamily)
    }

    Shortcut { sequence: "Ctrl+N"; onActivated: root.newTab() }
    Shortcut { sequence: "Ctrl+Shift+N"; onActivated: App.newWindow() }
    Shortcut { sequence: "Ctrl+O"; onActivated: root.open() }
    Shortcut { sequence: "Ctrl+S"; onActivated: root.save(root.current, false) }
    Shortcut { sequence: "Ctrl+Shift+S"; onActivated: root.save(root.current, true) }
    Shortcut { sequence: "Ctrl+Alt+S"; onActivated: root.saveAll() }
    Shortcut { sequence: "Ctrl+P"; onActivated: root.current.doc.print(root.fontFamily) }
    Shortcut { sequence: "Ctrl+W"; onActivated: root.closeTab(root.currentIndex) }
    Shortcut { sequence: "Ctrl+Shift+W"; onActivated: root.close() }
    Shortcut { sequence: "Ctrl+Tab"; onActivated: root.currentIndex = (root.currentIndex + 1) % root.tabs.length }
    Shortcut { sequence: "Ctrl+Shift+Tab"; onActivated: root.currentIndex = (root.currentIndex + root.tabs.length - 1) % root.tabs.length }
    Shortcut { sequence: "Ctrl+F"; onActivated: root.showFind(false) }
    Shortcut { sequence: "Ctrl+H"; onActivated: root.showFind(true) }
    Shortcut { sequence: "F3"; onActivated: root.findNext(false) }
    Shortcut { sequence: "Shift+F3"; onActivated: root.findNext(true) }
    Shortcut { sequence: "Ctrl+G"; onActivated: root.goTo() }
    Shortcut { sequence: "F5"; onActivated: root.insertTimeDate() }
    Shortcut { sequences: ["Ctrl++", "Ctrl+="]; onActivated: root.zoomBy(10) }
    Shortcut { sequence: "Ctrl+-"; onActivated: root.zoomBy(-10) }
    Shortcut { sequence: "Ctrl+0"; onActivated: root.zoom = 100 }
    Shortcut { sequence: "Alt+F"; onActivated: root.openBelow(root.fileEntries(), fileButton, true) }
    Shortcut { sequence: "Alt+E"; onActivated: root.openBelow(root.editEntries(), editButton, true) }
    Shortcut { sequence: "Alt+V"; onActivated: root.openBelow(root.viewEntries(), viewButton, true) }

    // --- the title bar: icon, tabs, window buttons ---
    Item {
        id: strip
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 44

        // The empty part moves the window; a double click maximizes it.
        DragHandler {
            target: null
            onActiveChanged: if (active) root.startSystemMove()
        }
        TapHandler {
            onDoubleTapped: root.toggleMaximized()
        }

        Image {
            x: 14
            y: 16
            width: 16
            height: 16
            source: "image://appicon/accessories-text-editor"
            sourceSize: Qt.size(width, height)
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

        Row {
            id: tabRow
            x: 42
            anchors.bottom: parent.bottom
            spacing: 2
            // Room is left for the "+", the window buttons and a piece of bar to drag.
            readonly property real tabWidth: Math.max(100, Math.min(240, (strip.width - x - 40 - 3 * 46 - 48) / Math.max(1, root.tabs.length)))

            Repeater {
                model: root.tabs
                delegate: Rectangle {
                    id: tabButton
                    required property var modelData
                    required property int index
                    readonly property bool active: root.currentIndex === index
                    readonly property bool hovered: tabMouse.containsMouse || closeHover.hovered
                    width: tabRow.tabWidth
                    height: 36
                    topLeftRadius: Theme.radiusOverlay
                    topRightRadius: Theme.radiusOverlay
                    color: active ? Theme.layer : hovered ? Theme.subtleHover : "transparent"
                    Text {
                        x: 12
                        width: parent.width - 48
                        anchors.verticalCenter: parent.verticalCenter
                        text: tabButton.modelData.title
                        color: Theme.text
                        font.pixelSize: Theme.fontCaption + 1
                        font.weight: tabButton.active ? Font.DemiBold : Font.Normal
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        id: tabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                        onClicked: mouse => {
                            if (mouse.button === Qt.MiddleButton) {
                                root.closeTab(tabButton.index)
                            } else if (mouse.button === Qt.RightButton) {
                                const p = mapToItem(null, mouse.x, mouse.y)
                                const keep = tabButton.modelData
                                root.openMenu([
                                    { text: qsTr("&Close tab"), icon: "tab-close", shortcut: "Ctrl+W", action: () => root.closeTab(tabButton.index) },
                                    { text: qsTr("Close &other tabs"), enabled: root.tabs.length > 1, action: () => {
                                        // One at a time: each may ask to save, and Cancel stops the rest.
                                        const next = () => {
                                            const other = root.tabs.findIndex(t => t !== keep)
                                            if (other >= 0) root.closeTab(other, next)
                                        }
                                        next()
                                    } }
                                ], p.x, p.y)
                            } else {
                                root.currentIndex = tabButton.index
                            }
                        }
                    }
                    // Unsaved changes: a dot where the close button is, until
                    // the pointer is on the tab.
                    Rectangle {
                        anchors { right: parent.right; rightMargin: 14; verticalCenter: parent.verticalCenter }
                        width: 8
                        height: 8
                        radius: 4
                        color: Theme.text
                        visible: tabButton.modelData.doc.modified && !tabButton.hovered
                    }
                    ToolButton {
                        anchors { right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
                        width: 24
                        height: 24
                        icon.name: "window-close"
                        visible: tabButton.hovered || (tabButton.active && !tabButton.modelData.doc.modified)
                        onClicked: root.closeTab(tabButton.index)
                        HoverHandler { id: closeHover }
                    }
                }
            }
            ToolButton {
                anchors.verticalCenter: parent.verticalCenter
                width: 32
                height: 28
                icon.name: "list-add"
                tooltip: qsTr("New tab (Ctrl+N)")
                onClicked: root.newTab()
            }
        }
    }

    // --- the open tab: menus, text, status bar ---
    Rectangle {
        id: body
        anchors { left: parent.left; right: parent.right; top: strip.bottom; bottom: parent.bottom }
        color: Theme.layer

        Row {
            id: menuBar
            x: 2
            height: 36
            spacing: 8
            ToolButton {
                id: fileButton
                anchors.verticalCenter: parent.verticalCenter
                height: 28
                text: qsTr("File")
                onClicked: root.openBelow(root.fileEntries(), fileButton)
            }
            ToolButton {
                id: editButton
                anchors.verticalCenter: parent.verticalCenter
                height: 28
                text: qsTr("Edit")
                onClicked: root.openBelow(root.editEntries(), editButton)
            }
            ToolButton {
                id: viewButton
                anchors.verticalCenter: parent.verticalCenter
                height: 28
                text: qsTr("View")
                onClicked: root.openBelow(root.viewEntries(), viewButton)
            }
        }
        ToolButton {
            anchors { right: parent.right; rightMargin: 6; verticalCenter: menuBar.verticalCenter }
            height: 28
            icon.name: "configure"
            tooltip: qsTr("Settings")
            onClicked: settingsLoader.active = true
        }
        Item {
            id: pages
            anchors { left: parent.left; right: parent.right; top: menuBar.bottom; bottom: statusBar.visible ? statusBar.top : parent.bottom }

            // The text sits on a slightly lighter sheet than the menus and
            // the status bar.
            Rectangle {
                anchors.fill: parent
                color: Theme.card
            }

            // Files dragged onto the text open in tabs.
            DropArea {
                anchors.fill: parent
                keys: ["text/uri-list"]
                onDropped: drop => {
                    root.openFiles(drop.urls)
                    drop.accept()
                }
            }
        }

        Loader {
            id: findLoader
            anchors { top: pages.top; right: pages.right; topMargin: 8; rightMargin: 24 }
            z: 40
            active: false
            source: "FindBar.qml"
            onLoaded: {
                item.tab = Qt.binding(() => root.current)
                item.closed.connect(() => { if (root.current) root.current.focusView() })
            }
        }

        StatusBar {
            id: statusBar
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            visible: root.statusBarVisible
            tab: root.current
            zoom: root.zoom
            onZoomMenuRequested: item => root.openAbove(root.zoomEntries(), item)
            onLineEndingMenuRequested: item => root.openAbove(
                root.radio(["CRLF", "LF", "CR"], root.current.doc.lineEnding, lineEndingName, v => root.current.doc.lineEnding = v), item)
            onEncodingMenuRequested: item => root.openAbove(
                root.radio(root.current.doc.encodings, root.current.doc.encoding, v => v, v => root.current.doc.encoding = v), item)
        }
    }

    // --- dialogs ---
    function showError(message) {
        errorDialog.text = message
        errorDialog.open()
    }
    Dialog {
        id: errorDialog
        title: qsTr("Notepad")
        onChosen: if (root.current) root.current.focusView()
    }
    Dialog {
        id: saveDialog
        property var answer: null
        title: qsTr("Notepad")
        buttons: [
            { text: qsTr("Save"), accent: true, value: "save" },
            { text: qsTr("Don't save"), value: "discard" },
            { text: qsTr("Cancel"), value: "cancel" }
        ]
        onChosen: value => {
            const handler = answer
            answer = null
            if (root.current) root.current.focusView()
            if (handler) handler(value)
        }
    }
    // Settings, over the tab; loaded when first opened.
    Loader {
        id: settingsLoader
        anchors.fill: body
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
    Loader {
        id: goToLoader
        anchors.fill: parent
        z: 200
        active: false
        source: "GoToDialog.qml"
        onLoaded: item.accepted.connect(line => root.current.goToLine(line))
    }
    Connections {
        target: goToLoader.item
        function onChosen() { if (root.current) root.current.focusView() }
    }
    Loader {
        id: fontLoader
        anchors.fill: parent
        z: 200
        active: false
        source: "FontDialog.qml"
        onLoaded: item.accepted.connect(family => settings.fontFamily = family)
    }
    Connections {
        target: fontLoader.item
        function onChosen() { if (root.current) root.current.focusView() }
    }

    ResizeEdges {}

    MenuLayer {
        id: menuLayer
        anchors.fill: parent
        z: 100
        onVisibleChanged: if (!visible && root.current) root.current.focusView()
    }
}
