// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Shapes
import QtQuick.Effects
import QtQuick.Layouts
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
    property bool powerFocused: false // Tab: the power button, Enter opens its menu
    property string shortcutError: ""
    // Asked of logind only once: can the computer suspend?
    readonly property bool canSuspend: Shell.canSuspend()
    readonly property bool canRebootToWindows: Config.restartToWindows && Shell.canRebootToWindows()

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
    // startmenu-layout=1: Windows 7's Start menu (programs and the search at the
    // left, places and Shut down at the right) in place of the grid.
    readonly property bool seven: Theme.startMenuSeven
    property bool sevenAll: false // "All Programs": every app, not only the pinned ones
    property bool sevenPowerOpen: false
    readonly property var sevenPrograms: {
        Apps.catalogRevision
        if (searching) {
            return results
        }
        const out = []
        if (sevenAll) {
            void Apps.query
            for (let i = 0; i < Apps.count; ++i) {
                const id = Apps.idAt(i)
                const e = Apps.entry(id)
                out.push({ kind: "app", id: id, name: e.name, iconName: e.iconName })
            }
        } else {
            const seen = {}
            for (const p of pinned) {
                seen[p.id] = true
                out.push({ kind: "app", id: p.id, name: p.name, iconName: p.iconName })
            }
            // A compact list: the rest of the first apps, up to twelve.
            void Apps.query
            for (let i = 0; out.length < 12 && i < Apps.count; ++i) {
                const id = Apps.idAt(i)
                if (seen[id]) {
                    continue
                }
                const e = Apps.entry(id)
                out.push({ kind: "app", id: id, name: e.name, iconName: e.iconName })
            }
        }
        return out
    }
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
    // Where the arrows are while searching: the search box, the results, the
    // filter tabs above them or the selected result's actions beside them.
    property string searchArea: "results"
    property int actionIndex: 0
    onResultIndexChanged: actionIndex = 0
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
        if (searchArea === "box") {
            if (delta > 0) searchArea = "filters"
        } else if (searchArea === "filters") {
            searchArea = delta > 0 ? "results" : "box"
        } else if (searchArea === "actions") {
            const count = resultActions(result).length
            actionIndex = Math.max(0, Math.min(count - 1, actionIndex + delta))
        } else if (delta < 0 && resultIndex === 0) {
            searchArea = "filters"
        } else if (results.length > 0) {
            resultIndex = Math.max(0, Math.min(results.length - 1, resultIndex + delta))
        }
    }
    // Left or right while searching; false: the search box's caret takes it.
    function moveSearchSide(right) {
        if (searchArea === "box") {
            return false
        }
        if (searchArea === "filters") {
            cycleFilter(right ? 1 : -1)
            return true
        }
        if (searchArea === "actions") {
            if (!right) searchArea = "results"
            return true
        }
        if (right && search.cursorPosition === search.text.length && resultActions(result).length > 0) {
            searchArea = "actions"
            actionIndex = 0
            return true
        }
        return false
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
        // placeStartMenu: flush at the corner for a theme with Start at the left
        const left = Theme.taskbarAlignLeft ? 0 : Config.taskbarAlignment === "left" ? 12 : (Screen.width - root.width) / 2
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

    // A menu of its own, so the compositor blurs behind it like every other.
    function openPowerMenu() {
        const p = screenPoint(powerButton, powerButton.width - 200, -8) // flush with the button's right edge
        const updates = Shell.updatePending() ? [
            // Like Windows' "Update and shut down": installed first, then off (only if it worked).
            { text: qsTr("Update and restart"), icon: "system-software-update", action: () => { root.close(); Shell.updateThen(true) } },
            { text: qsTr("Update and shut down"), icon: "system-software-update", action: () => { root.close(); Shell.updateThen(false) } }
        ] : []
        const windows = canRebootToWindows ? [
            { text: qsTr("Restart to &Windows"), icon: "system-reboot", action: () => { root.close(); Shell.rebootToWindows() } }
        ] : []
        Menus.open([
            ...(canSuspend ? [{ text: qsTr("S&leep"), icon: "system-suspend", action: () => { root.close(); Shell.suspend() } }] : []),
            { text: qsTr("Sign &out"), icon: "system-log-out", action: () => Shell.logout() },
            { text: qsTr("Restart s&hell"), icon: "view-refresh", action: () => { root.close(); Shell.restartShell() } },
            ...updates,
            { text: qsTr("&Restart"), icon: "system-reboot", action: () => Shell.reboot() },
            ...windows,
            { text: qsTr("Sh&ut down"), icon: "system-shutdown", action: () => Shell.powerOff() }
        ], p.x, p.y, { above: true, minWidth: 200, screen: root.screen ? root.screen.name : Shell.primaryScreen })
    }

    // An entry of the Windows 7 power menu.
    Gradient {
        id: powerHighlight
        GradientStop { position: 0; color: "#F3F9FF" }
        GradientStop { position: 1; color: "#CFE3F8" }
    }
    component PowerEntry: Item {
        id: entry
        property string icon // from the theme; empty: the symbol drawn inside
        property string label
        property color textColor: Theme.text
        property bool light: false // Windows 7's light menu: dark glyphs, blue highlight
        default property alias glyph: glyphSlot.data
        signal activated()
        width: parent ? parent.width : 0
        height: 36

        Rectangle {
            anchors.fill: parent
            radius: entry.light ? 3 : Theme.radius
            color: entryMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
            border.width: entry.light ? 1 : 0
            border.color: "#A5C8EC"
            gradient: entry.light ? powerHighlight : null
            opacity: entryMouse.containsMouse ? 1 : 0
        }
        Image {
            x: 10
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            visible: entry.icon !== ""
            source: entry.icon !== "" ? (entry.light ? Theme.iconsOnLight : Theme.icons) + entry.icon : ""
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
            color: entry.textColor
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
        sevenPowerOpen = false
        sevenAll = false
        Apps.query = ""
        extraIndex = -1
        searchFilter = "all"
        search.text = ""
        grid.currentIndex = 0
        grid.keyNav = false
        selectedPinnedIndex = -1
        powerFocused = false
        searchArea = "results"
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
        // Nothing selected: Enter in the empty search box opens nothing.
        if (!searching && !grid.keyNav && search.text.length === 0) {
            return
        }
        if (searching) {
            if (searchArea === "filters") {
                searchArea = "results"
            } else if (searchArea === "actions") {
                const entry = resultActions(result)[actionIndex]
                if (entry) entry.action()
            } else {
                openResult(result)
            }
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
        // The pinned tiles are the grid's header: it starts at originY.
        const row = Math.floor(index / Theme.startMenuColumns)
        const top = grid.originY + row * Theme.startMenuCellHeight
        if (row === 0) grid.contentY = grid.originY
        else if (grid.contentY > top) grid.contentY = top
        else if (grid.contentY + grid.height < top + Theme.startMenuCellHeight)
            grid.contentY = top + Theme.startMenuCellHeight - grid.height
    }

    function selectApp(index) {
        selectedPinnedIndex = -1
        grid.currentIndex = index
        grid.positionViewAtIndex(index, GridView.Contain)
    }

    // Back to the search box: nothing selected, the list at its top.
    function selectSearch() {
        grid.keyNav = false
        selectedPinnedIndex = -1
        powerFocused = false
        grid.currentIndex = 0
        grid.contentY = grid.originY
    }

    // The arrows, everywhere: search box, pinned tiles, all apps, power button.
    // Up and down go by column and leave a section at its first and last row,
    // left and right go tile by tile through both sections.
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
        const columns = Theme.startMenuColumns
        const selectLast = column => {
            if (appCount > 0) {
                const lastRow = Math.floor((appCount - 1) / columns) * columns
                selectApp(Math.min(lastRow + column, appCount - 1))
            } else {
                const lastRow = Math.floor((pinCount - 1) / columns) * columns
                selectPinned(Math.min(lastRow + column, pinCount - 1))
            }
        }

        if (powerFocused) {
            if (direction === "up" || direction === "left") {
                powerFocused = false
                grid.keyNav = true
                selectLast(direction === "up" ? 0 : columns - 1)
            }
            return
        }
        // No tile is highlighted before keyboard navigation starts, except
        // search results. The first arrow should select, not skip, tile zero.
        if (!grid.keyNav && search.text.length === 0) {
            if (direction === "up") return
            grid.keyNav = true
            if (pinCount > 0) selectPinned(0)
            else selectApp(0)
            return
        }

        grid.keyNav = true
        if (selectedPinnedIndex >= 0 && pinCount > 0) {
            const index = selectedPinnedIndex
            const lastRow = Math.floor((pinCount - 1) / columns)
            const row = Math.floor(index / columns)
            if (direction === "left") {
                if (index > 0) selectPinned(index - 1)
            } else if (direction === "right") {
                if (index + 1 < pinCount) selectPinned(index + 1)
                else if (appCount > 0) selectApp(0)
                else { selectedPinnedIndex = -1; powerFocused = true }
            } else if (direction === "down") {
                if (row < lastRow) selectPinned(Math.min(index + columns, pinCount - 1))
                else if (appCount > 0) selectApp(Math.min(index % columns, appCount - 1))
                else { selectedPinnedIndex = -1; powerFocused = true }
            } else if (direction === "up") {
                if (row > 0) selectPinned(index - columns)
                else selectSearch()
            }
        } else if (appCount > 0) {
            const index = Math.max(0, grid.currentIndex)
            const lastRow = Math.floor((appCount - 1) / columns)
            const row = Math.floor(index / columns)
            const lastPinnedRow = pinCount > 0 ? Math.floor((pinCount - 1) / columns) * columns : 0
            if (direction === "left") {
                if (index > 0) selectApp(index - 1)
                else if (pinCount > 0) selectPinned(pinCount - 1)
            } else if (direction === "right") {
                if (index + 1 < appCount) selectApp(index + 1)
                else powerFocused = true
            } else if (direction === "down") {
                if (row < lastRow) selectApp(Math.min(index + columns, appCount - 1))
                else powerFocused = true
            } else if (direction === "up") {
                if (row > 0) selectApp(index - columns)
                else if (pinCount > 0) selectPinned(Math.min(lastPinnedRow + index % columns, pinCount - 1))
                else selectSearch()
            }
        }
    }

    // Page Up/Down: three rows. Home/End: the first and the last tile.
    function navigateKey(key) {
        if (key === Qt.Key_Home) {
            grid.keyNav = true
            powerFocused = false
            if (showPinned && pinned.length > 0) selectPinned(0)
            else if (grid.count > 0) selectApp(0)
        } else if (key === Qt.Key_End) {
            grid.keyNav = true
            powerFocused = false
            if (grid.count > 0) selectApp(grid.count - 1)
            else if (showPinned && pinned.length > 0) selectPinned(pinned.length - 1)
        } else {
            for (let i = 0; i < 3; ++i) navigate(key === Qt.Key_PageDown ? "down" : "up")
        }
    }

    // The menu key, Shift+F10: the selected tile's menu, in the middle of the panel.
    function openSelectionMenu() {
        const x = panel.width / 2, y = panel.height / 2
        if (selectedPinnedIndex >= 0 && selectedPinnedIndex < pinned.length) {
            openAppMenu(panel, x, y, pinned[selectedPinnedIndex].id, true)
        } else if (grid.keyNav && grid.currentItem) {
            openAppMenu(panel, x, y, grid.currentItem.appId, false)
        } else {
            openPanelMenu(panel, x, y)
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
        } else if (active && visible) {
            search.forceActiveFocus() // the arrows work again after a menu
        }
    }

    // The menu of the panel's empty parts.
    function openPanelMenu(item, x, y) {
        const p = screenPoint(item, x, y)
        Menus.open([
            { text: qsTr("&All apps"), checkable: true, checked: Theme.startMenuAllApps,
              action: () => Style.setConfigFlag("startmenu-all-apps", !Theme.startMenuAllApps) },
            { separator: true },
            { text: qsTr("&Start settings"), icon: "settings-configure",
              action: () => { root.close(); System.trigger("start") } }
        ], p.x, p.y, { screen: root.screen ? root.screen.name : Shell.primaryScreen })
    }

    ParallelAnimation {
        id: openAnimation
        NumberAnimation {
            target: panel; property: "offset"; to: 0
            duration: Theme.animated(190) // Windows 11's: about 170 ms, slowing down
            easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate
        }
        NumberAnimation {
            target: panel; property: "opacity"; to: 1
            duration: Theme.animated(60) // it does not fade in on Windows, only comes up
            easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate
        }
    }

    // Windows 11's, measured at 30 fps: down in about 170 ms, speeding up, without a fade.
    SequentialAnimation {
        id: closeAnimation
        NumberAnimation {
            target: panel; property: "offset"; to: panel.slide
            duration: Theme.animated(170)
            easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.accelerate
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
        // Windows 7's menu sits on the taskbar, at the corner: no gap at the Start button's side.
        readonly property real restX: root.seven && !side ? 0 : side && Config.taskbarLeft ? 22 : 10
        // How far it rises while opening (falls, at the top, comes in from
        // the side next to a vertical taskbar). The part sticking out is cut
        // at the taskbar's edge.
        // Windows 11's, measured: it rises about 370 px, out of the taskbar's edge.
        readonly property real slide: side ? (Config.taskbarLeft ? -140 : 140) : (Config.taskbarTop ? -1 : 1) * Math.min(370, height)
        property real offset: 0
        x: restX + (side ? offset : 0)
        y: restY + (side ? 0 : offset)
        width: root.width - (side ? 32 : root.seven ? 10 : 20)
        height: root.height - (side ? 20 : root.seven ? restY : 32) // 10 above, 22 below: 12 of gap from the taskbar (Windows 7: none)
        radius: Theme.startMenuRadius
        // Windows 7: the Aero glass colour. Else the theme's tint, made more
        // solid by Settings > Taskbar > Start menu background.
        color: root.seven ? "#E69EB6D0" : Qt.rgba(Theme.startMenuSurface.r, Theme.startMenuSurface.g, Theme.startMenuSurface.b,
            Theme.startMenuSurface.a + (1 - Theme.startMenuSurface.a) * Config.startSolid / 100)
        border.width: 1
        border.color: root.seven ? "#B3101820" : Theme.stroke // Aero: a dark outer line...

        Rectangle { // ...and a white one just inside it
            visible: root.seven
            anchors { fill: parent; margins: 1 }
            radius: parent.radius - 1
            color: "transparent"
            border.width: 1
            border.color: "#80FFFFFF"
        }
        Rectangle { // Windows 7's darkening of the glass: light at the edges, dark in the middle (AeroThemePlasma's SevenStart)
            visible: root.seven
            anchors { fill: parent; margins: 2 }
            topLeftRadius: parent.radius - 2
            topRightRadius: parent.radius - 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#26000000" }
                GradientStop { position: 0.11; color: "#53000000" }
                GradientStop { position: 0.2775; color: "#82000000" }
                GradientStop { position: 0.445; color: "#99000000" }
                GradientStop { position: 0.51; color: "#99000000" }
                GradientStop { position: 0.84; color: "#4F000000" }
                GradientStop { position: 1.0; color: "#00000000" }
            }
        }

        // Behind everything: a click on the empty parts keeps the arrows working,
        // a right click opens the panel's menu.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onPressed: search.forceActiveFocus()
            onClicked: mouse => {
                if (mouse.button === Qt.RightButton) {
                    root.openPanelMenu(this, mouse.x, mouse.y)
                }
            }
        }

        // --- search ---
        Rectangle {
            id: searchBox
            // x/y, not anchors: Windows 7's layout puts it at the bottom left.
            z: 5
            // Windows 7: centred in the light strip at the bottom of the white column.
            x: root.seven ? sevenRow.x + sevenLeft.x + 10 : 24
            y: root.seven ? sevenRow.y + sevenLeft.y + sevenLeft.height - 2 - (sevenSearchStrip.height + height) / 2 : 24
            width: root.seven ? sevenLeft.width - 20 : parent.width - 48
            height: root.seven ? 30 : 38
            radius: root.seven ? 3 : Theme.startMenuSearchRadius >= 0 ? Theme.startMenuSearchRadius : height / 2
            color: root.seven ? "transparent" : Theme.control
            border.width: root.seven ? 0 : search.activeFocus && (root.searching
                ? root.searchArea === "box" || root.searchArea === "results"
                : !grid.keyNav && !root.powerFocused) ? 1 : 0
            border.color: Theme.accent

            BorderImage { // Windows 7's field (AeroThemePlasma's lineedit.svg)
                visible: root.seven
                anchors.fill: parent
                source: search.activeFocus ? "qrc:/seven/search-focus.png" : "qrc:/seven/search.png"
                border { left: 6; top: 6; right: 6; bottom: 6 }
            }

            Shape { // the magnifier, at the right end (Windows 7's)
                visible: root.seven
                x: parent.width - 22
                y: (parent.height - 14) / 2
                width: 14
                height: 14
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: "#4A78AE"
                    strokeWidth: 1.6
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    PathAngleArc { centerX: 5.5; centerY: 5.5; radiusX: 4.2; radiusY: 4.2; startAngle: 0; sweepAngle: 360 }
                    PathMove { x: 8.6; y: 8.6 }
                    PathLine { x: 12.5; y: 12.5 }
                }
            }
            Text {
                anchors { left: parent.left; leftMargin: root.seven ? 8 : 18; verticalCenter: parent.verticalCenter }
                visible: search.text.length === 0
                text: root.seven ? qsTr("Search programs and files") : qsTr("Search apps, files and settings")
                color: root.seven ? "#7C8794" : Theme.textSecondary
                font.pixelSize: root.seven ? 15 : Theme.fontBody
                font.italic: root.seven
            }

            MenuTextField {
                id: search
                color: root.seven ? "#1B2B40" : Theme.text
                anchors { left: parent.left; right: parent.right; leftMargin: root.seven ? 8 : 18; rightMargin: root.seven ? 26 : 18; verticalCenter: parent.verticalCenter }
                mapToScreen: (x, y) => root.screenPoint(search, x, y)

                onTextChanged: {
                    Apps.query = text
                    FileSearch.query = text
                    root.extraIndex = -1
                    root.powerFocused = false
                    root.searchArea = "results"
                    grid.currentIndex = 0
                    root.selectedPinnedIndex = -1
                    grid.keyNav = false
                    grid.positionViewAtBeginning()
                }

                // The keyboard stays in the search; the arrows move the grid.
                // Esc first comes back to the search box, then closes.
                Keys.onEscapePressed: {
                    if (root.searching && root.searchArea !== "box" && root.searchArea !== "results") root.searchArea = "box"
                    else if (!root.searching && (grid.keyNav || root.powerFocused)) root.selectSearch()
                    else root.close()
                }
                Keys.onPressed: event => {
                    if (root.searching || text.length > 0) return
                    if (event.key === Qt.Key_Home || event.key === Qt.Key_End
                        || event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown) {
                        root.navigateKey(event.key)
                        event.accepted = true
                    } else if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
                        root.openSelectionMenu()
                        event.accepted = true
                    }
                }
                Keys.onReturnPressed: root.powerFocused ? root.openPowerMenu() : root.launchSelection()
                Keys.onEnterPressed: root.powerFocused ? root.openPowerMenu() : root.launchSelection()
                Keys.onDownPressed: root.searching ? root.moveResult(1) : root.navigate("down")
                Keys.onUpPressed: root.searching ? root.moveResult(-1) : root.navigate("up")
                Keys.onTabPressed: root.searching ? root.cycleFilter(1) : root.powerFocused = !root.powerFocused
                Keys.onBacktabPressed: root.searching ? root.cycleFilter(-1) : root.powerFocused = !root.powerFocused
                // A click in the field is editing again. The default (passive) policy: the
                // text input grabs the mouse itself, which cancels the other policies.
                TapHandler {
                    onTapped: { grid.keyNav = false; root.extraIndex = -1 }
                }
                // Left and Right edit text if the search isn't empty.
                Keys.onLeftPressed: event => {
                    if (root.searching) { event.accepted = root.moveSearchSide(false); return }
                    if (text.length > 0) { event.accepted = false; return }
                    root.navigate("left")
                }
                Keys.onRightPressed: event => {
                    if (root.searching) { event.accepted = root.moveSearchSide(true); return }
                    if (text.length > 0) { event.accepted = false; return }
                    root.navigate("right")
                }
            }
        }

        Text {
            id: sectionTitle
            visible: !root.searching && !root.seven
            anchors { top: searchBox.bottom; left: parent.left; topMargin: 20; leftMargin: 32 }
            text: search.text.length > 0 ? qsTr("Results") : root.showPinned ? qsTr("Pinned") : qsTr("All apps")
            color: Theme.text
            font.pixelSize: Theme.fontBody
            font.weight: Font.DemiBold
        }

        // --- app grid ---
        GridView {
            id: grid
            visible: !root.searching && !root.seven
            anchors {
                top: sectionTitle.bottom; bottom: footer.top
                left: parent.left; right: parent.right
                topMargin: 10; leftMargin: 20; rightMargin: 20; bottomMargin: 8
            }
            clip: true
            model: Theme.startMenuAllApps || !root.showPinned ? Apps : null
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
                    visible: root.showPinned && Theme.startMenuAllApps
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
                visible: grid.count === 0 && root.extras.length === 0 && !root.showPinned
                text: qsTr("No apps found")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
            }
        }

        // --- search results (while searching, in place of the grid) ---
        Item {
            id: searchView
            visible: root.searching && !root.seven
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
                            opacity: tabMouse.containsMouse || (tab.selected && root.searchArea === "filters") ? 1 : 0
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
                        required property int index
                        width: ListView.view.width
                        height: 34
                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.radius
                            color: actMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
                            opacity: actMouse.containsMouse || (root.searchArea === "actions" && act.index === root.actionIndex) ? 1 : 0
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
            visible: Theme.startMenuFooter && !root.seven
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
                    Image {
                        id: userPhoto
                        anchors.fill: parent
                        source: Shell.userPicture
                        sourceSize: Qt.size(64, 64)
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        visible: status === Image.Ready
                        layer.enabled: true
                        layer.effect: MultiEffect { maskEnabled: true; maskSource: userPhotoMask }
                    }
                    Rectangle { // the round cut of the picture
                        id: userPhotoMask
                        width: 32; height: 32; radius: 16
                        layer.enabled: true
                        visible: false
                    }
                    Text {
                        visible: userPhoto.status !== Image.Ready
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

                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radius
                    color: powerMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
                    opacity: powerMouse.containsMouse || root.powerFocused ? 1 : 0
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
                    onClicked: root.openPowerMenu()
                }
            }
        }

        // --- Windows 7's Start menu: the programs on white at the left, the
        // places and Shut down on the glass at the right ---
        Item {
            id: sevenMenu
            visible: root.seven
            anchors.fill: parent

            RowLayout {
                id: sevenRow
                anchors { fill: parent; margins: 6 }
                spacing: 4

                BorderImage { // the left column: white, the search in a light strip at its bottom (AeroThemePlasma's background.svg)
                    id: sevenLeft
                    Layout.preferredWidth: 312
                    Layout.fillHeight: true
                    source: "qrc:/seven/panel.png"
                    border { left: 6; top: 6; right: 6; bottom: 6 }

                    Rectangle {
                        id: sevenSearchStrip
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 2 }
                        height: 52
                        radius: 3
                        color: "#F1F5FB"
                        Rectangle { // its top edge
                            anchors { left: parent.left; right: parent.right; top: parent.top }
                            height: 4
                            gradient: Gradient {
                                GradientStop { position: 0; color: "#CCD9EA" }
                                GradientStop { position: 1; color: "#F0F4FA" }
                            }
                        }
                    }

                    ColumnLayout {
                        anchors { fill: parent; margins: 4; bottomMargin: sevenSearchStrip.height + 4 }
                        spacing: 4

                        ListView {
                            id: sevenList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: root.sevenPrograms
                            boundsBehavior: Flickable.StopAtBounds
                            delegate: Item {
                                id: row
                                required property var modelData
                                width: sevenList.width
                                height: 41
                                BorderImage {
                                    anchors.fill: parent
                                    visible: rowMouse.containsMouse
                                    source: "qrc:/seven/item-hover.png"
                                    border { left: 2; top: 2; right: 2; bottom: 2 }
                                }
                                Image {
                                    x: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 32
                                    height: 32
                                    source: Theme.icons + encodeURIComponent(row.modelData.iconName ?? "")
                                    sourceSize: Qt.size(width, height)
                                    smooth: true
                                }
                                Text {
                                    anchors { left: parent.left; leftMargin: 54; right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                                    text: row.modelData.name
                                    color: "#3C3C3C"
                                    elide: Text.ElideRight
                                    font.pixelSize: 15
                                }
                                MouseArea {
                                    id: rowMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    onClicked: mouse => {
                                        if (mouse.button === Qt.RightButton && row.modelData.kind === "app") {
                                            root.openAppMenu(row, mouse.x, mouse.y, row.modelData.id, false)
                                        } else if (mouse.button === Qt.LeftButton) {
                                            root.openResult(row.modelData)
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle { // the line above "All Programs"
                            visible: !root.searching
                            Layout.fillWidth: true
                            Layout.leftMargin: 6
                            Layout.rightMargin: 6
                            Layout.preferredHeight: 1
                            color: "#D6E5F5"
                        }

                        Item { // "All Programs" / "Back"
                            id: allPrograms
                            visible: !root.searching
                            Layout.fillWidth: true
                            Layout.preferredHeight: visible ? 34 : 0
                            BorderImage {
                                anchors.fill: parent
                                visible: allMouse.containsMouse
                                source: "qrc:/seven/item-hover.png"
                                border { left: 2; top: 2; right: 2; bottom: 2 }
                            }
                            Text {
                                x: 12
                                anchors.verticalCenter: parent.verticalCenter
                                text: root.sevenAll ? "◂  " + qsTr("Back") : "▸  " + qsTr("All Programs")
                                color: "#3C3C3C"
                                font.pixelSize: 15
                            }
                            MouseArea {
                                id: allMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: root.sevenAll = !root.sevenAll
                            }
                        }

                    }
                }

                // The right column, straight on the panel's dark glass: the user, the places, Shut down.
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        id: sevenRight
                        anchors { fill: parent; margins: 6 }
                        spacing: 0

                        // The user's picture in Windows 7's frame, at the top of the column.
                        Item {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: -2
                            Layout.preferredWidth: 72
                            Layout.preferredHeight: 72
                            Item {
                            anchors.fill: parent
                            // The user's picture (AccountsService), else a portrait drawn.
                            Rectangle {
                                id: portrait
                                readonly property string user: System.homeDirectory().split("/").pop()
                                anchors { fill: parent; margins: 8 }
                                color: "#8AA9CC"
                                clip: true
                                Image {
                                    id: portraitImage
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    source: Shell.userPicture
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                }
                                Item { // no picture: a head and shoulders
                                    anchors.fill: parent
                                    visible: portraitImage.status !== Image.Ready
                                    Rectangle { x: 15; y: 8; width: 18; height: 18; radius: 9; color: "#E6F0FA" }
                                    Rectangle { x: 7; y: 28; width: 34; height: 24; radius: 12; color: "#E6F0FA" }
                                }
                            }
                            BorderImage { // Windows 7's frame over it (AeroThemePlasma's user.png)
                                anchors.fill: parent
                                source: "qrc:/seven/avatar-frame.png"
                                border { left: 11; top: 11; right: 11; bottom: 11 }
                            }
                            }
                        }
                        Text {
                            Layout.topMargin: 6
                            Layout.leftMargin: 6
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: Shell.userName
                            color: "white"
                            font.pixelSize: 15
                        }
                        ColumnLayout {
                            Layout.topMargin: 8
                            Layout.fillWidth: true
                            spacing: 0
                            Repeater {
                                // dir: a folder of the home; page: a Settings page; "": the home
                                model: [
                                    { text: qsTr("Documents"), dir: "Documents" },
                                    { text: qsTr("Pictures"), dir: "Pictures" },
                                    { text: qsTr("Music"), dir: "Music" },
                                    { rule: true },
                                    { text: qsTr("Games"), dir: "Games" },
                                    { text: qsTr("Computer"), dir: "" },
                                    { rule: true },
                                    { text: qsTr("Control Panel"), page: "" },
                                    { text: qsTr("Devices and Printers"), page: "bluetooth" },
                                    { text: qsTr("Default Programs"), page: "default-apps" },
                                    { text: qsTr("Help and Support"), page: "about" }
                                ]
                                delegate: Item {
                                    id: place
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: modelData.rule ? 14 : 32
                                    Rectangle {
                                        visible: place.modelData.rule === true
                                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 6; rightMargin: 6 }
                                        height: 1
                                        color: "#4DFFFFFF"
                                    }
                                    BorderImage { // AeroThemePlasma's sidebaritem.svg
                                        anchors.fill: parent
                                        visible: !place.modelData.rule && placeMouse.containsMouse
                                        source: "qrc:/seven/place-hover.png"
                                        border { left: 6; top: 5; right: 5; bottom: 5 }
                                    }
                                    Text {
                                        anchors { left: parent.left; right: parent.right; leftMargin: 8; rightMargin: 4; verticalCenter: parent.verticalCenter }
                                        visible: !place.modelData.rule
                                        elide: Text.ElideRight
                                        text: place.modelData.text ?? ""
                                        color: "white"
                                        font.pixelSize: 15
                                    }
                                    MouseArea {
                                        id: placeMouse
                                        anchors.fill: parent
                                        enabled: !place.modelData.rule
                                        hoverEnabled: true
                                        onClicked: {
                                            if (place.modelData.page !== undefined) {
                                                System.openSettingsPage(place.modelData.page)
                                            } else {
                                                const home = System.homeDirectory()
                                                System.openUrl(place.modelData.dir !== "" ? home + "/" + place.modelData.dir : home)
                                            }
                                            root.close()
                                        }
                                    }
                                }
                            }
                        }

                        Item { Layout.fillHeight: true } // the places above, Shut down below

                        // Shut down, with the arrow for the rest.
                        // The two buttons are AeroThemePlasma's startmenu-buttons.svg (normal/hover/pressed).
                        Item {
                            id: sevenShut
                            readonly property bool hot: shutMouse.containsMouse || arrowMouse.containsMouse || root.sevenPowerOpen
                            Layout.preferredWidth: Math.min(sevenRight.width - 4, 138)
                            Layout.preferredHeight: 30
                            Layout.bottomMargin: 7 // level with the search field
                            BorderImage {
                                anchors { left: parent.left; top: parent.top; bottom: parent.bottom; right: arrowPart.left }
                                source: shutMouse.pressed ? "qrc:/seven/shutdown-pressed.png" : sevenShut.hot ? "qrc:/seven/shutdown-hover.png" : "qrc:/seven/shutdown.png"
                                border { left: 3; top: 3; right: 3; bottom: 3 }
                            }
                            Text {
                                anchors { horizontalCenter: parent.horizontalCenter; horizontalCenterOffset: -arrowPart.width / 2; verticalCenter: parent.verticalCenter; verticalCenterOffset: -1 }
                                text: qsTr("Shut down")
                                color: "white"
                                font.pixelSize: 15
                            }
                            MouseArea {
                                id: shutMouse
                                anchors { left: parent.left; top: parent.top; bottom: parent.bottom; right: arrowPart.left }
                                hoverEnabled: true
                                onClicked: Shell.powerOff()
                            }
                            BorderImage {
                                id: arrowPart
                                anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                                width: 30
                                source: arrowMouse.pressed ? "qrc:/seven/arrow-pressed.png" : sevenShut.hot ? "qrc:/seven/arrow-hover.png" : "qrc:/seven/arrow.png"
                                border { left: 3; top: 3; right: 3; bottom: 3 }
                                Text {
                                    anchors.centerIn: parent
                                    text: "▸"
                                    color: "white"
                                    font.pixelSize: 12
                                }
                                MouseArea {
                                    id: arrowMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        root.sevenPowerOpen = !root.sevenPowerOpen
                                        if (root.sevenPowerOpen) root.updatePending = Shell.updatePending()
                                    }
                                }
                            }

                            Rectangle { // the menu of the arrow, above the button
                                visible: root.sevenPowerOpen
                                anchors { right: parent.right; bottom: parent.top; bottomMargin: 4 }
                                width: 220
                                height: sevenPowerEntries.implicitHeight + 8
                                radius: 3
                                color: "#F2F2F2"
                                border.width: 1
                                border.color: "#979797"
                                z: 10
                                Column {
                                    id: sevenPowerEntries
                                    x: 4
                                    y: 4
                                    width: parent.width - 8
                                    PowerEntry {
                                        light: true
                                        textColor: "#1E1E1E"
                                        icon: "system-suspend"
                                        label: qsTr("Sleep")
                                        visible: root.canSuspend
                                        onActivated: { root.close(); Shell.suspend() }
                                    }
                                    PowerEntry {
                                        light: true
                                        textColor: "#1E1E1E"
                                        icon: "view-refresh"
                                        label: qsTr("Restart shell")
                                        onActivated: { root.close(); Shell.restartShell() }
                                    }
                                    PowerEntry {
                                        light: true
                                        textColor: "#1E1E1E"
                                        visible: root.updatePending
                                        icon: "system-software-update"
                                        label: qsTr("Update and restart")
                                        onActivated: { root.close(); Shell.updateThen(true) }
                                    }
                                    PowerEntry {
                                        light: true
                                        textColor: "#1E1E1E"
                                        visible: root.updatePending
                                        icon: "system-software-update"
                                        label: qsTr("Update and shut down")
                                        onActivated: { root.close(); Shell.updateThen(false) }
                                    }
                                    PowerEntry {
                                        light: true
                                        textColor: "#1E1E1E"
                                        icon: "system-reboot"
                                        label: qsTr("Restart")
                                        onActivated: Shell.reboot()
                                    }
                                    PowerEntry {
                                        light: true
                                        textColor: "#1E1E1E"
                                        icon: "system-reboot"
                                        label: qsTr("Restart to Windows")
                                        visible: root.canRebootToWindows
                                        onActivated: { root.close(); Shell.rebootToWindows() }
                                    }
                                    PowerEntry {
                                        light: true
                                        textColor: "#1E1E1E"
                                        icon: "system-log-out"
                                        label: qsTr("Sign out")
                                        onActivated: Shell.logout()
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
