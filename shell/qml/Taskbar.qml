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
    readonly property real startedAt: Date.now()
    // taskbar/style=islands: three floating surfaces instead of one bar. Along
    // a vertical taskbar it falls back to the classic one.
    readonly property bool islands: Config.taskbarStyle === "islands" && !vertical
    // The dock style (Config.dock) is taller: a floating pill with big icons;
    // the tray and clock live in the top panel (TopBar.qml).
    readonly property bool dock: Config.dock
    readonly property int barHeight: dock ? 68 : Theme.taskbarBarHeight
    // A theme's taskbar-margin / taskbar-radius: the bar floats inside its window.
    readonly property bool floating: !islands && !dock && (Theme.taskbarMargin > 0 || Theme.taskbarRadius > 0)
    // taskbar-fit: the bar shrinks to its content, apps and tray side by side
    // in the middle (Windhawk's WindowGlass). Widths only, no x: no binding loop.
    readonly property bool fit: floating && Theme.taskbarFit && !vertical
    readonly property bool edge: fit && Theme.taskbarEdge > 0
    readonly property int edgeGap: Math.max(Theme.taskbarMargin, 8) // the tray pill's gap to the screen's edge
    readonly property real trayGroupWidth: overflowButton.width + (tray.visible ? tray.width + 4 : 0)
        + screenShare.width + microphone.width + systemIcons.width + 2 + clock.width
    // taskbar-side-margin, shrunk when the screen is too narrow for it: the
    // weather and the tray and clock must still fit beside the centered apps.
    readonly property int sideMargin: Math.min(Theme.taskbarSideMargin, Math.max(Theme.taskbarMargin,
        Math.floor((width - buttons.width) / 2 - Math.max((weather.visible ? weather.width : 0) + 16, trayGroupWidth + 20))))
    readonly property bool underMaximized: {
        const name = root.screen ? root.screen.name : ""
        for (const r of Desktops.rects) {
            const desktop = Desktops.windows[r.id]
            if (r.maximized && r.output === name && (desktop === undefined || desktop === -1 || desktop === Desktops.current)) {
                return true
            }
        }
        return false
    }
    readonly property real fitGap: 8 + Theme.taskbarSplitGap
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
            Effects.setBlur(root, [r], dockPill.radius)
            Effects.setInputRegion(root, [r])
            return
        }
        // A single floating pill: only it, rounded. With the backdrop strip
        // showing or a pill per side, the whole window (the panel's alpha
        // gives the shape).
        if (floating && !floatingSurface.split && !edge && !(Theme.taskbarBackdrop && underMaximized)) {
            Effects.setBlur(root, [Qt.rect(floatingSurface.x, floatingSurface.y, floatingSurface.width, floatingSurface.height)], Theme.taskbarRadius)
            Effects.setInputRegion(root, [])
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
        Effects.setBlur(root, blur, Theme.radiusIsland)
        // The show desktop strip beyond the right island stays clickable.
        Effects.setInputRegion(root, Config.showDesktopCorner ? blur.concat([Qt.rect(width - 8, 0, 8, height)]) : blur)
    }
    onWidthChanged: updateBlur()
    onHeightChanged: updateBlur()
    onIslandsChanged: updateBlur()
    onUnderMaximizedChanged: updateBlur()
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
            item(qsTr("E&vent viewer"), "text-x-log-symbolic", "events"),
            item(qsTr("S&ystem"), "computer", "system"),
            item(qsTr("Device &manager"), "preferences-devices-tree", "devices"),
            item(qsTr("Net&work connections"), "preferences-system-network", "network"),
            item(qsTr("Dis&k management"), "drive-harddisk-symbolic", "disks"),
            item(qsTr("Computer mana&gement"), "computer", "computer"),
            { separator: true },
            item(qsTr("&Terminal"), "utilities-terminal-symbolic", "terminal"),
            item(qsTr("Terminal (&Admin)"), "utilities-terminal-symbolic", "terminal-admin"),
            { separator: true },
            item(qsTr("Task manage&r"), "task-manager", "task-manager"),
            item(qsTr("Setti&ngs"), "preferences-system-symbolic", "settings"),
            item(qsTr("File &Explorer"), "system-file-manager-symbolic", "files"),
            { text: qsTr("&Search"), icon: "search", action: () => { root.fromHere(); if (!Shell.startMenuOpen) Shell.toggleStartMenu() } },
            { text: qsTr("&Run"), icon: "system-run", action: () => { root.fromHere(); Shell.runRequested() } },
            { separator: true },
            { text: qsTr("Sh&ut down or sign out"), icon: "system-shutdown", children: [
                { text: qsTr("Sign &out"), icon: "system-log-out", action: () => Shell.logout() },
                { text: qsTr("S&leep"), icon: "system-suspend", enabled: Shell.canSuspend(), action: () => Shell.suspend() },
                { text: qsTr("Sh&ut down"), icon: "system-shutdown", action: () => Shell.powerOff() },
                { text: qsTr("&Restart"), icon: "system-reboot", action: () => Shell.reboot() }
            ] },
            { text: qsTr("&Desktop"), icon: "user-desktop-symbolic", action: () => Tasks.toggleDesktop() }
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
        visible: Config.taskView && Theme.taskbarTaskView
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

        // taskbar-backdrop: under a maximized window on this screen and desktop
        // the strip around the pill is blurred too (the blur follows its alpha).
        Rectangle {
            anchors.fill: parent
            visible: opacity > 0
            opacity: root.floating && Theme.taskbarBackdrop && root.underMaximized ? 1 : 0
            color: Theme.taskbarBackdropColor
            Behavior on opacity { NumberAnimation { duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.standard } }
        }
        Rectangle {
            id: floatingSurface
            readonly property bool split: root.fit && !root.edge && Theme.taskbarSplitGap > 0
            onXChanged: root.updateBlur()
            onYChanged: root.updateBlur()
            onWidthChanged: root.updateBlur()
            onHeightChanged: root.updateBlur()
            visible: root.floating
            x: root.fit ? buttons.x - 10 : root.sideMargin
            y: Theme.taskbarMargin
            width: split ? buttons.width + 14
                : root.edge ? buttons.width + 20
                : root.fit ? buttons.width + root.fitGap + root.trayGroupWidth + 10 : parent.width - 2 * root.sideMargin
            readonly property bool flush: root.edge && Theme.taskbarFlush
            height: parent.height - (flush ? 1 : 2) * Theme.taskbarMargin
            radius: Theme.taskbarRadius
            topRightRadius: split ? Theme.taskbarSplitRadius : radius
            bottomRightRadius: flush ? 0 : topRightRadius
            bottomLeftRadius: flush ? 0 : radius
            color: Theme.taskbar
            border.width: 1
            border.color: Theme.taskbarStroke
        }
        // taskbar-split-gap: the tray a pill of its own, like WindowGlass's Split.
        Rectangle {
            visible: floatingSurface.visible && (floatingSurface.split || root.edge)
            x: root.edge ? parent.width - root.edgeGap - 12 - root.trayGroupWidth - 10
                : floatingSurface.x + floatingSurface.width + Theme.taskbarSplitGap
            y: floatingSurface.flush ? 3 : floatingSurface.y
            width: root.edge ? root.trayGroupWidth + 22 : buttons.x + buttons.width + root.fitGap + root.trayGroupWidth - x
            height: floatingSurface.flush ? parent.height - 6 : floatingSurface.height
            radius: Theme.taskbarRadius
            topLeftRadius: root.edge ? radius : Theme.taskbarSplitRadius
            bottomLeftRadius: topLeftRadius
            color: Theme.taskbar
            border.width: 1
            border.color: Theme.taskbarStroke
        }

        // taskbar-reflection: Aero's streaks of light over the glass.
        Image {
            anchors.fill: parent
            visible: source != "" && !root.islands && !root.dock && !root.floating
            source: Theme.taskbarReflection
            fillMode: Image.PreserveAspectCrop
            opacity: Theme.taskbarReflectionOpacity
            asynchronous: true
        }

        // Empty space: Task Manager and taskbar settings.
        MouseArea {
            id: emptyArea
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onClicked: mouse => {
                const p = root.screenPoint(emptyArea, mouse.x, mouse.y)
                root.edgeMenu([
                    { text: qsTr("Task &manager"), icon: "task-manager", enabled: System.available("task-manager"), action: () => System.trigger("task-manager") },
                    { text: qsTr("Taskbar &settings"), icon: "taskbar-settings", enabled: System.available("taskbar-settings"), action: () => System.trigger("taskbar-settings") }
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
            color: Theme.taskbarStroke
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
            : root.edge ? (Theme.taskbarEdge === 2 ? Theme.taskbarMargin + 12 : Math.round((parent.width - width) / 2))
            : root.fit ? Math.round((parent.width - width - root.fitGap - root.trayGroupWidth) / 2)
            : root.islands ? (Config.taskbarAlignment === "left" ? leftIsland.x + leftIsland.width + 14
                : Math.max(leftIsland.x + leftIsland.width + 14, Math.round((parent.width - width) / 2)))
            : Config.taskbarAlignment === "left" || Theme.taskbarAlignLeft ? 12 + weather.width + (weather.width > 0 ? 8 : 0)
              // Centered, but off center rather than over the weather or the
              // tray when the screen is narrow, like Windows 11.
              : Math.max((root.floating ? root.sideMargin : 0) + 12 + (weather.visible ? weather.width + 8 : 0),
                  Math.min(Math.round((parent.width - width) / 2), overflowButton.x - 8 - width))
        y: root.vertical ? (Config.taskbarAlignment === "left" ? 12 : Math.max(12, Math.round((parent.height - height) / 2)))
            : root.dock ? dockPill.y + Math.round((dockPill.height - height) / 2)
            : Math.round((parent.height - height) / 2)
        spacing: Theme.taskbarSpacing

        Behavior on x {
            enabled: Theme.motion > 0
            Eased {}
        }
        Behavior on y {
            NumberAnimation { duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        }
        // The button of an app just opened comes in growing.
        add: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.normal }
            Eased { property: "scale"; from: 1 - 0.5 * Theme.motion; to: 1; duration: Theme.slow }
        }
        move: Transition {
            enabled: !taskRepeater.dropping
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
            visible: Config.taskView && Theme.taskbarTaskView && !root.islands
        }

        // The buttons are already where the drop puts them.
        Timer { id: dropDone; interval: 100; onTriggered: taskRepeater.dropping = false }

        Repeater {
            id: taskRepeater
            model: Tasks
            // The drag in progress: the button dragged from dragFrom, the place it
            // would take (dragTo), within its group (pinned or not).
            property int dragFrom: -1
            property int dragTo: -1
            property int dragMin: 0
            property int dragMax: 0
            property bool dropping: false
            onCountChanged: dragFrom = -1

            TaskbarButton {
                id: task
                draggable: true
                hops: true
                // With the setting on, opening a window lifts the icon and
                // closing its last one drops it.
                readonly property bool windowBounce: Config.taskbarWindowBounce && Theme.taskbarActivateHop
                property int lastCount: windowCount
                // An app that isn't pinned gets its button with its first
                // window; not at the shell's start, for those already open.
                Component.onCompleted: if (windowBounce && windowCount > 0 && Date.now() - root.startedAt > 3000) bounce(false)
                // Dropped further away: it moves by as many places as buttons
                // it passed.
                readonly property real step: (root.vertical ? height : width) + buttons.spacing
                dragLow: dragging ? (taskRepeater.dragMin - index) * step : -Infinity
                dragHigh: dragging ? (taskRepeater.dragMax - index) * step : Infinity
                onDragOffsetChanged: if (dragging && taskRepeater.dragFrom === index) taskRepeater.dragTo = index + Math.round(dragOffset / step)
                slideAnimated: !taskRepeater.dropping
                slide: {
                    const from = taskRepeater.dragFrom
                    const to = taskRepeater.dragTo
                    if (from < 0 || index === from) return 0
                    if (index > from && index <= to) return -step
                    if (index < from && index >= to) return step
                    return 0
                }
                onDropped: dx => {
                    const from = taskRepeater.dragFrom
                    const to = taskRepeater.dragTo
                    const offset = Math.max(dragLow, Math.min(dragHigh, dx))
                    taskRepeater.dropping = true
                    taskRepeater.dragFrom = -1
                    dropDone.restart()
                    if (from !== index) {
                        settle(offset)
                        return
                    }
                    if (to !== index) Tasks.move(index, to)
                    settle(offset - (to - from) * step)
                }
                onDraggingChanged: {
                    if (!dragging) return
                    Menus.preview = null
                    let first = index
                    let last = index
                    while (first > 0 && taskRepeater.itemAt(first - 1).pinned === pinned) --first
                    while (last < taskRepeater.count - 1 && taskRepeater.itemAt(last + 1).pinned === pinned) ++last
                    taskRepeater.dragMin = first
                    taskRepeater.dragMax = last
                    taskRepeater.dragTo = index
                    taskRepeater.dragFrom = index
                }
                required property int index
                required property string key
                required property string name
                required property string iconName
                required property int windowCount
                required property bool pinned
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
                onWindowCountChanged: {
                    reportGeometry()
                    if (windowBounce && (windowCount > 0) !== (lastCount > 0)) bounce(windowCount === 0)
                    lastCount = windowCount
                }
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

    // Transfers remain tracked by FileOps; the shell only aggregates status.
    Item {
        id: transferIndicator
        objectName: "transferIndicator"
        visible: root.primary && !root.dock && Transfers.activeCount > 0
        x: root.vertical ? 0 : overflowButton.x - width - 4
        y: root.vertical ? overflowButton.y - height - 4 : 0
        width: root.vertical ? parent.width : visible ? 68 : 0
        height: root.vertical ? (visible ? 32 : 0) : parent.height
        readonly property var running: Transfers.jobs.filter(job => !job.finished)
        readonly property real progress: {
            if (running.length !== 1 || running[0].total <= 0) return -1
            return Math.max(0, Math.min(100, Math.round(100 * running[0].done / running[0].total)))
        }
        function readableSize(bytes) {
            if (bytes >= 1073741824) return (bytes / 1073741824).toFixed(1) + " GB"
            if (bytes >= 1048576) return (bytes / 1048576).toFixed(1) + " MB"
            if (bytes >= 1024) return (bytes / 1024).toFixed(1) + " KB"
            return Math.round(bytes) + " B"
        }
        function jobById(owner, id) {
            return Transfers.jobs.find(job => job.owner === owner && job.id === id)
        }
        function openTransferMenu() {
            const entries = [{ header: qsTr("File transfers") }]
            for (const initial of running) {
                const owner = initial.owner
                const id = initial.id
                entries.push({
                    text: initial.title, enabled: false,
                    liveText: () => {
                        const j = jobById(owner, id)
                        if (!j) return qsTr("Transfer removed")
                        return j.title + " · " + (j.finished ? qsTr("Finished")
                            : j.total > 0 ? Math.round(100 * j.done / j.total) + "%" : qsTr("Preparing"))
                    }
                })
                entries.push({
                    text: initial.current || qsTr("Preparing"), enabled: false,
                    liveText: () => {
                        const j = jobById(owner, id)
                        return j ? (j.current || (j.finished ? qsTr("Finished") : qsTr("Preparing"))) : ""
                    }
                })
                entries.push({
                    text: qsTr("Bytes transferred"), enabled: false,
                    liveText: () => {
                        const j = jobById(owner, id)
                        return j ? readableSize(j.done) + " / " +
                            (j.total > 0 ? readableSize(j.total) : qsTr("calculating")) : ""
                    }
                })
                entries.push({
                    text: qsTr("Calculating speed"), enabled: false,
                    liveText: () => {
                        const j = jobById(owner, id)
                        if (!j) return ""
                        if (j.finished) return qsTr("Transfer finished")
                        if (j.speed <= 0) return qsTr("Calculating transfer speed…")
                        const eta = j.eta < 0 ? qsTr("time unknown")
                            : j.eta < 60 ? qsTr("%1 seconds remaining").arg(Math.ceil(j.eta))
                            : qsTr("%1 minutes remaining").arg(Math.ceil(j.eta / 60))
                        return readableSize(j.speed) + "/s · " + eta
                    }
                })
                entries.push({ text: qsTr("Cancel transfer"), icon: "process-stop",
                               action: () => Transfers.cancel(owner, id) })
                entries.push({ separator: true })
            }
            root.openAbove(entries, transferIndicator, true, { minWidth: 360 })
        }
        Fill {
            anchors { fill: parent; topMargin: 8; bottomMargin: 8 }
            radius: Theme.radius
            color: transferMouse.pressed ? Theme.subtlePressed
                : transferMouse.containsMouse ? Theme.subtleHover : Theme.subtleRest
        }
        Icon {
            x: 7
            anchors.verticalCenter: parent.verticalCenter
            name: "document-save"
            size: 16
        }
        Text {
            anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 6 }
            text: transferIndicator.progress >= 0 ? transferIndicator.progress + "%"
                : String(Transfers.activeCount)
            color: Theme.text
            font.pixelSize: Theme.fontCaption
        }
        MouseArea {
            id: transferMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: transferIndicator.openTransferMenu()
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
        visible: root.primary && !root.dock && !root.fit && Weather.shown && Theme.taskbarWeather
        vertical: root.vertical
        screenName: root.screenName
        x: root.vertical ? 0 : root.islands ? 14 : 8 + (root.floating ? root.sideMargin : 0)
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
        x: root.vertical ? 0 : caffeine.x - width
        y: root.vertical ? caffeine.y - height : 0
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
            color: microphoneMouse.pressed ? Theme.subtlePressed : microphoneMouse.containsMouse ? Theme.subtleHover : Theme.subtleRest
        }
        Icon {
            anchors.centerIn: parent
            name: "audio-input-microphone"
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

    // Caffeine (keep awake, compositor/src/lock.c): always here, faint while
    // off. Either click lists the durations; the remaining time is in the
    // tooltip and at the top of the menu. Breeze's only cup is KTeaTime's.
    Item {
        id: caffeine
        visible: root.primary && !root.dock && (Config.taskbarCaffeine === "always" || (Config.taskbarCaffeine === "on" && Access.caffeine))
        x: root.vertical ? 0 : systemIcons.x - width
        y: root.vertical ? systemIcons.y - height : 0
        width: root.vertical ? parent.width : (visible ? 32 : 0)
        height: root.vertical ? (visible ? 32 : 0) : parent.height

        function remainingText(secs) {
            const minutes = Math.ceil(secs / 60)
            if (minutes < 60) {
                return minutes === 1 ? qsTr("1 minute remaining") : qsTr("%1 minutes remaining").arg(minutes)
            }
            if (minutes % 60 === 0) {
                return qsTr("%1 h remaining").arg(minutes / 60)
            }
            return qsTr("%1 h %2 min remaining").arg(Math.floor(minutes / 60)).arg(minutes % 60)
        }
        readonly property string tooltipText: !Access.caffeine ? qsTr("Caffeine: off")
            : Access.caffeineSecs > 0 ? qsTr("Caffeine: %1").arg(remainingText(Access.caffeineSecs))
            : qsTr("Caffeine: keeping the computer awake")
        function showTooltip() {
            const p = caffeine.mapToItem(null, caffeine.width / 2, 0)
            Menus.tooltip = { text: caffeine.tooltipText, center: p.x, screen: root.screenName, owner: "caffeine" }
        }
        function hideTooltip() {
            caffeineTooltipDelay.stop()
            if (Menus.tooltip && Menus.tooltip.owner === "caffeine") {
                Menus.tooltip = null
            }
        }
        onTooltipTextChanged: if (Menus.tooltip && Menus.tooltip.owner === "caffeine") showTooltip()
        onVisibleChanged: if (!visible) hideTooltip()

        // The selected preset, not the time left (which keeps decreasing).
        function openMenu() {
            const presets = [15, 30, 60, 120, 180, 300, 480]
            const names = [qsTr("15 minutes"), qsTr("30 minutes"), qsTr("1 hour"), qsTr("2 hours"),
                           qsTr("3 hours"), qsTr("5 hours"), qsTr("8 hours")]
            const entries = []
            if (Access.caffeine && Access.caffeineSecs > 0) {
                entries.push({ header: remainingText(Access.caffeineSecs) })
            }
            entries.push({ text: qsTr("&Indefinitely"), radio: true,
                           checked: Access.caffeine && Access.caffeineSecs < 0,
                           action: () => Access.caffeine = true },
                         { separator: true })
            for (let i = 0; i < presets.length; i++) {
                const minutes = presets[i]
                entries.push({ text: names[i], radio: true,
                               checked: Access.caffeine && Access.caffeineDurationSecs === minutes * 60,
                               action: () => Access.setCaffeineMinutes(minutes) })
            }
            entries.push({ separator: true },
                         { text: qsTr("Turn &off"), enabled: Access.caffeine, action: () => Access.caffeine = false },
                         { text: qsTr("&Power settings"), icon: "preferences-system-power-management",
                           enabled: System.available("power"), action: () => System.trigger("power") })
            root.openAbove(entries, caffeine, true, { minWidth: 220 })
        }

        Timer {
            id: caffeineTooltipDelay
            interval: 500
            onTriggered: caffeine.showTooltip()
        }
        Fill {
            anchors { fill: parent; topMargin: 8; bottomMargin: 8 }
            radius: Theme.radius
            color: caffeineMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
            opacity: caffeineMouse.containsMouse ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
        }
        Image {
            anchors.centerIn: parent
            width: 16
            height: 16
            source: Theme.icons + "kteatime-symbolic"
            sourceSize: Qt.size(width, height)
            opacity: Access.caffeine ? 1 : 0.45
            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
        }
        MouseArea {
            id: caffeineMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onContainsMouseChanged: containsMouse ? caffeineTooltipDelay.restart() : caffeine.hideTooltip()
            onClicked: {
                caffeine.hideTooltip()
                caffeine.openMenu()
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
            color: systemMouse.pressed ? Theme.subtlePressed : systemMouse.containsMouse || Menus.quickSettingsOpen ? Theme.subtleHover : Theme.subtleRest
        }
        Grid {
            id: iconsRow
            anchors.centerIn: parent
            columns: root.vertical ? 1 : 3
            spacing: 10
            Icon {
                id: networkIcon
                name: Status.networkIconName
            }
            Icon {
                id: volumeIcon
                visible: Audio.available
                name: Audio.iconName
            }
            Icon {
                visible: Status.batteryPresent
                name: Status.batteryCharging ? "battery-good-charging" : "battery-good"
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
        x: root.vertical ? 0 : root.edge ? parent.width - width - root.edgeGap - 12
            : root.fit ? buttons.x + buttons.width + root.fitGap + root.trayGroupWidth - width
            : parent.width - width - 12 - (root.floating ? root.sideMargin : 0)
        y: root.vertical ? showDesktop.y - height - 4 : 0
        readonly property bool badge: Notifications.history.count > 0 || Notifications.silenced
        width: root.vertical ? parent.width : clockColumn.implicitWidth + 16 + (badge ? 24 : 0) // 8 px each side, 6 between text and badge
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
            x: root.vertical ? (parent.width - width) / 2 : parent.width - width - 8
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
            Icon {
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
        visible: (Config.showDesktopCorner || Theme.taskbarDesktopImage !== "") && !root.dock // on every taskbar, like upstream
        x: root.vertical ? 0 : parent.width - width
        y: root.vertical ? parent.height - height : 0
        // Invisible at rest and over the clock's margin: it takes no room. Only
        // under the pointer a short line shows, like Windows. With a picture
        // (Windows 7's) it is a visible strip.
        width: root.vertical ? parent.width : (visible ? (Theme.taskbarDesktopImage !== "" ? 15 : 8) : 0)
        height: root.vertical ? (visible ? 8 : 0) : parent.height

        Image {
            anchors.fill: parent
            visible: source != "" && !root.vertical
            source: showDesktopMouse.containsMouse ? Theme.taskbarDesktopImageHover : Theme.taskbarDesktopImage
            fillMode: Image.Stretch
        }
        // A short, 2 px line, only while hovered.
        Rectangle {
            visible: Theme.taskbarDesktopImage === ""
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
