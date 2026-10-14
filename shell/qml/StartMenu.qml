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
    width: 660
    // On short outputs it gets shorter: the search at the top must stay
    // visible.
    height: Math.min(732, Screen.height - Theme.taskbarHeight - 12)
    color: "transparent"

    property bool closing: false
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

    // Vela Settings pages searchable from here; texts and keys follow the
    // Settings window's own search index (settings/qml/Main.qml).
    readonly property var settingsIndex: [
        { page: "display", text: qsTr("Display"), keys: qsTr("resolution scale refresh rate hz orientation monitor multiple displays arrangement") },
        { page: "sound", text: qsTr("Sound"), keys: qsTr("volume speakers headphones microphone output input sound") },
        { page: "volume-mixer", text: qsTr("Volume mixer"), keys: qsTr("app volume apps sound output input per app mixer reset") },
        { page: "notifications", text: qsTr("Notifications"), keys: qsTr("do not disturb alerts") },
        { page: "power", text: qsTr("Power"), keys: qsTr("screen off sleep idle battery energy saver lock") },
        { page: "about", text: qsTr("About"), keys: qsTr("specifications processor ram memory pc name rename version kernel graphics card") },
        { page: "bluetooth", text: qsTr("Bluetooth"), keys: qsTr("devices headphones mouse keyboard pair add") },
        { page: "network", text: qsTr("Network & internet"), keys: qsTr("wifi wi-fi ethernet cable ip dns connection") },
        { page: "personalization", text: qsTr("Personalization"), keys: qsTr("theme appearance") },
        { page: "background", text: qsTr("Background"), keys: qsTr("picture desktop photo wallpaper") },
        { page: "colors", text: qsTr("Colors"), keys: qsTr("accent color theme dark light mode") },
        { page: "taskbar", text: qsTr("Taskbar"), keys: qsTr("taskbar alignment left center end task") },
        { page: "apps", text: qsTr("Apps"), keys: qsTr("programs applications") },
        { page: "installed-apps", text: qsTr("Installed apps"), keys: qsTr("uninstall remove programs") },
        { page: "default-apps", text: qsTr("Default apps"), keys: qsTr("browser mail player open with default") },
        { page: "datetime", text: qsTr("Date & time"), keys: qsTr("clock time zone sync ntp") },
        { page: "time-language", text: qsTr("Language"), keys: qsTr("language english italian italiano inglese translation") },
        { page: "keyboard", text: qsTr("Keyboard"), keys: qsTr("layout language input typing key repeat") },
        { page: "night-light", text: qsTr("Night light"), keys: qsTr("night warm colors blue evening schedule sunset") },
        { page: "accessibility", text: qsTr("Accessibility"), keys: qsTr("low vision color blindness") }
    ]

    // The extra results of a search, in screen order: files, then settings.
    readonly property var extras: {
        const q = search.text.trim().toLowerCase()
        if (q.length === 0) {
            return []
        }
        const out = []
        for (const f of FileSearch.results) {
            out.push({ kind: "file", name: f.name, detail: f.path.replace(System.homeDirectory(), "~"), path: f.path })
        }
        let n = 0
        for (const e of settingsIndex) {
            if (n < 4 && (e.text.toLowerCase().indexOf(q) >= 0 || e.keys.toLowerCase().indexOf(q) >= 0)) {
                out.push({ kind: "setting", name: e.text, detail: qsTr("Settings"), page: e.page })
                n++
            }
        }
        return out
    }
    // The selected extra result; -1: the selection is in the app grid.
    property int extraIndex: -1

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
    function moveExtra(delta) {
        const next = extraIndex + delta
        if (extraIndex >= 0 && next >= extras.length) {
            extraIndex = -1
            grid.currentIndex = 0
        } else if (extraIndex < 0 && delta < 0 && extras.length > 0 && grid.currentIndex < 6) {
            extraIndex = extras.length - 1
        } else if (extraIndex >= 0) {
            extraIndex = Math.max(0, next)
        } else {
            return false
        }
        return true
    }
    onExtrasChanged: {
        // No app matches: the keyboard goes to the first extra.
        extraIndex = extras.length > 0 && grid.count === 0 ? 0 : -1
    }

    // The Start menu is above the taskbar, centered or at the left: from its
    // coordinates to the output's, for right-click menus.
    function screenPoint(item, x, y) {
        const p = item.mapToItem(null, x, y)
        const left = Config.taskbarAlignment === "left" ? 12 : (Screen.width - root.width) / 2 // placeStartMenu
        return Qt.point(left + p.x, Screen.height - Theme.taskbarHeight - root.height + p.y)
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
        const desktop = { text: qsTr("Add to &desktop"), icon: "user-desktop", action: () => { FileActions.createLinks([Apps.desktopFile(id)], System.desktopDirectory()); root.close() } }
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
                { text: qsTr("&More"), children: [taskbar, desktop, folder] },
                uninstall)
        }
        return entries
    }

    function openAppMenu(item, x, y, id, pinnedTile) {
        const p = screenPoint(item, x, y)
        Menus.open(appMenu(id, pinnedTile), p.x, p.y)
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
        Effects.setBlur(root, [Qt.rect(panel.x, Math.max(0, panel.y), panel.width, Math.max(0, Math.min(panel.height, root.height - panel.y)))])
    }
    Connections {
        target: panel
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
        search.text = ""
        grid.currentIndex = 0
        grid.keyNav = false
        grid.positionViewAtBeginning()
        panel.opacity = 0
        panel.y = panel.restY + panel.slide
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

    function launchCurrent() {
        if (extraIndex >= 0) {
            openExtra(extraIndex)
        } else {
            launch(grid.currentIndex)
        }
    }

    function launch(row) {
        if (row >= 0 && row < grid.count && Apps.launch(row)) {
            close()
        }
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
            target: panel; property: "y"; to: panel.restY
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
                target: panel; property: "y"; to: panel.restY + panel.slide
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

    // A soft shadow around the panel (not below: it's acrylic).
    PanelShadow {
        target: panel
        opacity: panel.opacity
        blur: 40
        offset: Qt.vector2d(0, 8)
        color: Qt.rgba(0, 0, 0, 0.5)
    }

    Rectangle {
        id: panel

        readonly property real restY: 10
        // How far it rises while opening. The part sticking out below the
        // window is cut at the taskbar's edge.
        readonly property real slide: 140
        x: 10
        y: restY
        width: root.width - 20
        height: root.height - 32 // 10 above, 22 below: 12 of gap from the taskbar
        radius: Theme.radiusOverlay
        color: Theme.surface
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
                    grid.positionViewAtBeginning()
                }

                // The keyboard stays in the search; the arrows move the grid.
                Keys.onEscapePressed: root.close()
                Keys.onReturnPressed: root.launchCurrent()
                Keys.onEnterPressed: root.launchCurrent()
                Keys.onDownPressed: { grid.keyNav = true; if (!root.moveExtra(1)) grid.moveCurrentIndexDown() }
                Keys.onUpPressed: { grid.keyNav = true; if (!root.moveExtra(-1)) grid.moveCurrentIndexUp() }
                Keys.onTabPressed: { grid.keyNav = true; grid.moveCurrentIndexRight() }
                Keys.onBacktabPressed: { grid.keyNav = true; grid.moveCurrentIndexLeft() }
                // Left and Right move the caret while there is text to edit.
                Keys.onLeftPressed: event => {
                    if (text.length > 0) { event.accepted = false; return }
                    grid.keyNav = true
                    grid.moveCurrentIndexLeft()
                }
                Keys.onRightPressed: event => {
                    if (text.length > 0) { event.accepted = false; return }
                    grid.keyNav = true
                    grid.moveCurrentIndexRight()
                }
            }
        }

        Text {
            id: sectionTitle
            anchors { top: searchBox.bottom; left: parent.left; topMargin: 20; leftMargin: 32 }
            text: search.text.length > 0 ? qsTr("Results") : root.showPinned ? qsTr("Pinned") : qsTr("All apps")
            color: Theme.text
            font.pixelSize: Theme.fontBody
            font.weight: Font.DemiBold
        }

        // --- app grid ---
        GridView {
            id: grid
            anchors {
                top: sectionTitle.bottom; bottom: footer.top
                left: parent.left; right: parent.right
                topMargin: 10; leftMargin: 20; rightMargin: 20; bottomMargin: 8
            }
            clip: true
            model: Apps
            cellWidth: Math.floor(width / 6)
            cellHeight: 96
            keyNavigationWraps: true
            boundsBehavior: Flickable.StopAtBounds
            highlightFollowsCurrentItem: false
            // The selection is drawn once the arrows have been used (always while searching).
            property bool keyNav: false

            // With the arrows the selection can leave the view: we follow it.
            onCurrentIndexChanged: positionViewAtIndex(currentIndex, GridView.Contain)

            delegate: AppTile {
                id: tile
                required property string appId
                width: grid.cellWidth
                height: grid.cellHeight
                current: root.extraIndex < 0 && index === grid.currentIndex && (search.text.length > 0 || grid.keyNav)
                onActivated: root.launch(index)
                onContextRequested: (x, y) => root.openAppMenu(tile, x, y, tile.appId, false)
            }

            // Above all apps, those pinned to Start.
            header: Item {
                width: grid.width
                height: root.showPinned ? pinnedGrid.height + allTitle.height + 24 : extraColumn.height
                visible: root.showPinned || root.extras.length > 0

                // Files and Settings matches, above the apps.
                Column {
                    id: extraColumn
                    width: parent.width
                    visible: !root.showPinned
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
                                topPadding: index === 0 ? 0 : 8
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

                Grid {
                    id: pinnedGrid
                    visible: root.showPinned
                    columns: 6

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

            Text {
                anchors.centerIn: parent
                visible: grid.count === 0 && root.extras.length === 0
                text: qsTr("No apps found")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
            }
        }

        // --- footer: user ---
        Rectangle {
            id: footer
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 60
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
