// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtCore
import Vela.Controls

// Explorer's window: tabs in the title bar, like Windows 11 (Ctrl+T, Ctrl+W,
// middle click on a folder), the open tab below. Menus, dialogs and copy
// progress live here too.
Window {
    id: root
    flags: Qt.Window | Qt.FramelessWindowHint // no title bar from Vela: the tabs are it
    width: 1180
    height: 720
    minimumWidth: 640
    minimumHeight: 420
    visible: true
    color: Theme.filesStrip // File Explorer keeps its colors when it loses the focus
    title: picking ? (Pick.title !== "" ? Pick.title : qsTr("File Explorer"))
        : current ? current.title + qsTr(" - File Explorer") : qsTr("File Explorer")

    onActiveChanged: Theme.windowActive = active
    onClosing: close => {
        if (Ops.activeJobCount > 0) {
            close.accepted = false
            activeTransfersDialog.open()
        }
    }

    // As a file chooser (vela-files --pick): one tab, no tab strip, the bottom bar of PickBar.
    readonly property bool picking: Pick.active
    readonly property alias pickBar: pickBar
    readonly property bool maximized: visibility === Window.Maximized
    function toggleMaximized() { maximized ? showNormal() : showMaximized() }

    // --- preferences, remembered across launches ---
    Settings {
        id: settings
        property string viewModes: "{}" // folder -> view
        property bool showHidden: false
        property bool showExtensions: true
        property bool itemChecks: false
        property bool compactView: false
        property bool navVisible: true
        property real navWidth: 205
        property string pane: "" // on the right: "", "preview" or "details"
        property real paneWidth: 313
    }
    property alias showHidden: settings.showHidden
    property alias showExtensions: settings.showExtensions
    property alias itemChecks: settings.itemChecks
    property alias compactView: settings.compactView
    property alias navVisible: settings.navVisible
    // With "File name extensions" off, a file shows its name without them.
    function displayName(name, isDir) {
        const dot = name.lastIndexOf(".")
        return showExtensions || isDir || dot <= 0 ? name : name.slice(0, dot)
    }
    property alias navWidth: settings.navWidth
    property alias pane: settings.pane
    property alias paneWidth: settings.paneWidth
    // Alt+P and Alt+Shift+P: the preview or details pane, or nothing.
    function togglePane(mode) { pane = pane === mode ? "" : mode }
    property var viewModes: JSON.parse(settings.viewModes)
    function viewModeFor(location) {
        if (viewModes[location]) return viewModes[location]
        // Images and videos look better as large icons, like in Windows.
        if (/\/(Immagini|Pictures|Video|Videos)$/.test(location)) return "large"
        return "details"
    }
    function setViewMode(location, mode) {
        const copy = Object.assign({}, viewModes)
        copy[location] = mode
        viewModes = copy
        settings.viewModes = JSON.stringify(copy)
    }

    // --- tabs ---
    property var tabs: []
    property int currentIndex: 0
    readonly property var current: tabs[currentIndex] || null

    function newTab(location, select) {
        const tab = tabComponent.createObject(pages, { location: location })
        tab.navigate(location, select)
        tabs = tabs.concat([tab])
        currentIndex = tabs.length - 1
    }
    function closeTab(index) {
        if (tabs.length === 1 && Ops.activeJobCount > 0) {
            activeTransfersDialog.open()
            return
        }
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
    function moveTab(from, to) {
        if (from === to) return
        const open = current
        const copy = tabs.slice()
        copy.splice(to, 0, copy.splice(from, 1)[0])
        tabs = copy
        currentIndex = tabs.indexOf(open)
    }
    onCurrentIndexChanged: if (current) current.focusView()
    Component.onCompleted: newTab(StartLocation, StartSelection)

    Component {
        id: tabComponent
        TabPage {
            anchors.fill: parent
            menus: menuLayer
            win: root
            visible: root.current === this
        }
    }

    // Eject (from any tab): the error only once.
    Connections {
        target: Places
        function onEjected(error) { if (error !== "") root.showError(error) }
    }

    // Back and Forward: Alt+arrows, the keyboard's Back/Forward keys and the
    // mouse's side buttons (common/navigationbuttons.cpp), whatever has the
    // focus.
    Shortcut { sequences: [StandardKey.Back]; onActivated: if (root.current) root.current.back() }
    Shortcut { sequences: [StandardKey.Forward]; onActivated: if (root.current) root.current.forward() }
    Shortcut { enabled: !root.picking; sequence: "Ctrl+T"; onActivated: root.newTab("home:", "") }
    Shortcut { enabled: !root.picking; sequence: "Ctrl+W"; onActivated: root.closeTab(root.currentIndex) }
    Shortcut { sequence: "Ctrl+Tab"; onActivated: root.currentIndex = (root.currentIndex + 1) % root.tabs.length }
    Shortcut { sequence: "Ctrl+Shift+Tab"; onActivated: root.currentIndex = (root.currentIndex + root.tabs.length - 1) % root.tabs.length }
    Shortcut { enabled: !root.picking; sequence: "Ctrl+N"; onActivated: Ops.newWindow(root.current ? root.current.location : "") }
    Shortcut { sequences: ["Ctrl+L", "Alt+D", "F4"]; onActivated: root.focusChild("address") }
    Shortcut { sequences: ["Ctrl+F", "Ctrl+E", "F3"]; onActivated: root.focusChild("search") }
    // The open tab's address bar and search.
    function focusChild(what) {
        if (!current) return
        for (let i = 0; i < current.children.length; ++i) {
            const c = current.children[i]
            if (c.focusAddress) {
                what === "address" ? c.focusAddress() : c.focusSearch()
                return
            }
        }
    }

    // --- the title bar, with the tabs ---
    Item {
        id: strip
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: root.visibility === Window.FullScreen ? 0 : 38
        visible: height > 0

        Rectangle { anchors.fill: parent; color: Theme.filesStrip }
        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: Theme.filesTabLine }

        // The bar's free part moves the window; a double click maximizes it.
        DragHandler {
            target: null
            onActiveChanged: if (active) root.startSystemMove()
        }
        TapHandler {
            onDoubleTapped: root.toggleMaximized()
        }

        Text {
            visible: root.picking
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.text
            font.pixelSize: Theme.fontBody
        }
        Row {
            id: tabRow
            visible: !root.picking
            x: 8
            anchors.bottom: parent.bottom
            spacing: -1 // File Explorer: a tab every 239
            // Room is left for "+", the window buttons and a piece of bar to drag.
            // A tab dragged along the bar: the others make room, the order changes on release.
            property int dragFrom: -1
            property real dragOffset: 0
            readonly property int dropIndex: dragFrom < 0 ? -1
                : Math.max(0, Math.min(root.tabs.length - 1, dragFrom + Math.round(dragOffset / (tabWidth + spacing))))
            readonly property real tabWidth: Math.max(100, Math.min(240, (strip.width - x - 40 - 3 * 46 - 48) / Math.max(1, root.tabs.length)))

            Repeater {
                model: root.tabs
                delegate: Fill {
                    id: tabButton
                    required property var modelData
                    required property int index
                    readonly property bool active: root.currentIndex === index
                    width: tabRow.tabWidth
                    height: 32
                    z: tabRow.dragFrom === index ? 2 : active ? 1 : 0 // its line and curves lie over its neighbors
                    transform: Translate {
                        x: tabRow.dragFrom < 0 ? 0
                            : tabRow.dragFrom === tabButton.index ? tabRow.dragOffset
                            : tabButton.index > tabRow.dragFrom && tabButton.index <= tabRow.dropIndex ? -(tabRow.tabWidth + tabRow.spacing)
                            : tabButton.index < tabRow.dragFrom && tabButton.index >= tabRow.dropIndex ? tabRow.tabWidth + tabRow.spacing
                            : 0
                        // WinUI's reorder hint settles in 200 ms.
                        Behavior on x {
                            enabled: tabRow.dragFrom >= 0 && tabRow.dragFrom !== tabButton.index
                            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                        }
                    }
                    radius: 0
                    topLeftRadius: Theme.radiusOverlay
                    topRightRadius: Theme.radiusOverlay
                    // WinUI's TabView: its states change at once, without a fade.
                    animated: false
                    color: active ? Theme.filesActiveTab : tabMouse.pressed ? Theme.filesTabPressed : tabMouse.containsMouse ? Theme.filesTabHover : "transparent"
                    Repeater {
                        model: tabButton.active ? [-1, 1] : []
                        // The curve into the toolbar: only the tab's color, so it
                        // shows nothing of its own over a neighbor.
                        delegate: Canvas {
                            required property int modelData
                            readonly property color fill: Theme.filesActiveTab
                            onFillChanged: requestPaint()
                            width: Theme.radiusOverlay
                            height: width
                            x: modelData < 0 ? -width : tabButton.width
                            anchors.bottom: parent.bottom
                            onPaint: {
                                const ctx = getContext("2d")
                                const r = width
                                ctx.clearRect(0, 0, r, r)
                                ctx.fillStyle = fill
                                ctx.beginPath()
                                if (modelData < 0) {
                                    ctx.moveTo(r, r)
                                    ctx.lineTo(0, r)
                                    ctx.arc(0, 0, r, Math.PI / 2, 0, true)
                                } else {
                                    ctx.moveTo(0, r)
                                    ctx.lineTo(r, r)
                                    ctx.arc(r, 0, r, Math.PI / 2, Math.PI, false)
                                }
                                ctx.closePath()
                                ctx.fill()
                                // File Explorer's line follows the curve.
                                ctx.strokeStyle = Theme.filesTabLine
                                ctx.lineWidth = 1
                                ctx.beginPath()
                                if (modelData < 0)
                                    ctx.arc(0, 0, r - 0.5, Math.PI / 2, 0, true)
                                else
                                    ctx.arc(r, 0, r - 0.5, Math.PI / 2, Math.PI, false)
                                ctx.stroke()
                            }
                        }
                    }
                    // The tab's line: under a tab at rest, and on the open tab's side.
                    Rectangle {
                        visible: !tabButton.active
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: root.currentIndex === tabButton.index - 1 ? Theme.radiusOverlay : 0 }
                        height: 1
                        color: Theme.filesTabLine
                    }
                    Repeater {
                        model: tabButton.active ? [-1, tabButton.width] : []
                        delegate: Rectangle {
                            required property int modelData
                            x: modelData
                            y: Theme.radiusOverlay
                            width: 1
                            height: tabButton.height - 2 * Theme.radiusOverlay
                            color: Theme.filesTabLine
                        }
                    }
                    Image {
                        visible: !tabButton.modelData.loading
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        source: "image://fileicon/" + encodeURIComponent(tabButton.modelData.icon)
                        sourceSize: Qt.size(width, height)
                    }
                    // File Explorer's while a folder loads: a turning ring in place of the icon.
                    Canvas {
                        visible: !!tabButton.modelData.loading
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = Theme.accent
                            ctx.lineWidth = 1.5
                            ctx.lineCap = "round"
                            ctx.beginPath()
                            ctx.arc(8, 8, 6.5, 0, Math.PI / 2, false)
                            ctx.stroke()
                        }
                        RotationAnimator on rotation {
                            running: !!tabButton.modelData.loading
                            from: 0
                            to: 360
                            duration: 1000
                            loops: Animation.Infinite
                        }
                    }
                    Text {
                        x: 36
                        width: parent.width - x - 37
                        anchors.verticalCenter: parent.verticalCenter
                        text: tabButton.modelData.title
                        color: tabButton.active ? Theme.text : Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                        font.weight: tabButton.active ? Font.DemiBold : Font.Normal
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        id: tabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                        preventStealing: true
                        property real pressX: 0
                        property bool moved: false
                        onPressed: mouse => {
                            pressX = mapToItem(tabRow, mouse.x, 0).x
                            moved = false
                        }
                        onPositionChanged: mouse => {
                            if (!(pressedButtons & Qt.LeftButton)) return
                            const dx = mapToItem(tabRow, mouse.x, 0).x - pressX
                            if (!moved && Math.abs(dx) < 4) return
                            if (!moved) {
                                moved = true
                                root.currentIndex = tabButton.index
                                tabRow.dragFrom = tabButton.index
                            }
                            tabRow.dragOffset = dx
                        }
                        onReleased: {
                            if (!moved) return
                            const from = tabRow.dragFrom, to = tabRow.dropIndex
                            tabRow.dragFrom = -1
                            tabRow.dragOffset = 0
                            root.moveTab(from, to)
                        }
                        onCanceled: { tabRow.dragFrom = -1; tabRow.dragOffset = 0 }
                        onClicked: mouse => {
                            if (moved) return
                            if (mouse.button === Qt.MiddleButton) {
                                root.closeTab(tabButton.index)
                            } else if (mouse.button === Qt.RightButton) {
                                const p = mapToItem(null, mouse.x, mouse.y)
                                menuLayer.open([
                                    { text: qsTr("&Duplicate tab"), icon: "tab-duplicate", action: () => root.newTab(tabButton.modelData.location, "") },
                                    { text: qsTr("Move to new &window"), icon: "window-new", action: () => { Ops.newWindow(tabButton.modelData.location); root.closeTab(tabButton.index) } },
                                    { separator: true },
                                    { text: qsTr("&Close tab"), icon: "tab-close", shortcut: "Ctrl+W", action: () => root.closeTab(tabButton.index) },
                                    { text: qsTr("Close &other tabs"), enabled: root.tabs.length > 1, action: () => {
                                        const keep = tabButton.modelData
                                        for (let i = root.tabs.length - 1; i >= 0; --i) if (root.tabs[i] !== keep) root.closeTab(i)
                                    } }
                                ], p.x, p.y)
                            } else {
                                root.currentIndex = tabButton.index
                            }
                        }
                    }
                    ToolButton {
                        anchors { right: parent.right; rightMargin: 5; verticalCenter: parent.verticalCenter }
                        width: 32
                        height: 24
                        icon.name: "window-close"
                        icon.width: 13 // the glyph comes out 9 wide, as measured on Windows
                        icon.height: 13
                        onClicked: root.closeTab(tabButton.index)
                    }
                    // WinUI's separator: between two tabs at rest, 8 from the top and the bottom.
                    Rectangle {
                        anchors { right: parent.right; top: parent.top; bottom: parent.bottom; topMargin: 8; bottomMargin: 8 }
                        width: 1
                        color: Theme.divider
                        visible: !tabButton.active && !tabMouse.containsMouse && root.currentIndex !== tabButton.index + 1 && tabButton.index + 1 < root.tabs.length
                    }
                    // Dragging files onto a tab opens it.
                    DropArea {
                        anchors.fill: parent
                        onEntered: root.currentIndex = tabButton.index
                    }
                }
            }
            Item { width: 9; height: 1 } // File Explorer: "+" starts 7 after the tab
            ToolButton {
                anchors.verticalCenter: parent.verticalCenter
                width: 32
                height: 24
                icon.name: "tab-new"
                icon.width: 12 // the glyph comes out 10 wide, as measured on Windows
                icon.height: 12
                onClicked: root.newTab("home:", "")
            }
        }
        Row {
            anchors { right: parent.right; top: parent.top }
            WindowButton { visible: !root.picking; glyph: "M 0 5.5 H 10"; onClicked: root.showMinimized() }
            WindowButton {
                visible: !root.picking
                glyph: root.maximized ? "M 0.5 2.5 H 7.5 V 9.5 H 0.5 Z M 2.5 2.5 V 0.5 H 9.5 V 7.5 H 7.5" : "M 0.5 0.5 H 9.5 V 9.5 H 0.5 Z"
                onClicked: root.toggleMaximized()
            }
            WindowButton { glyph: "M 0 0 L 10 10 M 10 0 L 0 10"; closes: true; onClicked: root.close() }
        }
    }

    Item {
        id: pages
        anchors { left: parent.left; right: parent.right; top: strip.bottom; bottom: root.picking ? pickBar.top : parent.bottom }
    }
    PickBar {
        id: pickBar
        visible: root.picking
        win: root
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
    }

    ProgressPanel {
        anchors { right: parent.right; bottom: parent.bottom; margins: 16; bottomMargin: 40 }
        z: 50
    }

    // --- dialogs ---
    Dialog {
        id: activeTransfersDialog
        title: qsTr("File transfer in progress")
        text: qsTr("Files are still being copied or moved. Keep File Explorer open until they finish, or cancel the transfer first.")
        buttons: [{ text: qsTr("Keep copying"), accent: true, value: "ok" }]
    }

    function showError(message) {
        errorDialog.text = message
        errorDialog.open()
    }
    property var confirmAction: null
    function confirm(title, text, yes, action) {
        confirmDialog.title = title
        confirmDialog.text = text
        confirmDialog.buttons = [{ text: yes, accent: true, value: "yes" }, { text: qsTr("Cancel"), value: "cancel" }]
        confirmAction = action
        confirmDialog.open()
    }

    // Rename several: one name for all, numbered "(1)", "(2)"...
    property var renamePaths: []
    function askRenameMany(paths) {
        renamePaths = paths
        const first = paths[0].substring(paths[0].lastIndexOf("/") + 1)
        const dot = first.lastIndexOf(".")
        renameName.text = dot > 0 ? first.substring(0, dot) : first
        renameDialog.open()
        renameName.forceActiveFocus()
        renameName.selectAll()
    }

    // Compress: the archive's name and format, like Windows' right click ›
    // "Compress". The folder never moves and taken names get " (2)".
    property var compressPaths: []
    property string compressLocation: ""
    function askCompress(paths) {
        compressPaths = paths
        compressLocation = root.current ? root.current.location : ""
        // The archive borrows the first item's name without the extension.
        const first = paths[0].substring(paths[0].lastIndexOf("/") + 1)
        const dot = first.lastIndexOf(".")
        compressName.text = dot > 0 ? first.substring(0, dot) : first
        zipRadio.checked = true
        sevenRadio.checked = false
        tarRadio.checked = false
        compressDialog.open()
        compressName.forceActiveFocus()
        compressName.selectAll()
    }
    Connections {
        target: Ops
        function onFailed(message) { root.showError(message) }
        function onImageMounted(mountPoint) { if (root.current) root.current.navigate(mountPoint, "") }
        // The destination already has files with the same name: like Windows.
        function onConflict(count, firstName, directory) {
            conflictDialog.title = qsTr("Replace or skip files")
            conflictDialog.text = count === 1
                ? qsTr("The destination already has a file named \"") + firstName + "\"."
                : qsTr("The destination already has ") + count + qsTr(" files with the same name (\"") + firstName + qsTr("\" and others).")
            conflictDialog.open()
        }
    }
    Dialog {
        id: errorDialog
        title: qsTr("File Explorer")
        onChosen: if (root.current) root.current.focusView()
    }
    Dialog {
        id: confirmDialog
        onChosen: value => {
            if (value === "yes" && root.confirmAction) root.confirmAction()
            root.confirmAction = null
            if (root.current) root.current.focusView()
        }
    }
    Dialog {
        id: conflictDialog
        dialogWidth: 600
        buttons: [
            { text: qsTr("Replace"), accent: true, value: "replace" },
            { text: qsTr("Skip"), value: "skip" },
            { text: qsTr("Keep both"), value: "keep" },
            { text: qsTr("Cancel"), value: "cancel" }
        ]
        onChosen: value => Ops.resolveConflict(value)
    }
    Dialog {
        id: renameDialog
        title: qsTr("Rename")
        buttons: [{ text: qsTr("Rename"), accent: true, value: "rename" }, { text: qsTr("Cancel"), value: "cancel" }]
        onChosen: value => {
            if (value === "rename" && root.renamePaths.length > 0) {
                const result = Ops.renameMany(root.renamePaths, renameName.text)
                if (result.startsWith("!")) {
                    root.showError(result.substring(1))
                    root.renamePaths = []
                    return
                }
            }
            root.renamePaths = []
            if (root.current) root.current.focusView()
        }
        content: Row {
            spacing: 12
            Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Name"); color: Theme.text; font.pixelSize: Theme.fontBody }
            // Enter renames: the text box eats Return.
            TextBox {
                id: renameName
                width: 280
                Keys.onReturnPressed: renameDialog.close("rename")
            }
            Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("(1), (2)..."); color: Theme.textSecondary; font.pixelSize: Theme.fontCaption }
        }
    }
    Dialog {
        id: compressDialog
        readonly property string format: zipRadio.checked ? "zip" : sevenRadio.checked ? "7z" : "tar.gz"
        title: qsTr("Compress")
        buttons: [{ text: qsTr("Compress"), accent: true, value: "compress" }, { text: qsTr("Cancel"), value: "cancel" }]
        onChosen: value => {
            if (value === "compress" && root.compressPaths.length > 0)
                Ops.compress(root.compressPaths, root.compressLocation, compressName.text, format)
            root.compressPaths = []
            if (root.current) root.current.focusView()
        }
        content: Grid {
            columns: 2
            spacing: 12
            verticalItemAlignment: Grid.AlignVCenter
            Text { text: qsTr("Name"); color: Theme.text; font.pixelSize: Theme.fontBody }
            // Enter compresses: the text box eats Return, so the panel's
            // accent-button handling never sees it.
            TextBox {
                id: compressName
                width: 240
                Keys.onReturnPressed: compressDialog.close("compress")
            }
            Text { text: qsTr("Format"); color: Theme.text; font.pixelSize: Theme.fontBody }
            Row {
                spacing: 12
                RadioButton {
                    id: zipRadio
                    text: qsTr("ZIP file")
                    onClicked: {
                        checked = true
                        sevenRadio.checked = false
                        tarRadio.checked = false
                    }
                }
                RadioButton {
                    id: sevenRadio
                    text: qsTr("7z file")
                    onClicked: {
                        checked = true
                        zipRadio.checked = false
                        tarRadio.checked = false
                    }
                }
                RadioButton {
                    id: tarRadio
                    text: qsTr("tar.gz file")
                    onClicked: {
                        checked = true
                        zipRadio.checked = false
                        sevenRadio.checked = false
                    }
                }
            }
        }
    }

    ResizeEdges {}

    MenuLayer {
        id: menuLayer
        anchors.fill: parent
        z: 100
        onVisibleChanged: if (!visible && root.current) root.current.focusView()
    }
}
