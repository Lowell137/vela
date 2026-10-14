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
    width: 1280
    height: Theme.taskbarHeight
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
        || Menus.trayOverflow !== null || Menus.preview !== null
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
    readonly property int bottomMargin: Math.round(slide * (height - 2))
    onBottomMarginChanged: Shell.setBottomMargin(root, bottomMargin)
    Item {
        anchors.fill: parent
        HoverHandler { id: barHover }
    }

    // The whole taskbar above the blurred background (acrylic).
    function updateBlur() {
        Effects.setBlur(root, [Qt.rect(0, 0, width, height)])
    }
    onWidthChanged: updateBlur()
    onHeightChanged: updateBlur()

    // A button's window previews (TaskbarPreview.qml).
    Timer {
        id: previewDelay
        property var task: null
        interval: 450
        onTriggered: {
            if (!task || !task.hovered || task.windowCount === 0) return
            const p = task.mapToItem(null, task.width / 2, 0)
            Menus.preview = { key: task.key, appIds: Tasks.appIds(task.index), center: p.x, screen: root.screenName }
        }
    }

    // --- right-click menus (docs/renderer.md §14.4-14.6) ---

    // The taskbar is at the bottom of the output: from its coordinates to the
    // output's.
    function screenPoint(item, x, y) {
        const p = item.mapToItem(null, x, y)
        return Qt.point(p.x, Screen.height - root.height + p.y)
    }
    // Taskbar menus rise from its top edge.
    readonly property real menuBottom: Screen.height - root.height - 4

    function openAbove(entries, item, centered, options) {
        const p = screenPoint(item, centered ? item.width / 2 : 0, 0)
        root.openMenu(entries, p.x, menuBottom, Object.assign({ above: true, centered: centered }, options || {}))
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
        const p = screenPoint(startButton, 0, 0)
        root.openMenu(winXEntries(), p.x, menuBottom, { above: true, keyboard: keyboard })
    }

    // The menu of a notification area icon (the app decides it). Those in the
    // flyout open their own (TrayOverflow.qml).
    Connections {
        target: Tray
        enabled: root.primary && !Menus.trayOverflow
        function onMenuReady(row, anchorX, entries) {
            root.openMenu(Menus.trayEntries(row, entries), anchorX, root.menuBottom, { above: true, centered: true })
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.taskbar

        // Empty space: Task Manager and taskbar settings.
        MouseArea {
            id: emptyArea
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onClicked: mouse => {
                const p = root.screenPoint(emptyArea, mouse.x, 0)
                root.openMenu([
                    { text: qsTr("Task &manager"), icon: "utilities-system-monitor", enabled: System.available("task-manager"), action: () => System.trigger("task-manager") },
                    { text: qsTr("Taskbar &settings"), icon: "configure", enabled: System.available("taskbar-settings"), action: () => System.trigger("taskbar-settings") }
                ], p.x, root.menuBottom, { above: true })
            }
        }

        // A thin line of light on the top edge, like glass.
        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: 1
            color: Theme.stroke
        }
    }

    Row {
        id: buttons
        // Centered by hand instead of with anchors: so when an app opens or
        // closes, the row slides to the new center instead of jumping. Or on
        // the left, like earlier versions of Windows.
        x: Config.taskbarAlignment === "left" ? 12 : Math.round((parent.width - width) / 2)
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4

        Behavior on x {
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
                property: "x"; duration: Theme.normal
                easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate
            }
        }

        TaskbarButton {
            id: startButton
            active: Shell.startMenuOpen
            onClicked: { root.fromHere(); Shell.toggleStartMenu() }
            onRightClicked: root.openWinX(false)
            StartGlyph { anchors.centerIn: parent }
        }

        // Task View: two overlapping windows, like the Windows 11 icon.
        TaskbarButton {
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

        Repeater {
            id: taskRepeater
            model: Tasks

            TaskbarButton {
                id: task
                draggable: true
                // Dropped further away: it moves by as many places as buttons
                // it passed.
                onDropped: dx => {
                    const to = Math.max(0, Math.min(taskRepeater.count - 1, index + Math.round(dx / (width + buttons.spacing))))
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

                // Unread count / progress / urgent the app sent (Unity LauncherEntry).
                readonly property var badge: { Badges.revision; return Badges.entry(desktopId) }
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
                onWindowCountChanged: reportGeometry()
                Connections {
                    target: buttons
                    function onXChanged() { task.reportGeometry() }
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
                    x: parent.width - width + 5
                    y: -5
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

    // Notification area: the apps' icons (Telegram, Discord, Steam...), left
    // of the clock like on Windows. With the overflow on (the default) only
    // those asking for attention stay here, the others are behind the "^".
    Row {
        id: tray
        visible: root.primary
        anchors { right: screenShare.left; top: parent.top; bottom: parent.bottom; rightMargin: 4 }
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
                height: tray.height
                visible: icon !== "" && !hidden
            }
        }
    }

    // The "^" button: the hidden icons in a flyout; it points down while open.
    Item {
        id: overflowButton
        visible: root.primary && Config.trayOverflow && tray.hiddenCount > 0
        anchors { right: tray.left; top: parent.top; bottom: parent.bottom }
        width: visible ? 32 : 0
        readonly property bool open: !!Menus.trayOverflow && Menus.trayOverflow.screen === root.screenName
        onVisibleChanged: if (!visible && open) Menus.trayOverflow = null

        Rectangle {
            anchors { fill: parent; topMargin: 8; bottomMargin: 8 }
            radius: Theme.radius
            color: overflowMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
            opacity: overflowMouse.containsMouse || overflowButton.open ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
        }
        Shape {
            anchors.centerIn: parent
            width: 10
            height: 6
            rotation: overflowButton.open ? 180 : 0
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
                const p = overflowButton.mapToItem(null, overflowButton.width / 2, 0)
                Menus.trayOverflow = overflowButton.open ? null : { center: p.x, screen: root.screenName }
            }
        }
    }

    // The screen being shared or recorded, like Windows 11: a red dot over a
    // screen, "Your screen is being shared" in the tooltip (the compositor
    // says when, capture_watch.c).
    Item {
        id: screenShare
        visible: root.primary && Shell.screenCaptured
        anchors { right: microphone.left; top: parent.top; bottom: parent.bottom }
        width: visible ? 32 : 0

        function showTooltip() {
            const p = screenShare.mapToItem(null, screenShare.width / 2, 0)
            Menus.tooltip = { text: Notifications.captureSilenced ? qsTr("Your screen is being shared. Notifications are silenced.") : qsTr("Your screen is being shared"), center: p.x, screen: root.screenName, owner: "screenShare" }
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
            anchors { fill: parent; topMargin: 8; bottomMargin: 8 }
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
        visible: root.primary && Audio.microphoneUsers.length > 0
        anchors { right: systemIcons.left; top: parent.top; bottom: parent.bottom }
        width: visible ? 32 : 0

        readonly property string tooltipText: {
            const users = Audio.microphoneUsers
            if (users.length === 1) {
                return qsTr("%1 is using your microphone").arg(users[0].name)
            }
            return [qsTr("These apps are using your microphone:")].concat(users.map(u => u.name)).join("\n")
        }
        function showTooltip() {
            const p = microphone.mapToItem(null, microphone.width / 2, 0)
            Menus.tooltip = { text: microphone.tooltipText, center: p.x, screen: root.screenName, owner: "microphone" }
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
            anchors { fill: parent; topMargin: 8; bottomMargin: 8 }
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
        visible: root.primary
        anchors { right: clock.left; top: parent.top; bottom: parent.bottom; rightMargin: 2 }
        width: visible ? iconsRow.width + 16 : 0

        Rectangle {
            anchors { fill: parent; topMargin: 4; bottomMargin: 4 }
            radius: Theme.radius
            color: systemMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
            opacity: systemMouse.containsMouse || Menus.quickSettingsOpen ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
        }
        Row {
            id: iconsRow
            anchors.centerIn: parent
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
                const p = root.screenPoint(systemMouse, mouse.x, 0)
                const overVolume = volumeIcon.visible && mouse.x >= volumeIcon.x + iconsRow.x - 5
                    && mouse.x < volumeIcon.x + iconsRow.x + volumeIcon.width + 5
                root.openMenu(overVolume ? [
                    { text: qsTr("Open volume &mixer"), icon: "audio-volume-high", enabled: Audio.available, action: () => { root.fromHere(); Shell.soundOutputRequested() } },
                    { text: qsTr("&Sound settings"), icon: "preferences-desktop-sound", enabled: System.available("sound-settings"), action: () => System.trigger("sound-settings") }
                ] : [
                    { text: qsTr("&Troubleshoot network problems"), icon: "network-workgroup", enabled: false },
                    { text: qsTr("&Network and Internet settings"), icon: "preferences-system-network", enabled: System.available("network"), action: () => System.trigger("network") }
                ], p.x, root.menuBottom, { above: true })
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

    // Clock: time on top, date below.
    Item {
        id: clock
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom; rightMargin: 12 }
        width: clockColumn.implicitWidth + 16 + (Notifications.history.count > 0 || Notifications.silenced ? 22 : 0)

        property date now: new Date()

        Timer {
            interval: 1000
            running: true
            repeat: true
            onTriggered: clock.now = new Date()
        }

        Rectangle {
            anchors { fill: parent; topMargin: 4; bottomMargin: 4 }
            radius: Theme.radius
            color: Theme.subtleHover
            opacity: clockMouse.containsMouse || Menus.notificationCenterOpen ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
        }
        // Notifications to read: a number next to the clock, like Windows 11
        // (with "Do not disturb" a muted bell).
        Rectangle {
            visible: Notifications.history.count > 0 || Notifications.silenced
            anchors { right: parent.right; rightMargin: -2; verticalCenter: parent.verticalCenter }
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
            anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
            spacing: 1

            Text {
                anchors.right: parent.right
                text: Qt.formatTime(clock.now, "HH:mm")
                color: Theme.text
                font.pixelSize: Theme.fontCaption
            }
            Text {
                anchors.right: parent.right
                text: Qt.formatDate(clock.now, "dd/MM/yyyy")
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
                const p = root.screenPoint(clock, mouse.x, 0)
                root.openMenu([
                    { text: qsTr("&Adjust date and time"), icon: "preferences-system-time", enabled: System.available("datetime"), action: () => System.trigger("datetime") },
                    { text: qsTr("&Notification settings"), icon: "preferences-desktop-notification", enabled: System.available("notification-settings"), action: () => System.trigger("notification-settings") }
                ], p.x, root.menuBottom, { above: true })
            }
        }
    }

    // Show desktop, at the far right edge like Windows 11: a click does what
    // Win+D does (Tasks.toggleDesktop); the thin line lights up on hover.
    Item {
        id: showDesktop
        visible: root.primary
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
        // Invisible at rest and over the clock's margin: it takes no room. Only
        // under the pointer a short line shows, like Windows.
        width: visible ? 8 : 0

        // A short, 2 px line, only while hovered.
        Rectangle {
            anchors { right: parent.right; rightMargin: 2; verticalCenter: parent.verticalCenter }
            width: 2
            height: 18
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
                const p = showDesktop.mapToItem(null, showDesktop.width / 2, 0)
                Menus.tooltip = { text: qsTr("Show desktop"), center: p.x, screen: root.screenName, owner: "show-desktop" }
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
