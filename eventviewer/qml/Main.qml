// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtCore
import Vela.Controls

// The Event Viewer's window, the one of Windows 11's: the views at the left,
// the filter bar, the table of events and the chosen one's details below.
// What's shown comes from Journal (src/reader.h): journalctl does the
// filtering, this only asks.
Window {
    id: root
    flags: Qt.Window | Qt.FramelessWindowHint // no title bar from Vela
    width: 1180
    height: 740
    minimumWidth: 720
    minimumHeight: 460
    visible: true
    color: Theme.background
    title: qsTr("Event Viewer")

    onActiveChanged: Theme.windowActive = active
    onClosing: Journal.follow = false // journalctl -f must not outlive the window

    readonly property bool maximized: visibility === Window.Maximized
    function toggleMaximized() { maximized ? showNormal() : showMaximized() }

    // --- views, levels, ranges ---
    readonly property var views: [
        { id: "all", icon: "view-list-text", title: qsTr("All logs") },
        { id: "warnings", icon: "dialog-warning", title: qsTr("Errors and warnings") },
        { id: "critical", icon: "dialog-error", title: qsTr("Critical") },
        { id: "boot", icon: "system-reboot", title: qsTr("This boot") },
        { id: "previous", icon: "view-history", title: qsTr("Previous boot") },
        { id: "kernel", icon: "cpu", title: qsTr("Kernel") },
        { id: "apps", icon: "applications-other", title: qsTr("Applications") },
        { id: "security", icon: "security-high", title: qsTr("Security") }
    ]
    readonly property var levelNames: [qsTr("Emergency"), qsTr("Alert"), qsTr("Critical"), qsTr("Error"),
        qsTr("Warning"), qsTr("Notice"), qsTr("Information"), qsTr("Debug")]
    readonly property var syslogNames: ["emerg", "alert", "crit", "err", "warning", "notice", "info", "debug"]
    readonly property var levelFilters: [
        { value: 7, title: qsTr("All levels") },
        { value: 5, title: qsTr("Notice and above") },
        { value: 4, title: qsTr("Warning and above") },
        { value: 3, title: qsTr("Error and above") },
        { value: 2, title: qsTr("Critical and above") }
    ]
    readonly property var ranges: [
        { id: "15m", title: qsTr("Last 15 minutes") },
        { id: "1h", title: qsTr("Last hour") },
        { id: "1d", title: qsTr("Last 24 hours") },
        { id: "1w", title: qsTr("Last 7 days") },
        { id: "boot", title: qsTr("Since boot") },
        { id: "all", title: qsTr("All time") }
    ]
    readonly property var scopes: [
        { id: "all", title: qsTr("System and user") },
        { id: "system", title: qsTr("System") },
        { id: "user", title: qsTr("User") }
    ]
    function titleOf(list, key, value) { return (list.find(e => e[key] === value) || list[0]).title }
    function levelColor(p) {
        return p <= 3 ? Theme.critical : p === 4 ? Theme.caution : p <= 6 ? Theme.accentFill : Theme.textTertiary
    }
    function timeText(ms) { return Qt.formatDateTime(new Date(ms), "yyyy-MM-dd HH:mm:ss") }

    Settings {
        id: settings
        property alias view: root.view
        property alias detailsHeight: root.detailsHeight
    }
    property string view: "all"
    property int detailsHeight: 210
    onViewChanged: Journal.view = view
    Component.onCompleted: Journal.view = view

    // The chosen event (its map), kept by cursor while the list moves.
    property var selected: null

    // --- what the buttons do ---
    function showError(message) {
        errorDialog.text = message
        errorDialog.open()
    }
    property var picker: null
    function saveShown() {
        if (!picker) {
            const component = Qt.createComponent("FilePicker.qml")
            picker = component.createObject(root, { parentWindow: root })
            if (!picker) {
                showError(component.errorString())
                return
            }
        }
        const folder = StandardPaths.writableLocation(StandardPaths.DocumentsLocation).toString()
        picker.pick(true, folder, "events.txt", files => {
            if (files.length === 0) return
            const error = Journal.save(files[0])
            if (error !== "") showError(error)
        })
    }
    function popup(button, entries) {
        const p = button.mapToItem(menuLayer, 0, button.height + 2)
        menuLayer.open(entries, p.x, p.y, {})
    }

    Shortcut { sequence: "Ctrl+F"; onActivated: search.forceActiveFocus() }
    Shortcut { sequence: "F5"; onActivated: Journal.reload() }
    Shortcut { sequence: "Ctrl+C"; enabled: !search.activeFocus && !unitField.activeFocus && root.selected !== null
        onActivated: Journal.copy(root.selected.message) }

    // --- the title bar ---
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
            source: "image://appicon/utilities-log-viewer"
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

    // --- the views, at the left ---
    Column {
        id: rail
        anchors { left: parent.left; top: strip.bottom; bottom: parent.bottom; leftMargin: 4 }
        width: 200
        spacing: 2
        Repeater {
            model: root.views
            delegate: Rectangle {
                id: entry
                required property var modelData
                readonly property bool current: root.view === modelData.id
                width: rail.width
                height: 36
                radius: Theme.radius
                color: current ? Theme.selection : entryMouse.containsMouse ? Theme.subtleHover : "transparent"
                Rectangle {
                    visible: entry.current
                    x: 0
                    anchors.verticalCenter: parent.verticalCenter
                    width: 3
                    height: 16
                    radius: 1.5
                    color: Theme.accentFill
                }
                Image {
                    x: 14
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16
                    height: 16
                    source: Theme.icons + encodeURIComponent(entry.modelData.icon)
                    sourceSize: Qt.size(width, height)
                }
                Text {
                    x: 42
                    width: parent.width - x - 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: entry.modelData.title
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    elide: Text.ElideRight
                }
                MouseArea {
                    id: entryMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.view = entry.modelData.id
                }
            }
        }
    }

    // --- the filter bar ---
    Item {
        id: content
        anchors { left: rail.right; right: parent.right; top: strip.bottom; bottom: parent.bottom; leftMargin: 8; rightMargin: 12; bottomMargin: 8 }

        component Field: Rectangle {
            id: field
            property alias text: input.text
            property alias placeholder: hint.text
            property alias activeInput: input.activeFocus
            function focusInput() { input.forceActiveFocus() }
            height: 32
            radius: Theme.radius
            color: input.activeFocus ? Theme.field : Theme.control
            border.width: 1
            border.color: input.activeFocus ? Theme.accentFill : Theme.controlStroke
            TextInput {
                id: input
                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                verticalAlignment: TextInput.AlignVCenter
                clip: true
                color: Theme.text
                selectionColor: Theme.accent
                selectedTextColor: "white"
                font.pixelSize: Theme.fontBody
                Keys.onEscapePressed: { text = ""; focus = false }
                Text {
                    id: hint
                    visible: input.text === ""
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.textTertiary
                    font.pixelSize: Theme.fontBody
                }
            }
        }

        Flow {
            id: bar
            anchors { left: parent.left; right: parent.right; top: parent.top }
            spacing: 8
            Field {
                id: search
                width: 240
                placeholder: qsTr("Search messages")
                onTextChanged: Journal.text = text
            }
            ToolButton {
                id: levelButton
                text: root.titleOf(root.levelFilters, "value", Journal.level)
                menu: true
                onClicked: root.popup(levelButton, root.levelFilters.map(l => ({
                    text: l.title, checked: Journal.level === l.value, action: () => { Journal.level = l.value }
                })))
            }
            ToolButton {
                id: rangeButton
                text: root.titleOf(root.ranges, "id", Journal.range)
                menu: true
                onClicked: root.popup(rangeButton, root.ranges.map(r => ({
                    text: r.title, checked: Journal.range === r.id, action: () => { Journal.range = r.id }
                })))
            }
            Field {
                id: unitField
                width: 170
                placeholder: qsTr("Unit, such as sshd")
                onTextChanged: Journal.unit = text
            }
            ToolButton {
                id: scopeButton
                text: root.titleOf(root.scopes, "id", Journal.scope)
                menu: true
                onClicked: root.popup(scopeButton, root.scopes.map(s => ({
                    text: s.title, checked: Journal.scope === s.id, action: () => { Journal.scope = s.id }
                })))
            }
            Row {
                spacing: 8
                height: 32
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Follow")
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                }
                Toggle {
                    anchors.verticalCenter: parent.verticalCenter
                    showLabel: false
                    checked: Journal.follow
                    onToggled: checked => { Journal.follow = checked }
                }
            }
            ToolButton {
                icon: "document-save-as"
                text: qsTr("Save the shown events as…")
                usable: Journal.count > 0
                onClicked: root.saveShown()
            }
            ToolButton {
                icon: "view-refresh"
                tooltip: qsTr("Reload (F5)")
                onClicked: Journal.reload()
            }
        }

        // --- the table ---
        Rectangle {
            id: table
            anchors { left: parent.left; right: parent.right; top: bar.bottom; bottom: details.top; topMargin: 10; bottomMargin: 8 }
            radius: Theme.radiusCard
            color: Theme.layer
            border.width: 1
            border.color: Theme.layerStroke
            clip: true

            readonly property int timeWidth: 160
            readonly property int levelWidth: 130
            readonly property int sourceWidth: Math.max(140, Math.min(260, width * 0.22))

            Row {
                id: header
                anchors { left: parent.left; right: parent.right; top: parent.top }
                height: 32
                Repeater {
                    model: [
                        { id: "time", title: qsTr("Date and time"), width: table.timeWidth },
                        { id: "level", title: qsTr("Level"), width: table.levelWidth },
                        { id: "source", title: qsTr("Source"), width: table.sourceWidth },
                        { id: "message", title: qsTr("Message"), width: table.width - table.timeWidth - table.levelWidth - table.sourceWidth }
                    ]
                    delegate: Rectangle {
                        id: head
                        required property var modelData
                        width: modelData.width
                        height: 32
                        color: headMouse.containsMouse ? Theme.subtleHover : "transparent"
                        Text {
                            x: 12
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 36
                            text: head.modelData.title
                            color: Theme.text
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideRight
                        }
                        Text {
                            visible: Journal.sortColumn === head.modelData.id
                            anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
                            text: Journal.descending ? "▼" : "▲"
                            color: Theme.textSecondary
                            font.pixelSize: 9
                        }
                        MouseArea {
                            id: headMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: Journal.sortBy(head.modelData.id)
                        }
                    }
                }
            }
            Rectangle {
                id: headLine
                anchors { left: parent.left; right: parent.right; top: header.bottom }
                height: 1
                color: Theme.divider
            }

            ListView {
                id: list
                anchors { left: parent.left; right: parent.right; top: headLine.bottom; bottom: parent.bottom }
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                reuseItems: true
                model: Journal.model
                delegate: Rectangle {
                    id: line
                    required property var model
                    readonly property var row: model.row
                    width: list.width
                    height: 28
                    color: root.selected && root.selected.key === row.key ? Theme.selection
                        : lineMouse.containsMouse ? Theme.subtleHover : "transparent"
                    Row {
                        anchors.fill: parent
                        Text {
                            width: table.timeWidth
                            height: parent.height
                            leftPadding: 12
                            verticalAlignment: Text.AlignVCenter
                            text: root.timeText(line.row.time)
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                        }
                        Item {
                            id: levelCell
                            width: table.levelWidth
                            height: parent.height
                            Rectangle {
                                x: 12
                                anchors.verticalCenter: parent.verticalCenter
                                width: 8
                                height: 8
                                radius: 4
                                color: root.levelColor(line.row.priority)
                            }
                            Text {
                                x: 28
                                width: parent.width - x - 4
                                height: parent.height
                                verticalAlignment: Text.AlignVCenter
                                text: root.levelNames[line.row.priority]
                                color: Theme.text
                                font.pixelSize: Theme.fontCaption
                                elide: Text.ElideRight
                            }
                            HoverHandler { id: levelHover }
                            Rectangle { // the syslog name, as a tooltip
                                visible: levelHover.hovered
                                x: 28
                                y: parent.height - 2
                                z: 10
                                width: syslogText.width + 16
                                height: 24
                                radius: Theme.radius
                                color: Theme.flyout
                                border.width: 1
                                border.color: Theme.stroke
                                Text {
                                    id: syslogText
                                    anchors.centerIn: parent
                                    text: line.row.syslog
                                    color: Theme.text
                                    font.pixelSize: Theme.fontCaption
                                }
                            }
                        }
                        Text {
                            width: table.sourceWidth
                            height: parent.height
                            leftPadding: 12
                            rightPadding: 8
                            verticalAlignment: Text.AlignVCenter
                            text: line.row.source
                            color: Theme.text
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width - x
                            height: parent.height
                            leftPadding: 12
                            rightPadding: 8
                            verticalAlignment: Text.AlignVCenter
                            text: line.row.summary
                            color: Theme.text
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideRight
                        }
                    }
                    MouseArea {
                        id: lineMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onPressed: mouse => {
                            root.selected = line.row
                            if (mouse.button === Qt.RightButton) {
                                const p = mapToItem(menuLayer, mouse.x, mouse.y)
                                menuLayer.open([
                                    { text: qsTr("Copy &message"), action: () => Journal.copy(line.row.message) },
                                    { text: qsTr("Copy &entry"), action: () => Journal.copy(root.entryText(line.row)) }
                                ], p.x, p.y, {})
                            }
                        }
                    }
                }
            }
            ScrollBar {
                flickable: list
                anchors { right: parent.right; top: headLine.bottom; bottom: parent.bottom }
            }

            // --- empty states ---
            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 48, 460)
                spacing: 8
                visible: Journal.count === 0 && Journal.state !== "loading"
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: Journal.state === "denied" ? qsTr("The journal can't be read")
                        : Journal.state === "failed" ? qsTr("journalctl failed") : qsTr("No events match")
                    color: Theme.text
                    font.pixelSize: Theme.fontBodyLarge
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: Journal.state === "denied"
                        ? qsTr("Your account isn't allowed to read the system journal. Ask an administrator to add you to the systemd-journal group (or adm), then sign out and in again. The user journal may still be readable: choose User above.")
                        : Journal.state === "failed" ? Journal.error : qsTr("Try a wider time range, a lower level or another view.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontBody
                }
            }
        }

        // --- the details of the chosen event ---
        Rectangle {
            id: details
            anchors { left: parent.left; right: parent.right; bottom: status.top; bottomMargin: 6 }
            height: Math.min(root.detailsHeight, content.height - 200)
            radius: Theme.radiusCard
            color: Theme.layer
            border.width: 1
            border.color: Theme.layerStroke
            clip: true

            // Dragging the top edge makes it taller or shorter.
            MouseArea {
                id: grip
                anchors { left: parent.left; right: parent.right; top: parent.top }
                height: 6
                cursorShape: Qt.SplitVCursor
                property real startY
                property int startHeight
                onPressed: mouse => { startY = mapToItem(content, 0, mouse.y).y; startHeight = details.height }
                onPositionChanged: mouse => {
                    if (!pressed) return
                    const dy = mapToItem(content, 0, mouse.y).y - startY
                    root.detailsHeight = Math.max(100, Math.min(content.height - 200, startHeight - dy))
                }
            }

            Text {
                anchors.centerIn: parent
                visible: root.selected === null
                text: qsTr("Select an event to see its details")
                color: Theme.textTertiary
                font.pixelSize: Theme.fontBody
            }
            Item {
                id: body
                anchors.fill: parent
                visible: root.selected !== null
                readonly property var fields: root.selected ? root.detailFields(root.selected) : []

                Flickable {
                    id: detailsFlick
                    anchors { left: parent.left; right: buttons.left; top: parent.top; bottom: parent.bottom; margins: 12 }
                    contentWidth: width
                    contentHeight: fieldsColumn.height
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    Column {
                        id: fieldsColumn
                        width: detailsFlick.width
                        spacing: 4
                        Repeater {
                            model: body.fields
                            delegate: Row {
                                id: fieldRow
                                required property var modelData
                                width: fieldsColumn.width
                                spacing: 12
                                Text {
                                    width: 110
                                    text: fieldRow.modelData[0]
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontCaption
                                    elide: Text.ElideRight
                                }
                                TextEdit {
                                    width: parent.width - 122
                                    text: fieldRow.modelData[1]
                                    readOnly: true
                                    selectByMouse: true
                                    wrapMode: TextEdit.Wrap
                                    color: Theme.text
                                    selectionColor: Theme.accent
                                    selectedTextColor: "white"
                                    font.pixelSize: Theme.fontCaption
                                }
                            }
                        }
                    }
                }
                ScrollBar {
                    flickable: detailsFlick
                    anchors { right: buttons.left; top: parent.top; bottom: parent.bottom; topMargin: 6; bottomMargin: 6 }
                }
                Column {
                    id: buttons
                    anchors { right: parent.right; top: parent.top; margins: 12 }
                    spacing: 8
                    Button {
                        width: 130
                        text: qsTr("Copy message")
                        onClicked: Journal.copy(root.selected.message)
                    }
                    Button {
                        width: 130
                        text: qsTr("Copy entry")
                        onClicked: Journal.copy(root.entryText(root.selected))
                    }
                }
            }
        }

        // --- the status line ---
        Text {
            id: status
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 4 }
            height: 20
            elide: Text.ElideRight
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            text: {
                if (Journal.state === "loading") return qsTr("Loading… %1 events").arg(Journal.count)
                let s = Journal.capped ? qsTr("Showing the last %1 events").arg(Journal.count) : qsTr("%1 events").arg(Journal.count)
                if (Journal.follow) s += " · " + qsTr("following")
                if (Journal.partial) s += " · " + qsTr("only part of the journal is readable: join the systemd-journal group")
                return s
            }
        }
    }

    // The entry as `Field: value` lines, from the details list.
    function detailFields(row) {
        const list = [
            [qsTr("Date and time"), timeText(row.time)],
            [qsTr("Level"), levelNames[row.priority] + " (" + row.syslog + ")"],
            [qsTr("Unit"), row.unit],
            [qsTr("Identifier"), row.identifier],
            [qsTr("PID"), row.pid > 0 ? String(row.pid) : ""],
            [qsTr("User ID"), row.uid >= 0 ? String(row.uid) : ""],
            [qsTr("Process"), row.comm],
            [qsTr("Executable"), row.exe],
            [qsTr("Command line"), row.cmdline],
            [qsTr("Computer"), row.hostname],
            [qsTr("Transport"), row.transport],
            [qsTr("Boot ID"), row.boot],
            [qsTr("Cursor"), row.cursor],
            [qsTr("Message"), row.message]
        ]
        return list.filter(f => f[1] !== "")
    }
    function entryText(row) { return detailFields(row).map(f => f[0] + ": " + f[1]).join("\n") }

    MenuLayer {
        id: menuLayer
        anchors.fill: parent
        z: 100
    }

    Dialog {
        id: errorDialog
        title: qsTr("Event Viewer")
    }

    ResizeEdges {}
}
