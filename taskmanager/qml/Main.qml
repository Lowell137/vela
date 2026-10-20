// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtCore
import Vela.Controls
import Vela.TaskManager.Backend

// The Task Manager's window, the one of Windows 11's: its own title bar with
// the search, the pages at the left (Processes, Performance, Startup apps,
// Details, Services) and the chosen one beside them. What's shown comes from
// Sys (src/system.h), read again every second.
Window {
    id: root
    flags: Qt.Window | Qt.FramelessWindowHint // no title bar from Vela
    width: 1100
    height: 720
    minimumWidth: 640
    minimumHeight: 420
    visible: true
    color: Theme.background
    title: qsTr("Task Manager")

    onActiveChanged: Theme.windowActive = active

    readonly property bool maximized: visibility === Window.Maximized
    function toggleMaximized() { maximized ? showNormal() : showMaximized() }

    // --- pages ---
    readonly property var pages: [
        { id: "processes", icon: "view-process-all", title: qsTr("Processes") },
        { id: "performance", icon: "view-performance", title: qsTr("Performance") },
        { id: "startup", icon: "view-startup", title: qsTr("Startup apps") },
        { id: "details", icon: "view-details", title: qsTr("Details") },
        { id: "services", icon: "view-services", title: qsTr("Services") }
    ]
    property string page: "processes"
    onPageChanged: {
        selected = null
        if (page === "startup") Sys.refreshStartup()
        if (page === "services") Sys.refreshServices()
    }
    Settings {
        id: settings
        property alias page: root.page
        property bool wideRail: true
    }
    readonly property bool railOpen: settings.wideRail && width >= 860
    Timer {
        interval: 5000
        repeat: true
        running: root.page === "services"
        onTriggered: Sys.refreshServices()
    }

    // The chosen row of the page's list (its map), kept by key while the list changes.
    property var selected: null
    function isSelected(row) { return selected !== null && selected.key === row.key && selected.kind === row.kind }

    // --- numbers, as they're read ---
    function amount(bytes) {
        const units = [qsTr("B"), qsTr("KB"), qsTr("MB"), qsTr("GB"), qsTr("TB")]
        let value = bytes
        let unit = 0
        while (value >= 1024 && unit < units.length - 1) {
            value /= 1024
            unit++
        }
        return Number(value).toLocaleString(Qt.locale(), "f", unit === 0 || value >= 100 ? 0 : 1) + " " + units[unit]
    }
    function rate(bytes) { return qsTr("%1/s").arg(amount(bytes)) }
    function percent(value) { return Number(value).toLocaleString(Qt.locale(), "f", value >= 10 ? 0 : 1) + "%" }
    function duration(seconds) {
        const s = Math.floor(seconds)
        const pad = n => String(n).padStart(2, "0")
        return Math.floor(s / 86400) + ":" + pad(Math.floor(s % 86400 / 3600)) + ":" + pad(Math.floor(s % 3600 / 60)) + ":" + pad(s % 60)
    }
    // How strongly a cell is tinted: the busier, the more.
    function heat(ratio) {
        const a = Theme.accentFill
        return Qt.rgba(a.r, a.g, a.b, Math.min(0.6, 0.06 + Math.max(0, ratio) * 0.7))
    }

    // --- what the buttons do ---
    function endSelected() {
        if (!selected || !selected.pids) return
        const error = Sys.endTask(selected.pids)
        if (error !== "") showError(error)
    }
    function showError(message) {
        errorDialog.text = message
        errorDialog.open()
    }
    function rowMenu(row, x, y) {
        selected = row
        menuLayer.open([
            { text: qsTr("&End task"), enabled: row.mine, action: () => root.endSelected() },
            { text: qsTr("Open file &location"), enabled: row.mine, action: () => Sys.openLocation(row.pid) },
            { separator: true },
            { text: qsTr("Go to &details"), action: () => {
                search.text = row.name
                root.page = "details"
            } }
        ], x, y, {})
    }

    Shortcut { sequence: "Ctrl+F"; onActivated: search.forceActiveFocus() }
    Shortcut { sequence: "Delete"; enabled: !search.activeFocus && (root.page === "processes" || root.page === "details"); onActivated: root.endSelected() }
    Shortcut { sequence: "F5"; onActivated: root.page === "services" ? Sys.refreshServices() : Sys.refreshStartup() }

    // --- the title bar, with the search ---
    Item {
        id: strip
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 44

        DragHandler {
            target: null
            onActiveChanged: if (active) root.startSystemMove()
        }
        TapHandler {
            onDoubleTapped: root.toggleMaximized()
        }

        Image {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            source: "image://appicon/utilities-system-monitor"
            sourceSize: Qt.size(width, height)
        }
        Text {
            x: 44
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.text
            opacity: root.active ? 1 : 0.5
            font.pixelSize: Theme.fontCaption
        }
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(420, parent.width - 420)
            visible: width > 120
            height: 30
            radius: Theme.radius
            color: search.activeFocus ? Theme.layer : Theme.control
            border.width: 1
            border.color: search.activeFocus ? Theme.accentFill : Theme.controlStroke
            Image {
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                width: 14
                height: 14
                source: Theme.icons + "edit-find"
                sourceSize: Qt.size(width, height)
            }
            TextInput {
                id: search
                anchors { fill: parent; leftMargin: 32; rightMargin: 10 }
                verticalAlignment: TextInput.AlignVCenter
                clip: true
                color: Theme.text
                selectionColor: Theme.accent
                selectedTextColor: "white"
                font.pixelSize: Theme.fontBody
                onTextChanged: {
                    Sys.filter = text
                    if (root.page === "services") Sys.refreshServices()
                }
                Keys.onEscapePressed: {
                    text = ""
                    focus = false
                }
                Text {
                    visible: search.text === ""
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Type a name to search for")
                    color: Theme.textTertiary
                    font.pixelSize: Theme.fontBody
                }
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

    // --- the pages, at the left ---
    Column {
        id: rail
        anchors { left: parent.left; top: strip.bottom; bottom: parent.bottom; leftMargin: 4 }
        width: root.railOpen ? 220 : 44
        spacing: 2
        ToolButton {
            width: 40
            height: 36
            icon: "application-menu"
            tooltip: qsTr("Open or close the navigation")
            onClicked: settings.wideRail = !settings.wideRail
        }
        Repeater {
            model: root.pages
            delegate: Rectangle {
                id: entry
                required property var modelData
                readonly property bool current: root.page === modelData.id
                width: rail.width
                height: 36
                radius: Theme.radius
                color: entryMouse.pressed ? Theme.subtlePressed : current || entryMouse.containsMouse ? Theme.subtleHover : "transparent"
                onCurrentChanged: navIndicator.track(entry, current)
                Component.onCompleted: navIndicator.track(entry, current)
                Image {
                    x: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16
                    height: 16
                    source: Theme.icons + entry.modelData.icon
                    sourceSize: Qt.size(width, height)
                }
                Text {
                    visible: root.railOpen
                    x: 44
                    anchors.verticalCenter: parent.verticalCenter
                    text: entry.modelData.title
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                }
                MouseArea {
                    id: entryMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.page = entry.modelData.id
                }
            }
        }
    }

    // The current entry's pill: it slides between entries.
    NavIndicator { id: navIndicator }

    // --- the page ---
    // A column's name: a click sorts by it, again turns the order around.
    component Head: Item {
        id: head
        property string column
        property string title
        property string total // the whole machine's, above the name
        property bool alignRight: true
        height: 44
        Text {
            anchors { right: head.alignRight ? parent.right : undefined; left: head.alignRight ? undefined : parent.left; rightMargin: 12; leftMargin: 12; top: parent.top; topMargin: 2 }
            visible: head.total !== ""
            text: head.total
            color: Theme.text
            font.pixelSize: 16
        }
        Text {
            anchors { right: head.alignRight ? parent.right : undefined; left: head.alignRight ? undefined : parent.left; rightMargin: 12; leftMargin: 12; bottom: parent.bottom; bottomMargin: 4 }
            text: head.title + (Sys.sortColumn === head.column ? (Sys.descending ? "  ↓" : "  ↑") : "")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
        }
        MouseArea {
            anchors.fill: parent
            enabled: head.column !== ""
            onClicked: Sys.sortBy(head.column)
        }
    }
    // A plain column of a list's row.
    component Field: Text {
        height: parent ? parent.height : 0
        verticalAlignment: Text.AlignVCenter
        leftPadding: 12
        rightPadding: 12
        color: Theme.text
        font.pixelSize: Theme.fontCaption
        elide: Text.ElideRight
    }
    // A number in its column, tinted by how much of the machine it is.
    component Cell: Rectangle {
        property string value
        property real ratio: -1 // below 0: no tint
        height: parent ? parent.height - 2 : 0
        y: 1
        color: ratio < 0 ? "transparent" : root.heat(ratio)
        Text {
            anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
            text: parent.value
            color: Theme.text
            font.pixelSize: Theme.fontCaption
        }
    }

    Rectangle {
        id: body
        anchors { left: rail.right; leftMargin: 4; right: parent.right; top: strip.bottom; bottom: parent.bottom }
        topLeftRadius: Theme.radiusOverlay
        color: Theme.layer

        // The page's name and its commands.
        Item {
            id: bar
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: 52
            Text {
                x: 20
                anchors.verticalCenter: parent.verticalCenter
                text: root.pages.find(p => p.id === root.page).title
                color: Theme.text
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }
            Row {
                anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                spacing: 4
                ToolButton {
                    visible: root.page === "processes" || root.page === "details"
                    icon: "window-new"
                    text: qsTr("Run new task")
                    onClicked: runDialog.ask()
                }
                ToolButton {
                    visible: root.page === "processes" || root.page === "details"
                    icon: "process-stop"
                    text: qsTr("End task")
                    usable: root.selected !== null && root.selected.mine === true
                    onClicked: root.endSelected()
                }
                ToolButton {
                    visible: root.page === "services"
                    icon: "media-playback-start"
                    text: qsTr("Start")
                    usable: root.selected !== null && !root.selected.active
                    onClicked: Sys.service("start", root.selected.unit, root.selected.user)
                }
                ToolButton {
                    visible: root.page === "services"
                    icon: "process-stop"
                    text: qsTr("Stop")
                    usable: root.selected !== null && root.selected.active === true
                    onClicked: Sys.service("stop", root.selected.unit, root.selected.user)
                }
                ToolButton {
                    visible: root.page === "services"
                    icon: "view-refresh"
                    text: qsTr("Restart")
                    usable: root.selected !== null && root.selected.active === true
                    onClicked: Sys.service("restart", root.selected.unit, root.selected.user)
                }
            }
        }

        // --- Processes ---
        Item {
            anchors { left: parent.left; right: parent.right; top: bar.bottom; bottom: parent.bottom }
            visible: root.page === "processes"

            Row {
                id: processHead
                width: parent.width
                Head { width: parent.width - 290; column: "name"; title: qsTr("Name"); alignRight: false }
                Head { width: 80; column: "cpu"; title: qsTr("CPU"); total: root.percent(Sys.cpu.usage) }
                Head { width: 100; column: "memory"; title: qsTr("Memory"); total: root.percent(Sys.memory.usage) }
                Head {
                    width: 110
                    column: "disk"
                    title: qsTr("Disk")
                    total: root.percent(Sys.disks.reduce((most, disk) => Math.max(most, disk.usage), 0))
                }
            }
            ListView {
                id: processList
                anchors { left: parent.left; right: parent.right; top: processHead.bottom; bottom: parent.bottom }
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.page === "processes" ? Sys.processes : null
                delegate: Rectangle {
                    id: line
                    required property var row
                    readonly property bool isHeader: row.kind === "header"
                    width: processList.width
                    height: isHeader ? 36 : 32
                    color: isHeader ? "transparent" : root.isSelected(row) ? Theme.selection : lineMouse.containsMouse ? Theme.subtleHover : "transparent"

                    Text {
                        visible: line.isHeader
                        x: 20
                        anchors { bottom: parent.bottom; bottomMargin: 6 }
                        text: line.row.name
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.DemiBold
                    }
                    MouseArea {
                        id: lineMouse
                        anchors.fill: parent
                        enabled: !line.isHeader
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onPressed: mouse => {
                            root.selected = line.row
                            if (mouse.button === Qt.RightButton) {
                                const p = mapToItem(null, mouse.x, mouse.y)
                                root.rowMenu(line.row, p.x, p.y)
                            }
                        }
                        onDoubleClicked: if (line.row.kind === "app") Sys.toggle(line.row.key)
                    }
                    Row {
                        visible: !line.isHeader
                        height: parent.height
                        Item {
                            width: line.width - 290
                            height: parent.height
                            // An app opens to show what it started.
                            Text {
                                visible: line.row.kind === "app" && line.row.count > 1
                                x: 12
                                width: 16
                                anchors.verticalCenter: parent.verticalCenter
                                horizontalAlignment: Text.AlignHCenter
                                text: line.row.open ? "⌄" : "›"
                                color: Theme.textSecondary
                                font.pixelSize: 14
                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -6
                                    onClicked: Sys.toggle(line.row.key)
                                }
                            }
                            Image {
                                visible: line.row.kind === "app"
                                x: 36
                                anchors.verticalCenter: parent.verticalCenter
                                width: 16
                                height: 16
                                source: line.row.kind === "app" ? "image://appicon/" + encodeURIComponent(line.row.icon || "application-x-executable") : ""
                                sourceSize: Qt.size(width, height)
                            }
                            Text {
                                x: line.row.kind === "app" ? 62 : line.row.kind === "child" ? 62 : 36
                                width: parent.width - x - 8
                                anchors.verticalCenter: parent.verticalCenter
                                text: line.row.name + (line.row.kind === "app" && line.row.count > 1 ? " (" + line.row.count + ")" : "")
                                color: Theme.text
                                font.pixelSize: Theme.fontBody
                                elide: Text.ElideRight
                            }
                        }
                        Cell { width: 80; value: root.percent(line.row.cpu || 0); ratio: (line.row.cpu || 0) / 100 }
                        Cell { width: 100; value: root.amount(line.row.memory || 0); ratio: (line.row.memory || 0) / Math.max(1, Sys.memory.total) }
                        Cell { width: 110; value: root.rate(line.row.disk || 0); ratio: (line.row.disk || 0) / 50e6 }
                    }
                }
            }
            ScrollBar {
                flickable: processList
                anchors { right: parent.right; top: processList.top; bottom: parent.bottom }
            }
        }

        // --- Performance ---
        Item {
            id: performance
            anchors { left: parent.left; right: parent.right; top: bar.bottom; bottom: parent.bottom }
            visible: root.page === "performance"

            // Every resource: its name, what it's doing, where its minute is kept.
            readonly property var resources: {
                const list = [
                    { title: qsTr("CPU"), now: root.percent(Sys.cpu.usage) + "  " + (Sys.cpu.speed / 1000).toFixed(2) + " GHz", series: "cpu", ceiling: 100, kind: "cpu", index: 0 },
                    { title: qsTr("Memory"), now: root.amount(Sys.memory.used) + " / " + root.amount(Sys.memory.total) + " (" + root.percent(Sys.memory.usage) + ")", series: "memory", ceiling: 100, kind: "memory", index: 0 }
                ]
                Sys.disks.forEach((disk, i) => list.push({ title: qsTr("Disk %1 (%2)").arg(i).arg(disk.name), now: root.percent(disk.usage), series: "disk:" + disk.name, ceiling: 100, kind: "disk", index: i }))
                Sys.networks.forEach((net, i) => list.push({ title: net.name, now: qsTr("S: %1  R: %2").arg(root.rate(net.sent)).arg(root.rate(net.received)), series: "net:" + net.name, ceiling: 0, kind: "network", index: i }))
                Sys.gpus.forEach((gpu, i) => list.push({ title: qsTr("GPU %1").arg(i), now: root.percent(gpu.usage) + (gpu.temperature > 0 ? "  " + gpu.temperature + " °C" : ""), series: "gpu:" + i, ceiling: 100, kind: "gpu", index: i }))
                return list
            }
            property int chosen: 0
            readonly property var resource: resources[Math.min(chosen, resources.length - 1)]
            // Its numbers, as pairs of a name and a value.
            readonly property var facts: {
                const r = resource
                if (r.kind === "cpu") return [
                    [qsTr("Utilization"), root.percent(Sys.cpu.usage)], [qsTr("Speed"), (Sys.cpu.speed / 1000).toFixed(2) + " GHz"],
                    [qsTr("Processes"), Sys.cpu.processes], [qsTr("Threads"), Sys.cpu.threads],
                    [qsTr("Logical processors"), Sys.cpu.count], [qsTr("Up time"), root.duration(Sys.cpu.uptime)]
                ]
                if (r.kind === "memory") return [
                    [qsTr("In use"), root.amount(Sys.memory.used)], [qsTr("Available"), root.amount(Sys.memory.available)],
                    [qsTr("Cached"), root.amount(Sys.memory.cached)], [qsTr("Total"), root.amount(Sys.memory.total)],
                    [qsTr("Swap in use"), root.amount(Sys.memory.swapUsed)], [qsTr("Swap total"), root.amount(Sys.memory.swapTotal)]
                ]
                if (r.kind === "disk") {
                    const disk = Sys.disks[r.index]
                    return disk ? [
                        [qsTr("Active time"), root.percent(disk.usage)], [qsTr("Capacity"), root.amount(disk.size)],
                        [qsTr("Read speed"), root.rate(disk.read)], [qsTr("Write speed"), root.rate(disk.written)]
                    ] : []
                }
                if (r.kind === "network") {
                    const net = Sys.networks[r.index]
                    return net ? [
                        [qsTr("Send"), root.rate(net.sent)], [qsTr("Receive"), root.rate(net.received)],
                        [qsTr("IPv4 address"), net.address || "-"], [qsTr("Adapter name"), net.name]
                    ] : []
                }
                const gpu = Sys.gpus[r.index]
                return gpu ? [
                    [qsTr("Utilization"), root.percent(gpu.usage)],
                    [qsTr("Dedicated memory"), root.amount(gpu.memoryUsed) + " / " + root.amount(gpu.memoryTotal)],
                    [qsTr("Temperature"), gpu.temperature > 0 ? gpu.temperature + " °C" : "-"]
                ] : []
            }
            readonly property string subtitle: resource.kind === "cpu" ? Sys.cpu.name
                : resource.kind === "disk" && Sys.disks[resource.index] ? Sys.disks[resource.index].model
                : resource.kind === "gpu" && Sys.gpus[resource.index] ? Sys.gpus[resource.index].name : ""

            ListView {
                id: resourceList
                anchors { left: parent.left; top: parent.top; bottom: parent.bottom; leftMargin: 8 }
                width: Math.min(260, parent.width * 0.36)
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: performance.resources
                delegate: Rectangle {
                    id: card
                    required property var modelData
                    required property int index
                    width: resourceList.width
                    height: 68
                    radius: Theme.radius
                    color: performance.chosen === index ? Theme.selection : cardMouse.containsMouse ? Theme.subtleHover : "transparent"
                    Graph {
                        x: 10
                        anchors.verticalCenter: parent.verticalCenter
                        width: 72
                        height: 48
                        grid: false
                        values: Sys.history[card.modelData.series] || []
                        ceiling: card.modelData.ceiling
                        color: Theme.accentFill
                        Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                            border.width: 1
                            border.color: Theme.accentFill
                        }
                    }
                    Text {
                        x: 94
                        y: 14
                        width: parent.width - x - 8
                        text: card.modelData.title
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        elide: Text.ElideRight
                    }
                    Text {
                        x: 94
                        y: 36
                        width: parent.width - x - 8
                        text: card.modelData.now
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        id: cardMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: performance.chosen = card.index
                    }
                }
            }
            Item {
                anchors { left: resourceList.right; right: parent.right; top: parent.top; bottom: parent.bottom; leftMargin: 20; rightMargin: 20; bottomMargin: 16 }
                Text {
                    id: resourceTitle
                    text: performance.resource.title
                    color: Theme.text
                    font.pixelSize: 24
                }
                Text {
                    anchors { right: parent.right; baseline: resourceTitle.baseline }
                    width: parent.width - resourceTitle.width - 24
                    horizontalAlignment: Text.AlignRight
                    text: performance.subtitle
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    elide: Text.ElideLeft
                }
                Text {
                    id: scaleNote
                    anchors { top: resourceTitle.bottom; topMargin: 8 }
                    text: performance.resource.kind === "network" ? qsTr("Throughput, the last 60 seconds") : qsTr("% utilization, the last 60 seconds")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                }
                Graph {
                    id: mainGraph
                    anchors { left: parent.left; right: parent.right; top: scaleNote.bottom; topMargin: 4; bottom: facts.top; bottomMargin: 16 }
                    values: Sys.history[performance.resource.series] || []
                    ceiling: performance.resource.ceiling
                    color: Theme.accentFill
                }
                // What's sent, over what's received.
                Graph {
                    anchors.fill: mainGraph
                    visible: performance.resource.kind === "network"
                    grid: false
                    values: Sys.history["sent:" + performance.resource.title] || []
                    ceiling: 0
                    color: Theme.textSecondary
                }
                Flow {
                    id: facts
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                    spacing: 20
                    Repeater {
                        model: performance.facts
                        delegate: Column {
                            required property var modelData
                            width: 170
                            spacing: 2
                            Text {
                                text: modelData[0]
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                            }
                            Text {
                                width: parent.width
                                text: modelData[1]
                                color: Theme.text
                                font.pixelSize: 18
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }

        // --- Startup apps ---
        Item {
            anchors { left: parent.left; right: parent.right; top: bar.bottom; bottom: parent.bottom }
            visible: root.page === "startup"
            Text {
                visible: Sys.startup.count === 0
                anchors.centerIn: parent
                text: qsTr("No app starts with the session.")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
            }
            ListView {
                id: startupList
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                clip: true
                spacing: 4
                boundsBehavior: Flickable.StopAtBounds
                model: root.page === "startup" ? Sys.startup : null
                delegate: Rectangle {
                    id: app
                    required property var row
                    width: startupList.width - 8
                    height: 56
                    radius: Theme.radius
                    color: Theme.control
                    border.width: 1
                    border.color: Theme.controlStroke
                    Image {
                        x: 16
                        anchors.verticalCenter: parent.verticalCenter
                        width: 24
                        height: 24
                        source: "image://appicon/" + encodeURIComponent(app.row.icon || "application-x-executable")
                        sourceSize: Qt.size(width, height)
                    }
                    Column {
                        x: 56
                        width: parent.width - x - 110
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        Text {
                            width: parent.width
                            text: app.row.name
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: app.row.command
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideMiddle
                        }
                    }
                    Toggle {
                        anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
                        checked: app.row.enabled
                        onToggled: checked => {
                            const error = Sys.setStartup(app.row.key, checked)
                            if (error !== "") root.showError(error)
                        }
                    }
                }
            }
            ScrollBar {
                flickable: startupList
                anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
            }
        }

        // --- Details ---
        Item {
            anchors { left: parent.left; right: parent.right; top: bar.bottom; bottom: parent.bottom }
            visible: root.page === "details"
            readonly property real flexible: Math.max(120, (width - 520) / 3)

            Row {
                id: detailHead
                width: parent.width
                Head { width: parent.parent.flexible; column: "name"; title: qsTr("Name"); alignRight: false }
                Head { width: 80; column: "pid"; title: qsTr("PID") }
                Head { width: 120; column: ""; title: qsTr("Status"); alignRight: false }
                Head { width: 100; column: "user"; title: qsTr("User name"); alignRight: false }
                Head { width: 60; column: "cpu"; title: qsTr("CPU") }
                Head { width: 90; column: "memory"; title: qsTr("Memory") }
                Head { width: 70; column: "threads"; title: qsTr("Threads") }
                Head { width: parent.parent.flexible * 2; column: ""; title: qsTr("Command line"); alignRight: false }
            }
            ListView {
                id: detailList
                anchors { left: parent.left; right: parent.right; top: detailHead.bottom; bottom: parent.bottom }
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.page === "details" ? Sys.details : null
                delegate: Rectangle {
                    id: detail
                    required property var row
                    readonly property var asRow: ({ key: String(row.pid), kind: "detail", pid: row.pid, pids: [row.pid], name: row.name, mine: row.mine })
                    readonly property real flexible: Math.max(120, (width - 520) / 3)
                    width: detailList.width
                    height: 28
                    color: root.isSelected(asRow) ? Theme.selection : detailMouse.containsMouse ? Theme.subtleHover : "transparent"
                    MouseArea {
                        id: detailMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onPressed: mouse => {
                            root.selected = detail.asRow
                            if (mouse.button === Qt.RightButton) {
                                const p = mapToItem(null, mouse.x, mouse.y)
                                menuLayer.open([
                                    { text: qsTr("&End task"), enabled: detail.row.mine, action: () => root.endSelected() },
                                    { text: qsTr("Open file &location"), enabled: detail.row.mine, action: () => Sys.openLocation(detail.row.pid) }
                                ], p.x, p.y, {})
                            }
                        }
                    }
                    Row {
                        height: parent.height
                        Field { width: detail.flexible; text: detail.row.name }
                        Field { width: 80; text: detail.row.pid; horizontalAlignment: Text.AlignRight }
                        Field { width: 120; text: detail.row.state }
                        Field { width: 100; text: detail.row.user }
                        Field { width: 60; text: root.percent(detail.row.cpu); horizontalAlignment: Text.AlignRight }
                        Field { width: 90; text: root.amount(detail.row.memory); horizontalAlignment: Text.AlignRight }
                        Field { width: 70; text: detail.row.threads; horizontalAlignment: Text.AlignRight }
                        Field { width: detail.flexible * 2; text: detail.row.command; color: Theme.textSecondary }
                    }
                }
            }
            ScrollBar {
                flickable: detailList
                anchors { right: parent.right; top: detailList.top; bottom: parent.bottom }
            }
        }

        // --- Services ---
        Item {
            anchors { left: parent.left; right: parent.right; top: bar.bottom; bottom: parent.bottom }
            visible: root.page === "services"

            Row {
                id: serviceHead
                width: parent.width
                Head { width: parent.width * 0.32; column: ""; title: qsTr("Name"); alignRight: false }
                Head { width: parent.width * 0.68 - 220; column: ""; title: qsTr("Description"); alignRight: false }
                Head { width: 120; column: ""; title: qsTr("Status"); alignRight: false }
                Head { width: 100; column: ""; title: qsTr("Group"); alignRight: false }
            }
            ListView {
                id: serviceList
                anchors { left: parent.left; right: parent.right; top: serviceHead.bottom; bottom: parent.bottom }
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.page === "services" ? Sys.services : null
                delegate: Rectangle {
                    id: unit
                    required property var row
                    readonly property var asRow: ({ key: row.key, kind: "service", unit: row.unit, user: row.user, active: row.active })
                    width: serviceList.width
                    height: 28
                    color: root.isSelected(asRow) ? Theme.selection : unitMouse.containsMouse ? Theme.subtleHover : "transparent"
                    MouseArea {
                        id: unitMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onPressed: mouse => {
                            root.selected = unit.asRow
                            if (mouse.button === Qt.RightButton) {
                                const p = mapToItem(null, mouse.x, mouse.y)
                                menuLayer.open([
                                    { text: qsTr("&Start"), enabled: !unit.row.active, action: () => Sys.service("start", unit.row.unit, unit.row.user) },
                                    { text: qsTr("S&top"), enabled: unit.row.active, action: () => Sys.service("stop", unit.row.unit, unit.row.user) },
                                    { text: qsTr("&Restart"), enabled: unit.row.active, action: () => Sys.service("restart", unit.row.unit, unit.row.user) }
                                ], p.x, p.y, {})
                            }
                        }
                    }
                    Row {
                        height: parent.height
                        Field { width: unit.width * 0.32; text: unit.row.unit }
                        Field { width: unit.width * 0.68 - 220; text: unit.row.description; color: Theme.textSecondary }
                        Field { width: 120; text: unit.row.active ? qsTr("Running") : unit.row.state === "failed" ? qsTr("Failed") : qsTr("Stopped") }
                        Field { width: 100; text: unit.row.user ? qsTr("User") : qsTr("System") }
                    }
                }
            }
            ScrollBar {
                flickable: serviceList
                anchors { right: parent.right; top: serviceList.top; bottom: parent.bottom }
            }
        }
    }

    MenuLayer {
        id: menuLayer
        anchors.fill: parent
        z: 100
    }

    // --- dialogs ---
    Dialog {
        id: errorDialog
        title: qsTr("Task Manager")
    }
    Dialog {
        id: runDialog
        title: qsTr("Run new task")
        text: qsTr("Type the name of a program, and Vela will open it for you.")
        buttons: [{ text: qsTr("OK"), accent: true, value: "ok" }, { text: qsTr("Cancel"), value: "cancel" }]
        onChosen: value => { if (value === "ok" && command.text.trim() !== "") Sys.run(command.text) }
        function ask() {
            command.text = ""
            open()
            command.forceActiveFocus()
        }
        Rectangle {
            width: parent.width
            height: 32
            radius: Theme.radius
            color: Theme.control
            border.width: 1
            border.color: command.activeFocus ? Theme.accentFill : Theme.controlStroke
            TextInput {
                id: command
                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                verticalAlignment: TextInput.AlignVCenter
                clip: true
                color: Theme.text
                selectionColor: Theme.accent
                selectedTextColor: "white"
                font.pixelSize: Theme.fontBody
                Keys.onReturnPressed: runDialog.close("ok")
                Keys.onEnterPressed: runDialog.close("ok")
                Keys.onEscapePressed: runDialog.close("cancel")
            }
        }
    }

    ResizeEdges {}
}
