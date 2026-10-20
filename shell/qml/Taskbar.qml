// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Shapes
import Vela.Controls

// The taskbar: Start button, pinned and open apps in the center, clock on the
// right.
Window {
    id: root
    objectName: "taskbar"
    // One per output (ScreenWindows): the main one also has the notification
    // area and system icons, the others the clock, like Windows.
    property bool primary: false
    readonly property string screenName: screen ? screen.name : ""

    // It stays invisible until main.cpp has turned it into a layer-shell
    // panel. The compositor decides the width (anchored to the sides).
    visible: false
    // The compositor decides the length (anchored to the sides).
    readonly property bool vertical: Config.taskbarVertical
    // taskbar/style=islands: three floating surfaces instead of one bar. Along
    // a vertical taskbar it falls back to the classic one.
    readonly property bool islands: Config.taskbarStyle === "islands" && !vertical
    // The dock style (Config.dock) is taller: a floating pill with big icons;
    // the tray and clock live in the top panel (TopBar.qml).
    readonly property bool dock: Config.dock
    readonly property int barHeight: dock ? 68 : Theme.taskbarBarHeight
    // A theme's taskbar-margin / taskbar-radius: the bar floats inside its window.
    readonly property bool floating: !islands && !dock && (Theme.taskbarMargin > 0 || Theme.taskbarRadius > 0)
    width: vertical ? Theme.taskbarHeight : 1280
    height: vertical ? 720 : barHeight
    color: "transparent"

    // The apps pinned at first start (.desktop ids; those not installed are
    // skipped). Afterwards the user's choices apply: pinned and unpinned from
    // the menus, reordered by dragging the buttons.
    readonly property list<string> pinnedIds: [
        "vela-files.desktop",
        "org.kde.konsole.desktop",
        "firefox.desktop",
        "org.mozilla.firefox.desktop",
        "chromium.desktop",
        "org.kde.kate.desktop",
        "vela-settings.desktop"
    ]
    // Only at first start: afterwards the saved ones apply (pinned and
    // unpinned from the menus).
    Component.onCompleted: {
        if (!Tasks.pinsSaved) {
            Tasks.pinnedIds = pinnedIds
        }
        updateBlur()
        if (!held) hideDelay.start()
    }

    // Auto-hide: it slides down (a negative bottom margin, ShellController)
    // leaving 2 px at the edge of the screen, which bring it back when the
    // pointer touches them. The compositor keeps reserving its space for the
    // panels above it and lets the windows use it (taskbar-autohide).
    readonly property bool held: !Config.taskbarAutoHide || barHover.hovered || Shell.startMenuOpen
        || Menus.isOpen || Menus.quickSettingsOpen || Menus.notificationCenterOpen || Menus.taskViewOpen
        || Menus.trayOverflow !== null || Menus.weather !== null || Menus.preview !== null
    property bool concealed: false
    onHeldChanged: {
        if (held) {
            hideDelay.stop()
            concealed = false
        } else {
            hideDelay.restart()
        }
    }
    Timer {
        id: hideDelay
        interval: 500
        onTriggered: root.concealed = !root.held
    }
    property real slide: concealed ? 1 : 0
    Behavior on slide { NumberAnimation { duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate } }
    readonly property int bottomMargin: Math.round(slide * (barHeight - 2))
    onBottomMarginChanged: Shell.setBottomMargin(root, bottomMargin)
    Item {
        anchors.fill: parent
        HoverHandler { id: barHover }
    }

    // The shell's offsets (Start menu, flyouts) follow the real height.
    Binding { target: Theme; property: "taskbarHeight"; value: root.barHeight; when: root.primary && !root.vertical }

    // The whole taskbar above the blurred background (acrylic); with islands,
    // each island has its own blur and the gaps between them let clicks
    // through to the desktop.
    function updateBlur() {
        if (dock) {
            const r = Qt.rect(dockPill.x, dockPill.y, dockPill.width, dockPill.height)
            Effects.setBlur(root, [r])
            Effects.setInputRegion(root, [r])
            return
        }
        if (!islands) {
            Effects.setBlur(root, [Qt.rect(0, 0, width, height)])
            Effects.setInputRegion(root, [])
            return
        }
        const rects = [leftIsland, rightIsland]
        if (middleIsland.visible) rects.splice(1, 0, middleIsland)
        const blur = rects.map(i => Qt.rect(i.x, i.y, i.width, i.height))
        Effects.setBlur(root, blur)
        // The show desktop strip beyond the right island stays clickable.
        Effects.setInputRegion(root, Config.showDesktopCorner ? blur.concat([Qt.rect(width - 8, 0, 8, height)]) : blur)
    }
    onWidthChanged: updateBlur()
    onHeightChanged: updateBlur()
    onIslandsChanged: updateBlur()
    onDockChanged: updateBlur()

    // A button's window previews (TaskbarPreview.qml).
    Timer {
        id: previewDelay
        property var task: null
        interval: 450
        onTriggered: {
            if (!task || !task.hovered || task.windowCount === 0) return
            const p = task.mapToItem(null, task.width / 2, 0)
            Menus.preview = { key: task.key, appIds: Tasks.appIds(task.index), center: p.x, centerY: p.y + task.height / 2, screen: root.screenName }
        }
    }

    // --- right-click menus (docs/renderer.md §14.4-14.6) ---

    // The taskbar is at the bottom of the output (or the top, taskbar/position):
    // from its coordinates to the output's.
    function screenPoint(item, x, y) {
        const p = item.mapToItem(null, x, y)
        if (vertical) {
            return Qt.point(Config.taskbarLeft ? p.x : Screen.width - root.width + p.x, p.y)
        }
        return Qt.point(p.x, Config.taskbarTop ? p.y : Screen.height - root.height + p.y)
    }
    // A menu next to the taskbar from the point `p` (screenPoint): rising from
    // the bottom one, hanging from the top one, at the side of a vertical one
    // (at the height of p, towards the screen).
    function edgeMenu(entries, p, options) {
        if (vertical) {
            const x = Config.taskbarLeft ? root.width + 4 : Screen.width - root.width - 4
            const side = Object.assign({}, options || {})
            side.centered = false
            root.openMenu(entries, x, p.y, side)
        } else {
            root.openMenu(entries, p.x, menuBottom, Object.assign({ above: !Config.taskbarTop }, options || {}))
        }
    }
    // Taskbar menus rise from its top edge, or at the top they hang from its
    // bottom one (`above: false`).
    readonly property real menuBottom: Config.taskbarTop ? root.height + 4 : Screen.height - root.height - 4

    function openAbove(entries, item, centered, options) {
        const p = screenPoint(item, centered ? item.width / 2 : 0, 0)
        root.edgeMenu(entries, p, Object.assign({ centered: centered }, options || {}))
    }
    // Taskbar menus open on its output; the panels it opens (Start, quick
    // settings...) go there too.
    function openMenu(entries, x, y, options) {
        Menus.panelScreen = root.screenName
        Menus.open(entries, x, y, Object.assign({ screen: root.screenName }, options || {}))
    }
    function fromHere() { Menus.panelScreen = root.screenName }

    // A button's jump list: pinned and recent files, the app's tasks, then the
    // app, pin/unpin, close.
    function jumpList(task) {
        const id = task.desktopId
        const entries = []
        if (id !== "") {
            const fileEntry = (file, pinned) => ({
                text: file.name.replace(/&/g, "&&"),
                icon: file.icon,
                action: () => Apps.launchWithFile(id, file.url),
                pin: { pinned: pinned, toggle: () => Jumps.setPinned(id, file.url, !pinned) },
                context: [
                    { text: qsTr("&Open"), icon: task.iconName, action: () => Apps.launchWithFile(id, file.url) },
                    pinned
                        ? { text: qsTr("&Remove from this list"), icon: "window-unpin", action: () => Jumps.setPinned(id, file.url, false) }
                        : { text: qsTr("&Pin to this list"), icon: "window-pin", action: () => Jumps.setPinned(id, file.url, true) },
                    { text: qsTr("Remove from &list"), icon: "list-remove", action: () => Jumps.forget(id, file.url) }
                ]
            })
            const pinned = Jumps.pinned(id)
            if (pinned.length > 0) {
                entries.push({ header: qsTr("Pinned") })
                pinned.forEach(f => entries.push(fileEntry(f, true)))
            }
            const recent = Jumps.recent(id, 10)
            if (recent.length > 0) {
                entries.push({ header: qsTr("Recent") })
                recent.forEach(f => entries.push(fileEntry(f, false)))
            }
            const actions = Apps.actions(id)
            if (actions.length > 0) {
                entries.push({ header: qsTr("Tasks") })
                actions.forEach(a => entries.push({ text: a.name.replace(/&/g, "&&"), icon: a.icon, action: () => Apps.launchAction(id, a.id) }))
            }
            if (entries.length > 0) {
                entries.push({ separator: true })
            }
            entries.push({ text: task.name.replace(/&/g, "&&"), icon: task.iconName, action: () => Apps.launchId(id) })
            entries.push(Tasks.isPinned(id)
                ? { text: qsTr("Unpin from taskbar"), icon: "window-unpin", action: () => Tasks.unpin(id) }
                : { text: qsTr("Pin to taskbar"), icon: "window-pin", action: () => Tasks.pin(id) })
        }
        if (task.windowCount > 0 && Config.endTask) {
            entries.push({ text: qsTr("End task"), icon: "process-stop", action: () => Tasks.endTask(task.index) })
        }
        if (task.windowCount > 0) {
            entries.push({
                text: task.windowCount > 1 ? qsTr("Close all windows") : qsTr("Close window"),
                icon: "window-close",
                action: () => Tasks.closeWindows(task.index)
            })
        }
        return entries
    }

    // Win+X: the Start button menu.
    function winXEntries() {
        const item = (text, icon, name) => ({ text: text, icon: icon, enabled: System.available(name), action: () => System.trigger(name) })
        const entries = [item(qsTr("Insta&lled apps"), "system-software-install", "installed-apps")]
        if (System.isLaptop()) {
            entries.push(item(qsTr("Mobility &Center"), "computer-laptop", "mobility"))
        }
        entries.push(
            item(qsTr("P&ower options"), "preferences-system-power-management", "power"),
            item(qsTr("E&vent viewer"), "text-x-log", "events"),
            item(qsTr("S&ystem"), "computer", "system"),
            item(qsTr("Device &manager"), "preferences-devices-tree", "devices"),
            item(qsTr("Net&work connections"), "preferences-system-network", "network"),
            item(qsTr("Dis&k management"), "drive-harddisk", "disks"),
            item(qsTr("Computer mana&gement"), "computer", "computer"),
            { separator: true },
            item(qsTr("&Terminal"), "utilities-terminal", "terminal"),
            item(qsTr("Terminal (&Admin)"), "utilities-terminal", "terminal-admin"),
            { separator: true },
            item(qsTr("Task manage&r"), "utilities-system-monitor", "task-manager"),
            item(qsTr("Setti&ngs"), "preferences-system", "settings"),
            item(qsTr("File &Explorer"), "system-file-manager", "files"),
            { text: qsTr("&Search"), icon: "search", action: () => { root.fromHere(); if (!Shell.startMenuOpen) Shell.toggleStartMenu() } },
            { text: qsTr("&Run"), icon: "system-run", action: () => { root.fromHere(); Shell.runRequested() } },
            { separator: true },
            { text: qsTr("Sh&ut down or sign out"), icon: "system-shutdown", children: [
                { text: qsTr("Sign &out"), icon: "system-log-out", action: () => Shell.logout() },
                { text: qsTr("S&leep"), icon: "system-suspend", enabled: Shell.canSuspend(), action: () => Shell.suspend() },
                { text: qsTr("Sh&ut down"), icon: "system-shutdown", action: () => Shell.powerOff() },
                { text: qsTr("&Restart"), icon: "system-reboot", action: () => Shell.reboot() }
            ] },
            { text: qsTr("&Desktop"), icon: "user-desktop", action: () => Tasks.toggleDesktop() }
        )
        return entries
    }

    Connections {
        target: Shell
        enabled: root.primary
        function onWinXRequested() {
            root.openWinX(true)
        }
        function onShowDesktopRequested() {
            Tasks.toggleDesktop()
        }
    }
    function openWinX(keyboard) {
        const p = screenPoint(islands ? startButtonIsland : startButton, 0, 0)
        root.edgeMenu(winXEntries(), p, { keyboard: keyboard })
    }

    // The menu of a notification area icon (the app decides it). Those in the
    // flyout open their own (TrayOverflow.qml).
    Connections {
        target: Tray
        enabled: root.primary && !Menus.trayOverflow && !root.dock
        function onMenuReady(row, anchorX, entries) {
            root.edgeMenu(Menus.trayEntries(row, entries), Qt.point(anchorX, Menus.trayAnchorY), { centered: true })
        }
    }

    // The Start and Task View buttons exist twice: in the centered row of the
    // classic taskbar, and in the left island.
    component StartBtn: TaskbarButton {
        active: Shell.startMenuOpen
        onClicked: { root.fromHere(); Shell.toggleStartMenu() }
        onRightClicked: root.openWinX(false)
        StartGlyph { anchors.centerIn: parent }
    }
    // Task View: two overlapping windows, like the Windows 11 icon.
    component TaskViewBtn: TaskbarButton {
        visible: Config.taskView
        tooltip: qsTr("Task view")
        onClicked: { root.fromHere(); Shell.taskViewRequested() }
        Item {
            anchors.centerIn: parent
            width: 20
            height: 18
            Rectangle {
                x: 0
                y: 4
                width: 13
                height: 13
                radius: 2.5
                color: "transparent"
                border.width: 1.5
                border.color: Theme.text
            }
            Rectangle {
                x: 6
                y: 0
                width: 13
                height: 13
                radius: 2.5
                color: Theme.accentFill
                opacity: 0.9
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: root.islands || root.dock || root.floating ? "transparent" : Theme.taskbar

        Rectangle {
            visible: root.floating
            anchors.fill: parent
            anchors.margins: Theme.taskbarMargin
            radius: Theme.taskbarRadius
            color: Theme.taskbar
            border.width: 1
            border.color: Theme.stroke
        }

        // Empty space: Task Manager and taskbar settings.
        MouseArea {
            id: emptyArea
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onClicked: mouse => {
                const p = root.screenPoint(emptyArea, mouse.x, mouse.y)
                root.edgeMenu([
                    { text: qsTr("Task &manager"), icon: "utilities-system-monitor", enabled: System.available("task-manager"), action: () => System.trigger("task-manager") },
                    { text: qsTr("Taskbar &settings"), icon: "configure", enabled: System.available("taskbar-settings"), action: () => System.trigger("taskbar-settings") }
                ], p)
            }
        }

        // A thin line of light on the edge towards the screen, like glass.
        Rectangle {
            visible: !root.islands && !root.dock && !root.floating
            // x/y, not anchors: anchors switched to undefined at run time
            // keep their old edge when the position changes live.
            x: root.vertical && Config.taskbarLeft ? parent.width - width : 0
            y: !root.vertical && Config.taskbarTop ? parent.height - height : 0
            width: root.vertical ? 1 : parent.width
            height: root.vertical ? parent.height : 1
            color: Theme.stroke
            // Sliding changes only the layer-shell margin, which takes effect
            // at the next commit: this barely visible change makes every step
            // render (and so commit), otherwise it jumps or lags.
            opacity: 1 - 0.02 * root.slide
        }
    }

    // The three surfaces of the islands style: Start at the left, the apps in
    // the middle, tray and clock at the right. 8 px from the sides, 12 px
    // corners, a hairline.
    component Island: Rectangle {
        visible: root.islands
        y: 4
        height: parent.height - 8
        radius: Theme.radiusIsland
        color: Theme.taskbar
        border.width: 1
        border.color: Theme.stroke
        onXChanged: root.updateBlur()
        onWidthChanged: root.updateBlur()
        onVisibleChanged: root.updateBlur()
    }
    Island {
        id: leftIsland
        x: 8
        width: leftRow.x + leftRow.width + 6 - x
    }
    Island {
        id: middleIsland
        visible: root.islands && buttons.width > 0
        x: buttons.x - 6
        width: buttons.width + 12
    }
    Island {
        id: rightIsland
        x: overflowButton.x - 6
        width: root.width - 8 - x
    }
    Row {
        id: leftRow
        visible: root.islands
        x: 8 + 6 + weather.width + (weather.width > 0 ? 4 : 0)
        y: Math.round((parent.height - height) / 2)
        StartBtn { id: startButtonIsland }
        TaskViewBtn { }
    }

    // The dock: one rounded pill under the buttons.
    Rectangle {
        id: dockPill
        visible: root.dock
        x: buttons.x - 12
        y: 4
        width: buttons.width + 24
        height: 56
        radius: 18
        color: Theme.taskbar
        border.width: 1
        border.color: Theme.stroke
        onXChanged: root.updateBlur()
        onWidthChanged: root.updateBlur()
        onVisibleChanged: root.updateBlur()
    }

    Grid {
        id: buttons
        // A row (a column along a vertical taskbar), centered by hand instead
        // of with anchors: so when an app opens or closes, it slides to the
        // new center instead of jumping. Or at the left (the top, if
        // vertical), like earlier versions of Windows.
        columns: root.vertical ? 1 : 1000
        x: root.vertical ? Math.round((parent.width - width) / 2)
            : root.dock ? Math.round((parent.width - width) / 2)
            : root.islands ? (Config.taskbarAlignment === "left" ? leftIsland.x + leftIsland.width + 14
                : Math.max(leftIsland.x + leftIsland.width + 14, Math.round((parent.width - width) / 2)))
            : Config.taskbarAlignment === "left" ? 12 + weather.width + (weather.width > 0 ? 8 : 0)
              : Math.round((parent.width - width) / 2)
        y: root.vertical ? (Config.taskbarAlignment === "left" ? 12 : Math.max(12, Math.round((parent.height - height) / 2)))
            : root.dock ? dockPill.y + Math.round((dockPill.height - height) / 2)
            : Math.round((parent.height - height) / 2)
        spacing: Theme.taskbarSpacing

        Behavior on x {
            NumberAnimation { duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        }
        Behavior on y {
            NumberAnimation { duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        }
        // The button of an app just opened comes in growing.
        add: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.normal }
            NumberAnimation {
                property: "scale"; from: 0.5; to: 1; duration: Theme.slow
                easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate
            }
        }
        move: Transition {
            NumberAnimation {
                properties: "x,y"; duration: Theme.normal
                easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate
            }
        }

        StartBtn {
            id: startButton
            visible: !root.islands
        }
        TaskViewBtn {
            visible: Config.taskView && !root.islands
        }

        Repeater {
            id: taskRepeater
            model: Tasks

            TaskbarButton {
                id: task
                draggable: true
                // Dropped further away: it moves by as many places as buttons
                // it passed.
                onDropped: dx => {
                    const to = Math.max(0, Math.min(taskRepeater.count - 1, index + Math.round(dx / ((root.vertical ? height : width) + buttons.spacing))))
                    if (to !== index) Tasks.move(index, to)
                }
                onDraggingChanged: if (dragging) Menus.preview = null
                required property int index
                required property string key
                required property string name
                required property string iconName
                required property int windowCount
                required property bool windowActive
                required property string desktopId

                // Unread count / progress / urgent the app sent (Unity LauncherEntry);
                // without a count of its own, its notifications since it was last seen.
                readonly property var badge: {
                    Badges.revision
                    Notifications.unreadRevision
                    const entry = Badges.entry(desktopId)
                    if (entry.count === 0 && !windowActive) {
                        entry.count = Notifications.unreadFor(desktopId)
                    }
                    return entry
                }
                onWindowActiveChanged: Notifications.markSeen(desktopId)
                tooltip: name
                running: windowCount > 0
                progress: badge.progress
                active: windowActive
                onClicked: {
                    Menus.preview = null
                    Tasks.activate(index)
                }
                onMiddleClicked: Tasks.launchNew(index)
                // The wheel: the volume of this app only, if it's playing
                // sound, with the indicator. A step every notch (120),
                // touchpads add up their small ones.
                property real wheelRest: 0
                onWheeled: delta => {
                    wheelRest += delta
                    while (Math.abs(wheelRest) >= 120) {
                        root.fromHere()
                        if (!Sound.stepTaskVolume(desktopId, wheelRest > 0 ? 1 : -1)) {
                            wheelRest = 0
                            return
                        }
                        wheelRest -= wheelRest > 0 ? 120 : -120
                    }
                }
                // The previews of its windows, with the mouse resting on the
                // button (at once, if another button's are already open).
                onHoveredChanged: {
                    Menus.previewButtonHovered = hovered
                    if (hovered && windowCount > 0) {
                        previewDelay.task = task
                        if (Menus.preview) {
                            previewDelay.triggered()
                        } else {
                            previewDelay.restart()
                        }
                    } else if (previewDelay.task === task) {
                        previewDelay.stop()
                    }
                }
                onRightClicked: shift => {
                    Menus.preview = null
                    if ((shift || Shell.shiftHeld()) && windowCount > 0) {
                        const state = Tasks.windowState(index)
                        root.openAbove(Menus.windowEntries(state.maximized, state.minimized, true,
                            action => Tasks.windowAction(task.index, action)), task, true)
                    } else {
                        root.openAbove(root.jumpList(task), task, true, { minWidth: 256, rebuild: () => root.jumpList(task) })
                    }
                }

                // The compositor flies minimized windows here.
                function reportGeometry() {
                    if (windowCount > 0) {
                        const p = mapToItem(null, 0, 0)
                        Tasks.setButtonGeometry(index, root, Qt.rect(p.x, p.y, width, height))
                    }
                }
                onXChanged: reportGeometry()
                onYChanged: reportGeometry()
                onWindowCountChanged: reportGeometry()
                Connections {
                    target: buttons
                    function onXChanged() { task.reportGeometry() }
                    function onYChanged() { task.reportGeometry() }
                }

                Image {
                    anchors.fill: parent
                    source: Theme.icons + encodeURIComponent(task.iconName)
                    sourceSize: Qt.size(width, height)
                    smooth: true
                    mipmap: true
                }

                // Unread count, top right of the button; red if urgent.
                Rectangle {
                    visible: task.badge.count > 0 || task.badge.urgent
                    x: parent.width - width + (root.vertical ? 1 : 5)
                    y: root.vertical ? -3 : -5
                    width: Math.max(13, countText.implicitWidth + 6)
                    height: 13
                    radius: 6.5
                    color: task.badge.urgent ? "#c42b1c" : Theme.accent
                    Text {
                        id: countText
                        anchors.centerIn: parent
                        visible: task.badge.count > 0
                        text: task.badge.count > 99 ? qsTr("99+") : task.badge.count
                        color: "white"
                        font.pixelSize: 9
                        font.weight: Font.DemiBold
                    }
                }
            }
        }
    }

    // From here on, the end of the taskbar (the bottom, along a vertical one),
    // each item against the next: tray, "^", screen share, microphone, system
    // icons, clock and show desktop. Horizontally they hang from the right
    // edge; vertically from the bottom one, with the same names for what is
    // "before" (left/top) and "after" (right/bottom).

    // Notification area: the apps' icons (Telegram, Discord, Steam...), left
    // of the clock like on Windows. With the overflow on (the default) only
    // those asking for attention stay here, the others are behind the "^".
    Grid {
        id: tray
        visible: root.primary && !root.dock
        columns: root.vertical ? 1 : 1000
        width: root.vertical ? parent.width : implicitWidth
        height: root.vertical ? implicitHeight : parent.height
        x: root.vertical ? 0 : screenShare.x - width - 4
        y: root.vertical ? screenShare.y - height - 4 : 0
        // Read in a binding: it follows the icons coming, going and changing.
        readonly property int hiddenCount: {
            let n = 0
            for (const c of children) {
                if (c.hidden) n++
            }
            return n
        }

        Repeater {
            model: Tray
            delegate: TrayIcon {
                readonly property bool hidden: icon !== "" && Config.trayOverflow && !attention
                width: root.vertical ? tray.width : 32
                height: root.vertical ? 32 : tray.height
                visible: icon !== "" && !hidden
            }
        }
    }

    // The "^" button: the hidden icons in a flyout; it points down while open
    // (towards the screen's inside along a vertical taskbar it points the way
    // the flyout opens).
    Item {
        id: overflowButton
        visible: root.primary && !root.dock && Config.trayOverflow && tray.hiddenCount > 0
        x: root.vertical ? 0 : tray.x - width - 0
        y: root.vertical ? tray.y - height - 0 : 0
        width: root.vertical ? parent.width : (visible ? 32 : 0)
        height: root.vertical ? (visible ? 32 : 0) : parent.height
        readonly property bool open: !!Menus.trayOverflow && Menus.trayOverflow.screen === root.screenName
        onVisibleChanged: if (!visible && open) Menus.trayOverflow = null

        Rectangle {
            anchors { fill: parent; topMargin: root.vertical ? 0 : 8; bottomMargin: root.vertical ? 0 : 8
                      leftMargin: root.vertical ? 8 : 0; rightMargin: root.vertical ? 8 : 0 }
            radius: Theme.radius
            color: overflowMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
            opacity: overflowMouse.containsMouse || overflowButton.open ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
        }
        Shape {
            anchors.centerIn: parent
            width: 10
            height: 6
            rotation: (root.vertical ? (Config.taskbarLeft ? 90 : -90) : 0) + (overflowButton.open ? 180 : 0)
            Behavior on rotation {
                NumberAnimation { duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
            }
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: Theme.text
                strokeWidth: 1.2
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                startX: 1; startY: 5
                PathLine { x: 5; y: 1 }
                PathLine { x: 9; y: 5 }
            }
        }
        MouseArea {
            id: overflowMouse
            anchors.fill: parent
            hoverEnabled: true
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Show hidden icons")
            onClicked: {
                const p = overflowButton.mapToItem(null, overflowButton.width / 2, overflowButton.height / 2)
                Menus.trayOverflow = overflowButton.open ? null : { center: p.x, centerY: p.y, screen: root.screenName }
            }
        }
    }

    // The weather (WeatherWidget.qml): at the left end of a horizontal
    // taskbar, like Windows 11; at the tray end of a vertical one.
    WeatherWidget {
        id: weather
        visible: root.primary && !root.dock && Weather.shown
        vertical: root.vertical
        screenName: root.screenName
        x: root.vertical ? 0 : root.islands ? 14 : 8
        y: root.vertical ? overflowButton.y - height : 0
    }

    // The screen being shared or recorded, like Windows 11: a red dot over a
    // screen, "Your screen is being shared" in the tooltip (the compositor
    // says when, capture_watch.c).
    Item {
        id: screenShare
        visible: root.primary && !root.dock && Shell.screenCaptured
        x: root.vertical ? 0 : microphone.x - width - 0
        y: root.vertical ? microphone.y - height - 0 : 0
        width: root.vertical ? parent.width : (visible ? 32 : 0)
        height: root.vertical ? (visible ? 32 : 0) : parent.height

        function showTooltip() {
            const p = screenShare.mapToItem(null, screenShare.width / 2, screenShare.height / 2)
            let text = Notifications.captureSilenced ? qsTr("Your screen is being shared. Notifications are silenced.") : qsTr("Your screen is being shared")
            if (Shell.captureHiddenWindows > 0) {
                text += "\n" + qsTr("%n window(s) hidden from the capture", "", Shell.captureHiddenWindows)
            }
            Menus.tooltip = { text: text, center: p.x, centerY: p.y, screen: root.screenName, owner: "screenShare" }
        }
        function hideTooltip() {
            shareTooltipDelay.stop()
            if (Menus.tooltip && Menus.tooltip.owner === "screenShare") {
                Menus.tooltip = null
            }
        }
        onVisibleChanged: if (!visible) hideTooltip()

        Timer {
            id: shareTooltipDelay
            interval: 500
            onTriggered: screenShare.showTooltip()
        }
        Rectangle {
            anchors { fill: parent; topMargin: root.vertical ? 0 : 8; bottomMargin: root.vertical ? 0 : 8
                      leftMargin: root.vertical ? 8 : 0; rightMargin: root.vertical ? 8 : 0 }
            radius: Theme.radius
            color: shareMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
            opacity: shareMouse.containsMouse ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
        }
        Image {
            anchors.centerIn: parent
            width: 16
            height: 16
            source: Theme.icons + "video-display"
            sourceSize: Qt.size(width, height)
        }
        Rectangle {
            width: 7
            height: 7
            radius: 3.5
            color: "#e81123"
            x: parent.width / 2 + 2
            y: parent.height / 2 + 1
        }
        MouseArea {
            id: shareMouse
            anchors.fill: parent
            hoverEnabled: true
            onContainsMouseChanged: containsMouse ? shareTooltipDelay.restart() : screenShare.hideTooltip()
            onClicked: screenShare.showTooltip()
        }
    }

    // The microphone in use (docs/audio.md §7.2), like Windows: who listens
    // in the tooltip, a click opens Settings > Sound.
    Item {
        id: microphone
        visible: root.primary && !root.dock && Audio.microphoneUsers.length > 0
        x: root.vertical ? 0 : systemIcons.x - width - 0
        y: root.vertical ? systemIcons.y - height - 0 : 0
        width: root.vertical ? parent.width : (visible ? 32 : 0)
        height: root.vertical ? (visible ? 32 : 0) : parent.height

        readonly property string tooltipText: {
            const users = Audio.microphoneUsers
            if (users.length === 1) {
                return qsTr("%1 is using your microphone").arg(users[0].name)
            }
            return [qsTr("These apps are using your microphone:")].concat(users.map(u => u.name)).join("\n")
        }
        function showTooltip() {
            const p = microphone.mapToItem(null, microphone.width / 2, microphone.height / 2)
            Menus.tooltip = { text: microphone.tooltipText, center: p.x, centerY: p.y, screen: root.screenName, owner: "microphone" }
        }
        function hideTooltip() {
            tooltipDelay.stop()
            if (Menus.tooltip && Menus.tooltip.owner === "microphone") {
                Menus.tooltip = null
            }
        }
        onTooltipTextChanged: if (Menus.tooltip && Menus.tooltip.owner === "microphone") showTooltip()
        onVisibleChanged: if (!visible) hideTooltip()

        Timer {
            id: tooltipDelay
            interval: 500
            onTriggered: microphone.showTooltip()
        }
        Rectangle {
            anchors { fill: parent; topMargin: root.vertical ? 0 : 8; bottomMargin: root.vertical ? 0 : 8
                      leftMargin: root.vertical ? 8 : 0; rightMargin: root.vertical ? 8 : 0 }
            radius: Theme.radius
            color: microphoneMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
            opacity: microphoneMouse.containsMouse ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
        }
        Image {
            anchors.centerIn: parent
            width: 16
            height: 16
            source: Theme.icons + "audio-input-microphone"
            sourceSize: Qt.size(width, height)
        }
        MouseArea {
            id: microphoneMouse
            anchors.fill: parent
            hoverEnabled: true
            onContainsMouseChanged: containsMouse ? tooltipDelay.restart() : microphone.hideTooltip()
            onClicked: {
                microphone.hideTooltip()
                System.trigger("sound-settings")
            }
        }
    }

    // The system icons (network, volume, battery) in a single button: it opens
    // quick settings, like on Windows 11. The wheel over the volume changes
    // it; the right button opens their menus (§14.5).
    Item {
        id: systemIcons
        visible: root.primary && !root.dock
        x: root.vertical ? 0 : clock.x - width - 2
        y: root.vertical ? clock.y - height - 2 : 0
        width: root.vertical ? parent.width : (visible ? iconsRow.width + 16 : 0)
        height: root.vertical ? (visible ? iconsRow.height + 16 : 0) : parent.height

        Rectangle {
            anchors { fill: parent; topMargin: root.vertical ? 0 : 4; bottomMargin: root.vertical ? 0 : 4
                      leftMargin: root.vertical ? 4 : 0; rightMargin: root.vertical ? 4 : 0 }
            radius: Theme.radius
            color: systemMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
            opacity: systemMouse.containsMouse || Menus.quickSettingsOpen ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
        }
        Grid {
            id: iconsRow
            anchors.centerIn: parent
            columns: root.vertical ? 1 : 3
            spacing: 10
            Image {
                id: networkIcon
                width: 16
                height: 16
                source: Theme.icons + encodeURIComponent(Status.networkIconName)
                sourceSize: Qt.size(width, height)
            }
            Image {
                id: volumeIcon
                visible: Audio.available
                width: 16
                height: 16
                source: Theme.icons + encodeURIComponent(Audio.iconName)
                sourceSize: Qt.size(width, height)
            }
            Image {
                visible: Status.batteryPresent
                width: 16
                height: 16
                source: Theme.icons + encodeURIComponent(Status.batteryCharging ? "battery-good-charging" : "battery-good")
                sourceSize: Qt.size(width, height)
            }
        }
        MouseArea {
            id: systemMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: mouse => {
                if (mouse.button === Qt.LeftButton) {
                    root.fromHere()
                    Shell.quickSettingsRequested()
                    return
                }
                // The menu of the icon under the mouse: volume or network.
                const p = root.screenPoint(systemMouse, mouse.x, mouse.y)
                const along = root.vertical ? mouse.y : mouse.x
                const start = root.vertical ? volumeIcon.y + iconsRow.y : volumeIcon.x + iconsRow.x
                const overVolume = volumeIcon.visible && along >= start - 5 && along < start + 16 + 5
                root.edgeMenu(overVolume ? [
                    { text: qsTr("Open volume &mixer"), icon: "audio-volume-high", enabled: Audio.available, action: () => { root.fromHere(); Shell.soundOutputRequested() } },
                    { text: qsTr("&Sound settings"), icon: "preferences-desktop-sound", enabled: System.available("sound-settings"), action: () => System.trigger("sound-settings") }
                ] : [
                    { text: qsTr("&Troubleshoot network problems"), icon: "network-workgroup", enabled: false },
                    { text: qsTr("&Network and Internet settings"), icon: "preferences-system-network", enabled: System.available("network"), action: () => System.trigger("network") }
                ], p)
            }
            // Like the keyboard's volume keys, with the indicator: a step every
            // notch (120), touchpads add up their small ones.
            property real wheelRest: 0
            onWheel: wheel => {
                wheelRest += wheel.angleDelta.y
                while (Math.abs(wheelRest) >= 120) {
                    root.fromHere()
                    Sound.press(wheelRest > 0 ? "up" : "down")
                    wheelRest -= wheelRest > 0 ? 120 : -120
                }
            }
        }
    }

    // Clock: time on top, date below (day and month only along a vertical
    // taskbar, which is narrow).
    Item {
        id: clock
        visible: !root.dock
        x: root.vertical ? 0 : parent.width - width - 12
        y: root.vertical ? showDesktop.y - height - 4 : 0
        readonly property bool badge: Notifications.history.count > 0 || Notifications.silenced
        width: root.vertical ? parent.width : clockColumn.implicitWidth + 16 + (badge ? 22 : 0)
        height: root.vertical ? clockColumn.implicitHeight + 16 + (badge ? 22 : 0) : parent.height

        property date now: new Date()

        Timer {
            interval: 1000
            running: true
            repeat: true
            onTriggered: clock.now = new Date()
        }

        Rectangle {
            anchors { fill: parent; topMargin: root.vertical ? 0 : 4; bottomMargin: root.vertical ? 0 : 4
                      leftMargin: root.vertical ? 4 : 0; rightMargin: root.vertical ? 4 : 0 }
            radius: Theme.radius
            color: Theme.subtleHover
            opacity: clockMouse.containsMouse || Menus.notificationCenterOpen ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
        }
        // Notifications to read: a number next to the clock, like Windows 11
        // (with "Do not disturb" a muted bell); below it along a vertical taskbar.
        Rectangle {
            visible: clock.badge
            x: root.vertical ? (parent.width - width) / 2 : parent.width - width + 2
            y: root.vertical ? parent.height - height - 4 : (parent.height - height) / 2
            width: 18
            height: 18
            radius: 9
            color: Notifications.silenced ? "transparent" : Theme.accent
            Text {
                anchors.centerIn: parent
                visible: !Notifications.silenced
                text: Math.min(Notifications.history.count, 9)
                color: "white"
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }
            Image {
                anchors.centerIn: parent
                visible: Notifications.silenced
                width: 14
                height: 14
                source: Theme.icons + "notifications-disabled"
                sourceSize: Qt.size(width, height)
            }
        }

        Column {
            id: clockColumn
            x: root.vertical ? (parent.width - width) / 2 : 8
            y: root.vertical ? 8 : (parent.height - height) / 2
            spacing: 1

            Text {
                anchors.right: parent.right
                text: Qt.formatTime(clock.now, "HH:mm")
                color: Theme.text
                font.pixelSize: Theme.fontCaption
            }
            Text {
                anchors.right: parent.right
                text: Qt.formatDate(clock.now, root.vertical ? "dd/MM" : "dd/MM/yyyy")
                color: Theme.text
                font.pixelSize: Theme.fontCaption
            }
        }

        MouseArea {
            id: clockMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: mouse => {
                if (mouse.button === Qt.LeftButton) {
                    root.fromHere()
                    Shell.notificationCenterRequested()
                    return
                }
                const p = root.screenPoint(clock, mouse.x, mouse.y)
                root.edgeMenu([
                    { text: qsTr("&Adjust date and time"), icon: "preferences-system-time", enabled: System.available("datetime"), action: () => System.trigger("datetime") },
                    { text: qsTr("&Notification settings"), icon: "preferences-desktop-notification", enabled: System.available("notification-settings"), action: () => System.trigger("notification-settings") }
                ], p)
            }
        }
    }

    // Show desktop, at the far end like Windows 11 (the bottom edge along a
    // vertical taskbar): a click does what Win+D does (Tasks.toggleDesktop);
    // the thin line lights up on hover.
    Item {
        id: showDesktop
        visible: Config.showDesktopCorner && !root.dock // on every taskbar, like upstream
        x: root.vertical ? 0 : parent.width - width
        y: root.vertical ? parent.height - height : 0
        // Invisible at rest and over the clock's margin: it takes no room. Only
        // under the pointer a short line shows, like Windows.
        width: root.vertical ? parent.width : (visible ? 8 : 0)
        height: root.vertical ? (visible ? 8 : 0) : parent.height

        // A short, 2 px line, only while hovered.
        Rectangle {
            x: root.vertical ? (parent.width - width) / 2 : parent.width - width - 2
            y: root.vertical ? parent.height - height - 2 : (parent.height - height) / 2
            width: root.vertical ? 18 : 2
            height: root.vertical ? 2 : 18
            radius: 1
            color: Theme.textSecondary
            opacity: showDesktopMouse.containsMouse ? 0.9 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
        }
        MouseArea {
            id: showDesktopMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            onEntered: {
                const p = showDesktop.mapToItem(null, showDesktop.width / 2, showDesktop.height / 2)
                Menus.tooltip = { text: qsTr("Show desktop"), center: p.x, centerY: p.y, screen: root.screenName, owner: "show-desktop" }
            }
            onExited: {
                if (Menus.tooltip && Menus.tooltip.owner === "show-desktop") Menus.tooltip = null
            }
            onClicked: {
                root.fromHere()
                Tasks.toggleDesktop()
            }
        }
    }

}
