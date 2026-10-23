// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Shapes
import QtQuick.Effects
import Vela.Controls

// The Start menu: search at the top, all apps below, user at the bottom. It
// opens rising and fading; it closes on a click outside or with Esc.
Window {
    id: root

    visible: false
    width: Theme.startMenuWidth + (Config.taskbarVertical ? 12 : 0)
    // On short outputs it gets shorter: the search at the top must stay
    // visible.
    height: Math.min(Theme.startMenuHeight, Screen.height - (Config.taskbarVertical ? 0 : Theme.taskbarHeight) - 12)
    color: "transparent"

    property bool closing: false
    // Pinned tiles are in the GridView header rather than its model.
    // -1 means the keyboard selection is in the app model.
    property int selectedPinnedIndex: -1
    property string shortcutError: ""
    // Asked of logind only once: can the computer suspend?
    readonly property bool canSuspend: Shell.canSuspend()

    // The apps pinned to Start, installed, without duplicates (the same app as
    // a package and as a Flatpak).
    readonly property var pinned: {
        // entry(id) is not observable: depend explicitly on catalogue changes.
        Apps.catalogRevision
        const seen = {}
        const out = []
        for (const id of Shell.startPins) {
            const e = Apps.entry(id)
            if (e.name && !seen[e.name]) {
                seen[e.name] = true
                out.push({ id: id, name: e.name, iconName: e.iconName })
            }
        }
        return out
    }
    readonly property bool showPinned: search.text.length === 0 && pinned.length > 0

    // The extra results of a search, in screen order, below the apps: settings, then files.
    readonly property var extras: {
        const q = search.text.trim().toLowerCase()
        if (q.length === 0) {
            return []
        }
        const out = []
        let n = 0
        for (const e of Menus.settingsIndex) {
            if (n < 4 && (e.text.toLowerCase().indexOf(q) >= 0 || e.keys.toLowerCase().indexOf(q) >= 0)) {
                out.push({ kind: "setting", name: e.text, detail: qsTr("Settings"), page: e.page })
                n++
            }
        }
        for (const f of FileSearch.results) {
            out.push({ kind: "file", name: f.name, detail: f.path.replace(System.homeDirectory(), "~"), path: f.path })
        }
        return out
    }
    // The selected extra result; -1: the selection is in the app grid.
    property int extraIndex: -1

    // --- search, like Windows 11: tabs, the results in a list (the best
    // match first) and the selected one's details and actions beside it ---
    readonly property bool searching: search.text.trim().length > 0
    property bool updatePending: false // asked when the power menu opens
    property string searchFilter: "all" // all, apps, settings, files
    readonly property var results: {
        const q = search.text.trim()
        if (q.length === 0) {
            return []
        }
        void Apps.query // the model is filtered when the query is set, after the text
        const out = []
        const all = searchFilter === "all"
        if (all || searchFilter === "apps") {
            const n = all ? Math.min(Apps.count, 6) : Apps.count
            for (let i = 0; i < n; ++i) {
                const id = Apps.idAt(i)
                const e = Apps.entry(id)
                out.push({ kind: "app", id: id, name: e.name, iconName: e.iconName, detail: qsTr("App") })
            }
        }
        // The web right after the apps, like Windows 11.
        if (all) {
            out.push({ kind: "web", name: q, iconName: Apps.browserIcon(), detail: qsTr("See web results") })
        }
        for (const e of extras) {
            if (e.kind === "setting" && (all || searchFilter === "settings")) {
                out.push({ kind: "setting", name: e.name, iconName: "preferences-system", detail: qsTr("Settings"), page: e.page })
            } else if (e.kind === "file" && (all || searchFilter === "files")) {
                out.push({ kind: "file", name: e.name, iconName: "text-x-generic", detail: e.detail, path: e.path })
            }
        }
        return out
    }
    property int resultIndex: 0
    onResultsChanged: resultIndex = 0
    readonly property var result: results[resultIndex] ?? null

    function openResult(r) {
        if (!r) {
            return
        }
        if (r.kind === "app") {
            if (Apps.launchId(r.id)) {
                close()
            }
            return
        }
        if (r.kind === "file") {
            System.openUrl(r.path)
        } else if (r.kind === "setting") {
            System.openSettingsPage(r.page)
        } else {
            System.openUrl("https://www.google.com/search?q=" + encodeURIComponent(r.name))
        }
        close()
    }
    // The selected result's actions, at the right: an app's are those of its
    // right-click menu, flattened.
    function resultActions(r) {
        if (!r) {
            return []
        }
        const open = { text: qsTr("Open"), icon: "document-open", action: () => root.openResult(r) }
        if (r.kind === "file") {
            return [open, { text: qsTr("Open file location"), icon: "document-open-folder", action: () => { System.showInFolder(r.path); root.close() } }]
        }
        if (r.kind !== "app") {
            return [open]
        }
        const out = [open]
        const add = entries => entries.forEach(e => {
            if (e.children) {
                add(e.children)
            } else if (e.text && e.enabled !== false) {
                out.push({ text: e.text.replace(/&(?!&)/g, "").replace(/&&/g, "&"), icon: e.icon, action: e.action })
            }
        })
        add(appMenu(r.id, false))
        return out
    }
    function moveResult(delta) {
        if (results.length > 0) {
            resultIndex = Math.max(0, Math.min(results.length - 1, resultIndex + delta))
        }
    }
    function cycleFilter(delta) {
        const filters = ["all", "apps", "settings", "files"]
        searchFilter = filters[(filters.indexOf(searchFilter) + delta + filters.length) % filters.length]
    }

    function openExtra(i) {
        const e = extras[i]
        if (!e) {
            return
        }
        if (e.kind === "file") {
            System.openUrl(e.path)
        } else {
            System.openSettingsPage(e.page)
        }
        close()
    }
    // Up/Down between the app grid and the extras below it (only while
    // searching, so never next to the pinned apps); false: not handled here.
    function moveExtra(delta) {
        const next = extraIndex + delta
        if (extraIndex >= 0) {
            if (next < 0 && grid.count > 0) {
                extraIndex = -1
                selectApp(grid.count - 1)
            } else {
                extraIndex = Math.max(0, Math.min(extras.length - 1, next))
                grid.positionViewAtEnd()
            }
            return true
        }
        // Down from the last row of apps goes to the extras below.
        if (delta > 0 && extras.length > 0 && Math.floor(grid.currentIndex / Theme.startMenuColumns) >= Math.floor((grid.count - 1) / Theme.startMenuColumns)) {
            extraIndex = 0
            grid.positionViewAtEnd()
            return true
        }
        return false
    }
    onExtrasChanged: {
        // No app matches: the keyboard goes to the first extra.
        extraIndex = extras.length > 0 && grid.count === 0 ? 0 : -1
    }

    // The Start menu is above the taskbar, centered or at the left: from its
    // coordinates to the output's, for right-click menus.
    function screenPoint(item, x, y) {
        const p = item.mapToItem(null, x, y)
        if (Config.taskbarVertical) {
            // At the top, next to the taskbar (placeStartMenu).
            return Qt.point((Config.taskbarLeft ? Theme.taskbarHeight : Screen.width - Theme.taskbarHeight - root.width) + p.x, p.y)
        }
        const left = Config.taskbarAlignment === "left" ? 12 : (Screen.width - root.width) / 2 // placeStartMenu
        return Qt.point(left + p.x, Config.taskbarTop ? Theme.taskbarHeight + p.y : Screen.height - Theme.taskbarHeight - root.height + p.y)
    }

    function addToDesktop(id) {
        const path = Apps.desktopFile(id)
        const error = path === ""
            ? qsTr("Couldn't find this application's launcher.")
            : FileActions.createShortcut(Desktop.directory, path, "")
        if (error !== "") {
            shortcutError = error
            shortcutErrorTimer.restart()
            return
        }
        shortcutError = ""
        Desktop.refresh()
        root.close()
    }

    // An app's menu (docs/renderer.md §14.7): recent files and tasks at the
    // top, like the jump list; then the Start entries.
    function appMenu(id, pinnedTile) {
        const entries = []
        const recent = Jumps.recent(id, 5)
        if (recent.length > 0) {
            entries.push({ header: qsTr("Recent") })
            recent.forEach(f => entries.push({ text: f.name.replace(/&/g, "&&"), icon: f.icon, action: () => { Apps.launchWithFile(id, f.url); root.close() } }))
        }
        const actions = Apps.actions(id)
        if (actions.length > 0) {
            entries.push({ header: qsTr("Tasks") })
            actions.forEach(a => entries.push({ text: a.name.replace(/&/g, "&&"), icon: a.icon, action: () => { Apps.launchAction(id, a.id); root.close() } }))
        }
        if (entries.length > 0) {
            entries.push({ separator: true })
        }
        const inStart = Shell.startPins.indexOf(id) >= 0
        const taskbar = Tasks.isPinned(id)
            ? { text: qsTr("Unpin from &taskbar"), icon: "window-unpin", action: () => Tasks.unpin(id) }
            : { text: qsTr("Pin to &taskbar"), icon: "window-pin", action: () => Tasks.pin(id) }
        const desktop = { text: qsTr("Add to &desktop"), icon: "user-desktop", action: () => root.addToDesktop(id) }
        const folder = { text: qsTr("Open file &location"), icon: "document-open-folder", action: () => { System.showInFolder(Apps.desktopFile(id)); root.close() } }
        const uninstall = { text: qsTr("&Uninstall"), icon: "edit-delete", enabled: System.canUninstall(Apps.desktopFile(id)), action: () => { System.uninstall(Apps.desktopFile(id)); root.close() } }
        if (pinnedTile) {
            entries.push(
                { text: qsTr("&Unpin from Start"), icon: "window-unpin", action: () => Shell.unpinFromStart(id) },
                { text: qsTr("Move to &front"), icon: "go-top", enabled: Shell.startPins.indexOf(id) > 0, action: () => Shell.moveStartPinToFront(id) },
                taskbar, desktop, folder, uninstall)
        } else {
            entries.push(
                inStart
                    ? { text: qsTr("&Unpin from Start"), icon: "window-unpin", action: () => Shell.unpinFromStart(id) }
                    : { text: qsTr("&Pin to Start"), icon: "window-pin", action: () => Shell.pinToStart(id) },
                desktop,
                { text: qsTr("&More"), children: [taskbar, folder] },
                uninstall)
        }
        return entries
    }

    function openAppMenu(item, x, y, id, pinnedTile) {
        const p = screenPoint(item, x, y)
        Menus.open(appMenu(id, pinnedTile), p.x, p.y, { screen: root.screen ? root.screen.name : Shell.primaryScreen })
    }

    // An entry of the power menu.
    component PowerEntry: Item {
        id: entry
        property string icon // from the theme; empty: the symbol drawn inside
        property string label
        default property alias glyph: glyphSlot.data
        signal activated()
        width: parent ? parent.width : 0
        height: 36

        Rectangle {
            anchors.fill: parent
            radius: Theme.radius
            color: entryMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
            opacity: entryMouse.containsMouse ? 1 : 0
        }
        Image {
            x: 10
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            visible: entry.icon !== ""
            source: entry.icon !== "" ? Theme.icons + entry.icon : ""
            sourceSize: Qt.size(width, height)
        }
        Item {
            id: glyphSlot
            x: 10
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
        }
        Text {
            x: 36
            anchors.verticalCenter: parent.verticalCenter
            text: entry.label
            color: Theme.text
            font.pixelSize: Theme.fontBody
        }
        MouseArea {
            id: entryMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: entry.activated()
        }
    }

    // The blurred background under the panel (which rises while opening).
    function updateBlur() {
        const top = Math.max(0, panel.y)
        Effects.setBlur(root, [Qt.rect(panel.x, top, panel.width, Math.max(0, Math.min(panel.y + panel.height, root.height) - top))], Theme.startMenuRadius)
    }
    Connections {
        target: panel
        function onXChanged() { root.updateBlur() }
        function onYChanged() { root.updateBlur() }
        function onHeightChanged() { root.updateBlur() }
        function onWidthChanged() { root.updateBlur() }
    }

    function open() {
        // A second Super can reopen the menu while it is still closing.
        // The old animation must not finish by hiding the reopened window.
        closeAnimation.stop()
        closing = false
        powerButton.menuOpen = false
        Apps.query = ""
        extraIndex = -1
        searchFilter = "all"
        search.text = ""
        grid.currentIndex = 0
        grid.keyNav = false
        selectedPinnedIndex = -1
        shortcutError = ""
        shortcutErrorTimer.stop()
        grid.positionViewAtBeginning()
        panel.opacity = 0
        panel.offset = panel.slide
        if (!Menus.placeOnTargetScreen(root)) {
            Qt.callLater(root.open)
            return
        }
        visible = true
        Shell.startMenuOpen = true
        openAnimation.restart()
        search.forceActiveFocus()
    }

    function close() {
        if (!visible || closing) {
            return
        }
        closing = true
        Shell.startMenuOpen = false
        openAnimation.stop()
        closeAnimation.restart()
    }

    function launch(row) {
        if (row >= 0 && row < grid.count && Apps.launch(row)) {
            close()
        }
    }

    function launchSelection() {
        if (searching) {
            openResult(result)
        } else if (extraIndex >= 0) {
            openExtra(extraIndex)
        } else if (showPinned && selectedPinnedIndex >= 0 && selectedPinnedIndex < pinned.length) {
            if (Apps.launchId(pinned[selectedPinnedIndex].id)) {
                close()
            }
        } else {
            launch(grid.currentIndex)
        }
    }

    function selectPinned(index) {
        selectedPinnedIndex = index
        // A long pinned section may itself be taller than the visible grid.
        const top = Math.floor(index / Theme.startMenuColumns) * grid.cellHeight
        if (grid.contentY > top) grid.contentY = top
        else if (grid.contentY + grid.height < top + grid.cellHeight)
            grid.contentY = top + grid.cellHeight - grid.height
    }

    function selectApp(index) {
        selectedPinnedIndex = -1
        grid.currentIndex = index
        grid.positionViewAtIndex(index, GridView.Contain)
    }

    // Navigate between the pinned header and the app model by column.
    // Left/right wrap inside the current section; up/down cross sections.
    function navigate(direction) {
        if (direction === "up" || direction === "down") {
            if (moveExtra(direction === "down" ? 1 : -1)) {
                grid.keyNav = true
                return
            }
        } else if (extraIndex >= 0) {
            return // Settings and Files rows: nothing to the sides
        }
        const pinCount = showPinned ? pinned.length : 0
        const appCount = grid.count
        if (pinCount === 0 && appCount === 0) return

        // No tile is highlighted before keyboard navigation starts, except
        // search results. The first arrow should select, not skip, tile zero.
        if (!grid.keyNav && search.text.length === 0) {
            grid.keyNav = true
            if (pinCount > 0) selectPinned(0)
            else selectApp(0)
            return
        }

        grid.keyNav = true
        const columns = Theme.startMenuColumns
        if (selectedPinnedIndex >= 0 && pinCount > 0) {
            const index = selectedPinnedIndex
            if (direction === "left") selectPinned((index + pinCount - 1) % pinCount)
            else if (direction === "right") selectPinned((index + 1) % pinCount)
            else if (direction === "down") {
                if (index + columns < pinCount) selectPinned(index + columns)
                else if (appCount > 0) selectApp(Math.min(index % columns, appCount - 1))
                else selectPinned(index % columns)
            } else if (direction === "up") {
                if (index >= columns) selectPinned(index - columns)
                else if (appCount > 0) {
                    const lastRow = Math.floor((appCount - 1) / columns) * columns
                    selectApp(Math.min(lastRow + index % columns, appCount - 1))
                } else {
                    const lastRow = Math.floor((pinCount - 1) / columns) * columns
                    selectPinned(Math.min(lastRow + index % columns, pinCount - 1))
                }
            }
        } else if (appCount > 0) {
            const index = Math.max(0, grid.currentIndex)
            if (direction === "left") selectApp((index + appCount - 1) % appCount)
            else if (direction === "right") selectApp((index + 1) % appCount)
            else if (direction === "down") {
                if (index + columns < appCount) selectApp(index + columns)
                else if (pinCount > 0) selectPinned(Math.min(index % columns, pinCount - 1))
                else selectApp(index % columns)
            } else if (direction === "up") {
                if (index >= columns) selectApp(index - columns)
                else if (pinCount > 0) {
                    const lastRow = Math.floor((pinCount - 1) / columns) * columns
                    selectPinned(Math.min(lastRow + index % columns, pinCount - 1))
                } else {
                    const lastRow = Math.floor((appCount - 1) / columns) * columns
                    selectApp(Math.min(lastRow + index % columns, appCount - 1))
                }
            }
        } else {
            selectPinned(0)
        }
    }

    Timer {
        id: shortcutErrorTimer
        interval: 7000
        onTriggered: root.shortcutError = ""
    }

    Connections {
        target: Shell
        function onToggleStartRequested() {
            if (root.visible && !root.closing) {
                root.close()
            } else {
                root.open()
            }
        }
    }

    // A click on a window or elsewhere: the menu loses focus and closes, like
    // on Windows. A right-click menu opened from here takes the keyboard: the
    // Start menu stays open, and gets it back when the menu closes. Once the
    // menu closes, the keyboard comes back here; if it went elsewhere instead
    // (a new window, a click), this panel closes too.
    Connections {
        target: Menus
        function onIsOpenChanged() {
            if (!Menus.isOpen && root.visible) {
                focusCheck.restart()
            }
        }
    }
    Timer {
        id: focusCheck
        interval: 200
        onTriggered: {
            if (!root.active && root.visible && !Menus.isOpen) {
                root.close()
            }
        }
    }

    onActiveChanged: {
        if (!active && visible && !Menus.isOpen) {
            close()
        }
    }

    ParallelAnimation {
        id: openAnimation
        NumberAnimation {
            target: panel; property: "offset"; to: 0
            duration: Theme.slow
            easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate
        }
        NumberAnimation {
            target: panel; property: "opacity"; to: 1
            duration: Theme.normal
            easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate
        }
    }

    SequentialAnimation {
        id: closeAnimation
        ParallelAnimation {
            NumberAnimation {
                target: panel; property: "offset"; to: panel.slide
                duration: Theme.normal
                easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.accelerate
            }
            NumberAnimation {
                target: panel; property: "opacity"; to: 0
                duration: Theme.normal
                easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.accelerate
            }
        }
        ScriptAction {
            script: {
                root.visible = false
                root.closing = false
            }
        }
    }

    // A soft shadow around the panel (not below: it's acrylic), small enough
    // for the window's 10 pixels around the panel: cut by them it was square.
    PanelShadow {
        target: panel
        opacity: panel.opacity
        radius: Theme.startMenuRadius
        blur: 10
        offset: Qt.vector2d(0, 3)
        color: Qt.rgba(0, 0, 0, 0.5)
    }

    Rectangle {
        id: panel

        // 10 above and 22 below (12 of gap from the taskbar); the other way
        // round with the taskbar at the top.
        // Next to a vertical taskbar it is 10 from the top and the same on the
        // far side, 22 from the taskbar.
        readonly property bool side: Config.taskbarVertical
        readonly property real restY: side ? 10 : (Config.taskbarTop ? 22 : 10)
        readonly property real restX: side && Config.taskbarLeft ? 22 : 10
        // How far it rises while opening (falls, at the top, comes in from
        // the side next to a vertical taskbar). The part sticking out is cut
        // at the taskbar's edge.
        readonly property real slide: side ? (Config.taskbarLeft ? -140 : 140) : (Config.taskbarTop ? -140 : 140)
        property real offset: 0
        x: restX + (side ? offset : 0)
        y: restY + (side ? 0 : offset)
        width: root.width - (side ? 32 : 20)
        height: root.height - (side ? 20 : 32) // 10 above, 22 below: 12 of gap from the taskbar
        radius: Theme.startMenuRadius
        color: Theme.startMenuSurface
        border.width: 1
        border.color: Theme.stroke

        // --- search ---
        Rectangle {
            id: searchBox
            anchors { top: parent.top; left: parent.left; right: parent.right; margins: 24 }
            height: 38
            radius: height / 2
            color: Theme.control
            border.width: search.activeFocus ? 1 : 0
            border.color: Theme.accent

            Text {
                anchors { left: parent.left; leftMargin: 18; verticalCenter: parent.verticalCenter }
                visible: search.text.length === 0
                text: qsTr("Search apps, files and settings")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
            }

            MenuTextField {
                id: search
                anchors { left: parent.left; right: parent.right; leftMargin: 18; rightMargin: 18; verticalCenter: parent.verticalCenter }
                mapToScreen: (x, y) => root.screenPoint(search, x, y)

                onTextChanged: {
                    Apps.query = text
                    FileSearch.query = text
                    root.extraIndex = -1
                    grid.currentIndex = 0
                    root.selectedPinnedIndex = -1
                    grid.keyNav = false
                    grid.positionViewAtBeginning()
                }

                // The keyboard stays in the search; the arrows move the grid.
                Keys.onEscapePressed: root.close()
                Keys.onReturnPressed: root.launchSelection()
                Keys.onEnterPressed: root.launchSelection()
                Keys.onDownPressed: root.searching ? root.moveResult(1) : root.navigate("down")
                Keys.onUpPressed: root.searching ? root.moveResult(-1) : root.navigate("up")
                Keys.onTabPressed: root.searching ? root.cycleFilter(1) : root.navigate("right")
                Keys.onBacktabPressed: root.searching ? root.cycleFilter(-1) : root.navigate("left")
                // A click in the field is editing again. The default (passive) policy: the
                // text input grabs the mouse itself, which cancels the other policies.
                TapHandler {
                    onTapped: { grid.keyNav = false; root.extraIndex = -1 }
                }
                // Left and Right edit text if the search isn't empty.
                Keys.onLeftPressed: event => {
                    if (text.length > 0) { event.accepted = false; return }
                    root.navigate("left")
                }
                Keys.onRightPressed: event => {
                    if (text.length > 0) { event.accepted = false; return }
                    root.navigate("right")
                }
            }
        }

        Text {
            id: sectionTitle
            visible: !root.searching
            anchors { top: searchBox.bottom; left: parent.left; topMargin: 20; leftMargin: 32 }
            text: search.text.length > 0 ? qsTr("Results") : root.showPinned ? qsTr("Pinned") : qsTr("All apps")
            color: Theme.text
            font.pixelSize: Theme.fontBody
            font.weight: Font.DemiBold
        }

        // --- app grid ---
        GridView {
            id: grid
            visible: !root.searching
            anchors {
                top: sectionTitle.bottom; bottom: footer.top
                left: parent.left; right: parent.right
                topMargin: 10; leftMargin: 20; rightMargin: 20; bottomMargin: 8
            }
            clip: true
            model: Apps
            cellWidth: Math.floor(width / Theme.startMenuColumns)
            cellHeight: Theme.startMenuCellHeight
            keyNavigationWraps: true
            boundsBehavior: Flickable.StopAtBounds
            highlightFollowsCurrentItem: false
            // Search results are selected immediately; otherwise the first
            // navigation key makes the selection visible.
            property bool keyNav: false

            // With the arrows the selection can leave the view: we follow it.
            onCurrentIndexChanged: if (currentIndex >= 0 && root.selectedPinnedIndex < 0)
                positionViewAtIndex(currentIndex, GridView.Contain)

            delegate: AppTile {
                id: tile
                required property string appId
                width: grid.cellWidth
                height: grid.cellHeight
                current: root.extraIndex < 0 && root.selectedPinnedIndex < 0 && index === grid.currentIndex
                    && (search.text.length > 0 || grid.keyNav)
                onActivated: root.launch(index)
                onContextRequested: (x, y) => root.openAppMenu(tile, x, y, tile.appId, false)
            }

            // Above all apps, those pinned to Start.
            header: Item {
                width: grid.width
                height: root.showPinned ? pinnedGrid.height + allTitle.height + 24 : 0
                visible: root.showPinned

                Grid {
                    id: pinnedGrid
                    visible: root.showPinned
                    columns: Theme.startMenuColumns

                    Repeater {
                        model: root.pinned

                        delegate: AppTile {
                            id: pinnedTile
                            required property var modelData
                            width: grid.cellWidth
                            height: grid.cellHeight
                            name: modelData.name
                            iconName: modelData.iconName
                            comment: ""
                            current: grid.keyNav && root.showPinned && root.selectedPinnedIndex === index
                            onActivated: {
                                if (Apps.launchId(modelData.id)) {
                                    root.close()
                                }
                            }
                            onContextRequested: (x, y) => root.openAppMenu(pinnedTile, x, y, modelData.id, true)
                        }
                    }
                }
                Text {
                    id: allTitle
                    visible: root.showPinned
                    anchors { top: pinnedGrid.bottom; topMargin: 16; left: parent.left; leftMargin: 12 }
                    text: qsTr("All apps")
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.weight: Font.DemiBold
                }
            }

            footer: Item {
                width: grid.width
                height: extraColumn.height
                visible: root.extras.length > 0

            // Settings and Files matches, below the apps.
            Column {
                id: extraColumn
                width: parent.width
                bottomPadding: root.extras.length > 0 ? 8 : 0

                Repeater {
                    model: root.extras

                    delegate: Column {
                        id: extraRow
                        required property var modelData
                        required property int index
                        width: extraColumn.width
                        readonly property bool first: index === 0 || root.extras[index - 1].kind !== modelData.kind

                        Text {
                            visible: extraRow.first
                            leftPadding: 12
                            topPadding: 8
                            bottomPadding: 4
                            text: extraRow.modelData.kind === "file" ? qsTr("Files") : qsTr("Settings")
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            font.weight: Font.DemiBold
                        }
                        Item {
                            width: parent.width
                            height: 36

                            Rectangle {
                                anchors.fill: parent
                                anchors.leftMargin: 3
                                anchors.rightMargin: 3
                                radius: Theme.radius
                                color: extraMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
                                opacity: extraMouse.containsMouse || root.extraIndex === extraRow.index ? 1 : 0
                                border.width: root.extraIndex === extraRow.index ? 1 : 0
                                border.color: Theme.stroke
                            }
                            Text {
                                id: extraName
                                x: 12
                                anchors.verticalCenter: parent.verticalCenter
                                width: Math.min(implicitWidth, parent.width * 0.5)
                                text: extraRow.modelData.name
                                color: Theme.text
                                font.pixelSize: Theme.fontBody
                                elide: Text.ElideRight
                            }
                            Text {
                                anchors { left: extraName.right; leftMargin: 10; right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                                text: extraRow.modelData.detail
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                                elide: Text.ElideMiddle
                            }
                            MouseArea {
                                id: extraMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: root.openExtra(extraRow.index)
                            }
                        }
                    }
                }
            }
            }

            Text {
                anchors.centerIn: parent
                visible: grid.count === 0 && root.extras.length === 0
                text: qsTr("No apps found")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
            }
        }

        // --- search results (while searching, in place of the grid) ---
        Item {
            id: searchView
            visible: root.searching
            anchors { top: searchBox.bottom; bottom: footer.top; left: parent.left; right: parent.right
                topMargin: 12; leftMargin: 24; rightMargin: 24; bottomMargin: 12 }

            // The tabs: what the results are.
            Row {
                id: filterTabs
                spacing: 4
                Repeater {
                    model: [{ id: "all", text: qsTr("All") }, { id: "apps", text: qsTr("Apps") },
                        { id: "settings", text: qsTr("Settings") }, { id: "files", text: qsTr("Files") }]
                    delegate: Item {
                        id: tab
                        required property var modelData
                        readonly property bool selected: root.searchFilter === modelData.id
                        width: tabText.implicitWidth + 20
                        height: 30
                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.radius
                            color: tabMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
                            opacity: tabMouse.containsMouse ? 1 : 0
                        }
                        Text {
                            id: tabText
                            anchors.centerIn: parent
                            text: tab.modelData.text
                            color: tab.selected ? Theme.text : Theme.textSecondary
                            font.pixelSize: Theme.fontBody
                            font.weight: tab.selected ? Font.DemiBold : Font.Normal
                        }
                        Rectangle {
                            visible: tab.selected
                            anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter }
                            width: 16
                            height: 3
                            radius: 1.5
                            color: Theme.accent
                        }
                        MouseArea {
                            id: tabMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: root.searchFilter = tab.modelData.id
                        }
                    }
                }
            }

            // At the left the results, the best match first and larger.
            ListView {
                id: resultList
                anchors { top: filterTabs.bottom; bottom: parent.bottom; left: parent.left; topMargin: 12 }
                width: Math.round(parent.width * 0.5)
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.results
                currentIndex: root.resultIndex
                onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
                delegate: Column {
                    id: row
                    required property var modelData
                    required property int index
                    readonly property bool best: index === 0
                    // A title above the best match and above each kind after it.
                    readonly property bool titled: index <= 1 || root.results[index - 1].kind !== modelData.kind
                    width: resultList.width

                    Text {
                        visible: row.titled
                        leftPadding: 8
                        topPadding: row.index === 0 ? 0 : 10
                        bottomPadding: 4
                        text: row.best ? qsTr("Best match")
                            : row.modelData.kind === "app" ? qsTr("Apps")
                            : row.modelData.kind === "setting" ? qsTr("Settings")
                            : row.modelData.kind === "file" ? qsTr("Files") : qsTr("Search the web")
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.DemiBold
                    }
                    Item {
                        width: parent.width
                        height: row.best ? 56 : 36

                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.radius
                            color: rowMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
                            opacity: rowMouse.containsMouse || root.resultIndex === row.index ? 1 : 0
                        }
                        // The selection's accent bar, like Windows 11's lists.
                        Rectangle {
                            visible: root.resultIndex === row.index
                            anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                            width: 3
                            height: 16
                            radius: 1.5
                            color: Theme.accent
                        }
                        Image {
                            id: rowIcon
                            x: 12
                            anchors.verticalCenter: parent.verticalCenter
                            width: row.best ? 32 : 20
                            height: width
                            source: Theme.icons + encodeURIComponent(row.modelData.iconName || "application-x-executable")
                            sourceSize: Qt.size(width, height)
                        }
                        Column {
                            anchors { left: rowIcon.right; leftMargin: 12; right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                            Text {
                                width: parent.width
                                text: row.modelData.name
                                color: Theme.text
                                font.pixelSize: Theme.fontBody
                                elide: Text.ElideRight
                            }
                            Text {
                                visible: row.best || row.modelData.kind === "web"
                                width: parent.width
                                text: row.modelData.detail
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                                elide: Text.ElideMiddle
                            }
                        }
                        MouseArea {
                            id: rowMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: root.resultIndex === row.index ? root.openResult(row.modelData) : root.resultIndex = row.index
                            onDoubleClicked: root.openResult(row.modelData)
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: resultList
                visible: root.results.length === 0
                text: qsTr("No results")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
            }

            // At the right the selected result: its icon, name, kind and actions.
            Rectangle {
                id: detail
                visible: root.result !== null
                anchors { top: resultList.top; bottom: parent.bottom; left: resultList.right; right: parent.right; leftMargin: 12 }
                radius: Theme.radiusCard
                color: Theme.card
                border.width: 1
                border.color: Theme.cardStroke

                Column {
                    id: detailHead
                    anchors { top: parent.top; left: parent.left; right: parent.right; topMargin: 24 }
                    spacing: 6
                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 56
                        height: 56
                        source: root.result ? Theme.icons + encodeURIComponent(root.result.iconName || "application-x-executable") : ""
                        sourceSize: Qt.size(width, height)
                    }
                    Text {
                        width: parent.width
                        leftPadding: 12
                        rightPadding: 12
                        horizontalAlignment: Text.AlignHCenter
                        text: root.result ? root.result.name : ""
                        color: Theme.text
                        font.pixelSize: Theme.fontSubtitle
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        leftPadding: 12
                        rightPadding: 12
                        horizontalAlignment: Text.AlignHCenter
                        text: root.result ? root.result.detail : ""
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideMiddle
                    }
                }
                Rectangle {
                    id: detailRule
                    anchors { top: detailHead.bottom; left: parent.left; right: parent.right; topMargin: 16; leftMargin: 12; rightMargin: 12 }
                    height: 1
                    color: Theme.divider
                }
                ListView {
                    anchors { top: detailRule.bottom; bottom: parent.bottom; left: parent.left; right: parent.right; margins: 8 }
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.resultActions(root.result)
                    delegate: Item {
                        id: act
                        required property var modelData
                        width: ListView.view.width
                        height: 34
                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.radius
                            color: actMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
                            opacity: actMouse.containsMouse ? 1 : 0
                        }
                        Image {
                            id: actIcon
                            x: 10
                            anchors.verticalCenter: parent.verticalCenter
                            width: 16
                            height: 16
                            source: act.modelData.icon ? Theme.icons + encodeURIComponent(act.modelData.icon) : ""
                            sourceSize: Qt.size(width, height)
                        }
                        Text {
                            anchors { left: actIcon.right; leftMargin: 12; right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                            text: act.modelData.text
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            elide: Text.ElideRight
                        }
                        MouseArea {
                            id: actMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: act.modelData.action()
                        }
                    }
                }
            }
        }

        // A failed shortcut must not appear to have succeeded.
        Rectangle {
            visible: root.shortcutError !== ""
            z: 3
            anchors { left: parent.left; right: parent.right; bottom: footer.top
                leftMargin: 24; rightMargin: 24; bottomMargin: 8 }
            height: shortcutErrorText.implicitHeight + 24
            radius: Theme.radius
            color: Theme.popup
            border.width: 1
            border.color: Theme.stroke

            Text {
                id: shortcutErrorText
                width: parent.width - 24
                anchors.centerIn: parent
                text: root.shortcutError
                color: Theme.text
                font.pixelSize: Theme.fontBody
                wrapMode: Text.WordWrap
            }
        }

        // --- footer: user ---
        Rectangle {
            id: footer
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            visible: Theme.startMenuFooter
            height: visible ? 60 : 0
            color: "transparent"

            Rectangle {
                anchors { left: parent.left; right: parent.right; top: parent.top }
                height: 1
                color: Theme.stroke
            }

            Row {
                anchors { left: parent.left; leftMargin: 32; verticalCenter: parent.verticalCenter }
                spacing: 12

                Rectangle {
                    width: 32
                    height: 32
                    radius: 16
                    color: Theme.accent
                    Text {
                        anchors.centerIn: parent
                        text: Shell.userInitial
                        color: "white"
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.DemiBold
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: Shell.userName
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                }
            }

            // Power, where Windows has its button: sleep, sign out (closes
            // Vela, not the computer), restart, shut down.
            Item {
                id: powerButton
                anchors { right: parent.right; rightMargin: 24; verticalCenter: parent.verticalCenter }
                width: 40
                height: 40
                property bool menuOpen: false
                onMenuOpenChanged: if (menuOpen) root.updatePending = Shell.updatePending()

                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radius
                    color: powerMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
                    opacity: powerMouse.containsMouse || powerButton.menuOpen ? 1 : 0
                    Behavior on opacity {
                        NumberAnimation { duration: Theme.fast; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
                    }
                }

                // The power symbol: a circle open at the top and a bar.
                Shape {
                    anchors.centerIn: parent
                    width: 20
                    height: 20
                    preferredRendererType: Shape.CurveRenderer
                    scale: powerMouse.pressed ? 0.9 : 1
                    Behavior on scale {
                        NumberAnimation { duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
                    }
                    ShapePath {
                        strokeColor: Theme.text
                        strokeWidth: 1.5
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        PathAngleArc { centerX: 10; centerY: 11; radiusX: 6.5; radiusY: 6.5; startAngle: -50; sweepAngle: 280 }
                    }
                    ShapePath {
                        strokeColor: Theme.text
                        strokeWidth: 1.5
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        startX: 10; startY: 3
                        PathLine { x: 10; y: 10 }
                    }
                }

                MouseArea {
                    id: powerMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: powerButton.menuOpen = !powerButton.menuOpen
                }

                Rectangle {
                    id: powerMenu
                    anchors { right: parent.right; bottom: parent.top; bottomMargin: 8 }
                    width: 200
                    height: powerEntries.implicitHeight + 8
                    radius: Theme.radiusOverlay
                    color: Theme.popup
                    border.width: 1
                    border.color: Theme.stroke
                    opacity: powerButton.menuOpen ? 1 : 0
                    visible: opacity > 0
                    Behavior on opacity {
                        NumberAnimation { duration: Theme.fast; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
                    }

                    Column {
                        id: powerEntries
                        x: 4
                        y: 4
                        width: parent.width - 8

                        PowerEntry {
                            icon: "system-suspend"
                            label: qsTr("Sleep")
                            visible: root.canSuspend
                            onActivated: { root.close(); Shell.suspend() }
                        }
                        PowerEntry {
                            label: qsTr("Sign out")
                            onActivated: Shell.logout()

                            // An open door and an arrow going out.
                            Shape {
                                anchors.fill: parent
                                preferredRendererType: Shape.CurveRenderer
                                ShapePath {
                                    strokeColor: Theme.text
                                    strokeWidth: 1.2
                                    fillColor: "transparent"
                                    capStyle: ShapePath.RoundCap
                                    joinStyle: ShapePath.RoundJoin
                                    startX: 8.5; startY: 2.5
                                    PathLine { x: 3; y: 2.5 }
                                    PathLine { x: 3; y: 13.5 }
                                    PathLine { x: 8.5; y: 13.5 }
                                }
                                ShapePath {
                                    strokeColor: Theme.text
                                    strokeWidth: 1.2
                                    fillColor: "transparent"
                                    capStyle: ShapePath.RoundCap
                                    joinStyle: ShapePath.RoundJoin
                                    startX: 6.5; startY: 8
                                    PathLine { x: 13.5; y: 8 }
                                    PathMove { x: 10.8; y: 5.3 }
                                    PathLine { x: 13.5; y: 8 }
                                    PathLine { x: 10.8; y: 10.7 }
                                }
                            }
                        }
                        PowerEntry {
                            icon: "view-refresh"
                            label: qsTr("Restart shell")
                            onActivated: { root.close(); Shell.restartShell() }
                        }
                        // A Vela update waiting, like Windows' "Update and
                        // shut down": installed first, then off (only if it worked).
                        PowerEntry {
                            visible: root.updatePending
                            icon: "system-software-update"
                            label: qsTr("Update and restart")
                            onActivated: { root.close(); Shell.updateThen(true) }
                        }
                        PowerEntry {
                            visible: root.updatePending
                            icon: "system-software-update"
                            label: qsTr("Update and shut down")
                            onActivated: { root.close(); Shell.updateThen(false) }
                        }
                        PowerEntry {
                            icon: "system-reboot"
                            label: qsTr("Restart")
                            onActivated: Shell.reboot()
                        }
                        PowerEntry {
                            icon: "system-shutdown"
                            label: qsTr("Shut down")
                            onActivated: Shell.powerOff()
                        }
                    }
                }
            }
        }
    }
}
