// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Shapes
import QtCore
import Vela.Controls

// The Clock's window, the one of Windows 11's Alarms & Clock: its own title
// bar, the pages at the left (Timer, Alarm, Stopwatch, World clock, Settings)
// and the chosen one beside them. What counts down and rings is Clock
// (src/clock.h); this only shows it, read again at every tick.
Window {
    id: root
    flags: Qt.Window | Qt.FramelessWindowHint // no title bar from Vela
    width: 900
    height: 640
    minimumWidth: 520
    minimumHeight: 420
    visible: true
    color: Theme.background
    title: qsTr("Clock")

    onActiveChanged: Theme.windowActive = active
    // Armed: the process stays without a window and rings (src/main.cpp).
    onClosing: close => {
        if (Clock.armed) {
            close.accepted = false
            root.visible = false
        } else {
            Qt.quit()
        }
    }

    readonly property bool maximized: visibility === Window.Maximized
    function toggleMaximized() { maximized ? showNormal() : showMaximized() }

    readonly property var pages: [
        { id: "timer", icon: "clock-timer", title: qsTr("Timer") },
        { id: "alarm", icon: "clock-alarm", title: qsTr("Alarm") },
        { id: "stopwatch", icon: "clock-stopwatch", title: qsTr("Stopwatch") },
        { id: "world", icon: "clock-world", title: qsTr("World clock") },
        { id: "settings", icon: "configure", title: qsTr("Settings") }
    ]
    property string page: "timer"
    Settings {
        id: settings
        property alias page: root.page
        property bool wideRail: true
    }
    readonly property bool railOpen: settings.wideRail && width >= 700

    // Centiseconds only matter where they are seen.
    Binding {
        target: Clock
        property: "fastTick"
        value: root.visible && root.page === "stopwatch" && Clock.swRunning
    }

    // --- how things are written ---
    function pad(n) { return String(n).padStart(2, "0") }
    function countdown(ms) {
        const s = Math.ceil(ms / 1000)
        const h = Math.floor(s / 3600)
        return (h > 0 ? h + ":" + pad(Math.floor(s % 3600 / 60)) : pad(Math.floor(s / 60))) + ":" + pad(s % 60)
    }
    function stopwatch(ms) {
        const cs = Math.floor(ms / 10)
        const h = Math.floor(cs / 360000)
        return (h > 0 ? h + ":" : "") + pad(Math.floor(cs / 6000) % 60) + ":" + pad(Math.floor(cs / 100) % 60) + "." + pad(cs % 100)
    }
    function clockText(h, m) {
        if (Clock.use24h) return pad(h) + ":" + pad(m)
        return ((h + 11) % 12 + 1) + ":" + pad(m) + " " + (h < 12 ? qsTr("AM") : qsTr("PM"))
    }
    // Monday first, as the bits of an alarm's days.
    function dayName(bit, format) { return Qt.locale().dayName((bit + 1) % 7, format) }
    function daysText(mask) {
        if (mask === 0) return qsTr("Once")
        if (mask === 127) return qsTr("Every day")
        const names = []
        for (let i = 0; i < 7; ++i) if (mask & (1 << i)) names.push(dayName(i, Locale.ShortFormat))
        return names.join(", ")
    }
    function etaText(secs) {
        if (secs < 0) return ""
        const m = Math.ceil(secs / 60)
        if (m < 1) return qsTr("in less than a minute")
        if (m < 60) return qsTr("in %1 min").arg(m)
        if (m < 1440) return qsTr("in %1 h %2 min").arg(Math.floor(m / 60)).arg(m % 60)
        return qsTr("in %1 d %2 h").arg(Math.floor(m / 1440)).arg(Math.floor(m % 1440 / 60))
    }
    function offsetText(secs) {
        if (secs === 0) return qsTr("Same time as here")
        const a = Math.abs(secs)
        const text = Math.floor(a / 3600) + (a % 3600 ? ":" + pad(Math.floor(a % 3600 / 60)) : "")
        return secs > 0 ? qsTr("%1 h ahead").arg(text) : qsTr("%1 h behind").arg(text)
    }
    function dayText(delta) { return delta === 0 ? qsTr("Today") : delta > 0 ? qsTr("Tomorrow") : qsTr("Yesterday") }

    // --- small pieces ---
    // A number with arrows over and under it, for hours, minutes, seconds.
    component NumberField: Column {
        id: field
        property int value: 0
        property int from: 0
        property int to: 59
        property bool wrap: true
        property string label
        spacing: 2
        function step(by) {
            let v = value + by
            if (v > to) v = wrap ? from : to
            if (v < from) v = wrap ? to : from
            value = v
        }
        ToolButton { anchors.horizontalCenter: parent.horizontalCenter; icon: "go-up"; onClicked: field.step(1) }
        Rectangle {
            width: 64
            height: 40
            radius: Theme.radius
            color: Theme.control
            border.width: 1
            border.color: Theme.controlStroke
            Text {
                anchors.centerIn: parent
                text: root.pad(field.value)
                color: Theme.text
                font.pixelSize: 20
            }
            MouseArea {
                anchors.fill: parent
                onWheel: wheel => field.step(wheel.angleDelta.y > 0 ? 1 : -1)
            }
        }
        ToolButton { anchors.horizontalCenter: parent.horizontalCenter; icon: "go-down"; onClicked: field.step(-1) }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: field.label !== ""
            text: field.label
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
        }
    }
    // One line of text in a box.
    component TextBox: Rectangle {
        id: box
        property alias text: input.text
        property string placeholder
        signal accepted()
        height: 32
        radius: Theme.radius
        color: Theme.control
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
            onAccepted: box.accepted()
            Text {
                visible: input.text === ""
                anchors.verticalCenter: parent.verticalCenter
                text: box.placeholder
                color: Theme.textTertiary
                font.pixelSize: Theme.fontBody
            }
        }
        function focusInput() { input.forceActiveFocus() }
    }
    // The round buttons of a timer and the stopwatch.
    component RoundButton: Rectangle {
        id: round
        property string icon
        property bool usable: true
        signal clicked()
        width: 44
        height: 44
        radius: 22
        opacity: usable ? 1 : 0.4
        color: roundMouse.containsMouse && usable ? Theme.controlHover : Theme.control
        border.width: 1
        border.color: Theme.controlStroke
        Image {
            anchors.centerIn: parent
            width: 18
            height: 18
            source: Theme.icons + round.icon
            sourceSize: Qt.size(width, height)
        }
        MouseArea {
            id: roundMouse
            anchors.fill: parent
            hoverEnabled: true
            enabled: round.usable
            onClicked: round.clicked()
        }
    }

    // --- the title bar ---
    Item {
        id: strip
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 44

        DragHandler {
            target: null
            onActiveChanged: if (active) root.startSystemMove()
        }
        TapHandler { onDoubleTapped: root.toggleMaximized() }

        Image {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            source: "image://appicon/preferences-system-time"
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
                Rectangle {
                    visible: entry.current
                    anchors.verticalCenter: parent.verticalCenter
                    width: 3
                    height: 16
                    radius: 1.5
                    color: Theme.accentFill
                }
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

    // --- the page ---
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
                    visible: root.page === "timer"
                    text: qsTr("1 min")
                    onClicked: Clock.timerAdd("", 60000, true)
                }
                ToolButton {
                    visible: root.page === "timer"
                    text: qsTr("5 min")
                    onClicked: Clock.timerAdd("", 300000, true)
                }
                ToolButton {
                    visible: root.page === "timer"
                    text: qsTr("10 min")
                    onClicked: Clock.timerAdd("", 600000, true)
                }
                ToolButton {
                    visible: root.page === "timer"
                    icon: "list-add"
                    text: qsTr("New timer")
                    onClicked: timerDialog.ask(-1, "", 300000)
                }
                ToolButton {
                    visible: root.page === "alarm"
                    icon: "list-add"
                    text: qsTr("New alarm")
                    onClicked: alarmDialog.ask(null)
                }
                ToolButton {
                    visible: root.page === "world"
                    icon: "list-add"
                    text: qsTr("Add city")
                    onClicked: cityDialog.ask()
                }
            }
        }

        // --- Timer ---
        Flickable {
            id: timerPage
            anchors { left: parent.left; right: parent.right; top: bar.bottom; bottom: parent.bottom }
            visible: root.page === "timer"
            clip: true
            contentHeight: timerFlow.height + 40
            Flow {
                id: timerFlow
                x: 20
                y: 8
                width: timerPage.width - 40
                spacing: 12
                Repeater {
                    model: Clock.timers
                    delegate: Rectangle {
                        id: card
                        required property var modelData
                        readonly property real remainingMs: Clock.timerRemaining(modelData.id, Clock.now)
                        width: 260
                        height: 290
                        radius: Theme.radiusOverlay
                        color: Theme.control
                        border.width: 1
                        border.color: Theme.controlStroke
                        Text {
                            x: 16
                            y: 14
                            width: parent.width - 32
                            text: card.modelData.name !== "" ? card.modelData.name : qsTr("Timer")
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Shape {
                            x: 50
                            y: 46
                            width: 160
                            height: 160
                            preferredRendererType: Shape.CurveRenderer
                            ShapePath {
                                strokeColor: Theme.controlStrokeStrong
                                strokeWidth: 6
                                fillColor: "transparent"
                                PathAngleArc { centerX: 80; centerY: 80; radiusX: 74; radiusY: 74; startAngle: 0; sweepAngle: 360 }
                            }
                            ShapePath {
                                strokeColor: Theme.accentFill
                                strokeWidth: 6
                                capStyle: ShapePath.RoundCap
                                fillColor: "transparent"
                                PathAngleArc {
                                    centerX: 80; centerY: 80; radiusX: 74; radiusY: 74
                                    startAngle: -90
                                    sweepAngle: 360 * card.remainingMs / card.modelData.durationMs
                                }
                            }
                        }
                        Text {
                            x: 50
                            y: 46
                            width: 160
                            height: 160
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            text: root.countdown(card.remainingMs)
                            color: Theme.text
                            font.pixelSize: 28
                        }
                        Row {
                            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 220 }
                            spacing: 12
                            RoundButton {
                                icon: card.modelData.running ? "media-playback-pause" : "media-playback-start"
                                onClicked: card.modelData.running ? Clock.timerPause(card.modelData.id) : Clock.timerStart(card.modelData.id)
                            }
                            RoundButton {
                                icon: "view-refresh"
                                usable: card.modelData.running || card.modelData.paused
                                onClicked: Clock.timerReset(card.modelData.id)
                            }
                        }
                        Row {
                            anchors { right: parent.right; rightMargin: 8; top: parent.top; topMargin: 8 }
                            ToolButton {
                                icon: "document-edit"
                                tooltip: qsTr("Edit")
                                onClicked: timerDialog.ask(card.modelData.id, card.modelData.name, card.modelData.durationMs)
                            }
                            ToolButton {
                                icon: "edit-delete"
                                tooltip: qsTr("Delete")
                                onClicked: Clock.timerRemove(card.modelData.id)
                            }
                        }
                    }
                }
            }
            Text {
                visible: Clock.timers.length === 0
                anchors.centerIn: parent
                text: qsTr("No timers yet. Start one with 1, 5 or 10 minutes, or make your own.")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
            }
        }

        // --- Alarm ---
        ListView {
            id: alarmList
            anchors { left: parent.left; right: parent.right; top: bar.bottom; bottom: parent.bottom; leftMargin: 20; rightMargin: 20 }
            visible: root.page === "alarm"
            clip: true
            spacing: 4
            model: Clock.alarms
            delegate: Rectangle {
                id: alarmRow
                required property var modelData
                width: ListView.view.width
                height: 72
                radius: Theme.radius
                color: rowMouse.containsMouse ? Theme.controlHover : Theme.control
                border.width: 1
                border.color: Theme.controlStroke
                opacity: modelData.enabled ? 1 : 0.6
                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: alarmDialog.ask(alarmRow.modelData)
                }
                Text {
                    x: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.clockText(alarmRow.modelData.hour, alarmRow.modelData.minute)
                    color: Theme.text
                    font.pixelSize: 28
                }
                Column {
                    anchors { left: parent.left; leftMargin: 190; right: toggle.left; rightMargin: 12; verticalCenter: parent.verticalCenter }
                    Text {
                        width: parent.width
                        text: alarmRow.modelData.name !== "" ? alarmRow.modelData.name : qsTr("Alarm")
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: {
                            const eta = root.etaText(Clock.alarmEta(alarmRow.modelData.id, Clock.wall))
                            const days = root.daysText(alarmRow.modelData.days)
                            return eta !== "" ? days + " · " + eta : days
                        }
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideRight
                    }
                }
                Toggle {
                    id: toggle
                    anchors { right: trash.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
                    showLabel: false
                    checked: alarmRow.modelData.enabled
                    onToggled: checked => Clock.alarmSetEnabled(alarmRow.modelData.id, checked)
                }
                ToolButton {
                    id: trash
                    anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                    icon: "edit-delete"
                    tooltip: qsTr("Delete")
                    onClicked: Clock.alarmRemove(alarmRow.modelData.id)
                }
            }
            Text {
                visible: Clock.alarms.length === 0
                anchors.centerIn: parent
                text: qsTr("No alarms yet.")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
            }
        }

        // --- Stopwatch ---
        Item {
            id: stopwatchPage
            anchors { left: parent.left; right: parent.right; top: bar.bottom; bottom: parent.bottom }
            visible: root.page === "stopwatch"
            Text {
                id: elapsed
                anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 24 }
                text: root.stopwatch(Clock.swElapsed(Clock.now))
                color: Theme.text
                font.pixelSize: 56
                font.weight: Font.Light
            }
            Row {
                id: swButtons
                anchors { horizontalCenter: parent.horizontalCenter; top: elapsed.bottom; topMargin: 20 }
                spacing: 12
                RoundButton {
                    icon: Clock.swRunning ? "media-playback-pause" : "media-playback-start"
                    onClicked: Clock.swRunning ? Clock.swPause() : Clock.swStart()
                }
                RoundButton {
                    icon: "flag"
                    usable: Clock.swRunning
                    onClicked: Clock.swLap()
                }
                RoundButton {
                    icon: "view-refresh"
                    usable: Clock.swRunning || Clock.swElapsed(Clock.now) > 0
                    onClicked: Clock.swReset()
                }
            }
            Row {
                id: lapHead
                visible: Clock.laps.length > 0
                anchors { left: parent.left; right: parent.right; top: swButtons.bottom; topMargin: 24; leftMargin: 40; rightMargin: 40 }
                height: 28
                Repeater {
                    model: [qsTr("Lap"), qsTr("Split"), qsTr("Total")]
                    delegate: Text {
                        required property string modelData
                        width: lapHead.width / 3
                        height: lapHead.height
                        verticalAlignment: Text.AlignVCenter
                        text: modelData
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                    }
                }
            }
            ListView {
                anchors { left: parent.left; right: parent.right; top: lapHead.bottom; bottom: parent.bottom; leftMargin: 40; rightMargin: 40 }
                clip: true
                model: Clock.laps
                delegate: Row {
                    id: lapRow
                    required property var modelData
                    width: ListView.view.width
                    height: 36
                    Repeater {
                        model: [lapRow.modelData.number, root.stopwatch(lapRow.modelData.split), root.stopwatch(lapRow.modelData.total)]
                        delegate: Text {
                            required property var modelData
                            width: lapRow.width / 3
                            height: lapRow.height
                            verticalAlignment: Text.AlignVCenter
                            text: modelData
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                        }
                    }
                }
            }
        }

        // --- World clock ---
        ListView {
            id: worldList
            anchors { left: parent.left; right: parent.right; top: bar.bottom; bottom: parent.bottom; leftMargin: 20; rightMargin: 20 }
            visible: root.page === "world"
            clip: true
            spacing: 4
            model: [Clock.localZone].concat(Clock.zones)
            delegate: Rectangle {
                id: zoneRow
                required property string modelData
                required property int index
                readonly property var info: Clock.zoneInfo(modelData, Clock.wall)
                width: ListView.view.width
                height: 72
                radius: Theme.radius
                color: Theme.control
                border.width: 1
                border.color: Theme.controlStroke
                Rectangle {
                    x: 16
                    anchors.verticalCenter: parent.verticalCenter
                    width: 12
                    height: 12
                    radius: 6
                    color: zoneRow.info.night ? "#6b78e0" : "#f2b705"
                }
                Column {
                    anchors { left: parent.left; leftMargin: 44; right: clockLabel.left; rightMargin: 12; verticalCenter: parent.verticalCenter }
                    Text {
                        width: parent.width
                        text: zoneRow.index === 0 ? qsTr("%1 (local)").arg(zoneRow.info.city) : zoneRow.info.city
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: [root.dayText(zoneRow.info.dayDelta), root.offsetText(zoneRow.info.offset),
                               zoneRow.info.night ? qsTr("Night") : qsTr("Day")].join(" · ")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideRight
                    }
                }
                Text {
                    id: clockLabel
                    anchors { right: zoneRow.index === 0 ? parent.right : zoneTrash.left; rightMargin: zoneRow.index === 0 ? 16 : 8; verticalCenter: parent.verticalCenter }
                    text: root.clockText(zoneRow.info.hour, zoneRow.info.minute)
                    color: Theme.text
                    font.pixelSize: 24
                }
                ToolButton {
                    id: zoneTrash
                    visible: zoneRow.index > 0
                    anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                    icon: "edit-delete"
                    tooltip: qsTr("Remove")
                    onClicked: Clock.zoneRemove(zoneRow.modelData)
                }
            }
        }

        // --- Settings ---
        Column {
            anchors { left: parent.left; right: parent.right; top: bar.bottom; leftMargin: 20; rightMargin: 20 }
            visible: root.page === "settings"
            spacing: 4
            component SettingRow: Rectangle {
                default property alias content: holder.data
                property string title
                width: parent.width
                height: 64
                radius: Theme.radius
                color: Theme.control
                border.width: 1
                border.color: Theme.controlStroke
                Text {
                    x: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: parent.title
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                }
                Item {
                    id: holder
                    anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
                    width: 220
                    height: 40
                }
            }
            SettingRow {
                title: qsTr("24-hour clock")
                Toggle {
                    anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                    checked: Clock.use24h
                    onToggled: checked => Clock.use24h = checked
                }
            }
            SettingRow {
                title: qsTr("Alarm sound volume")
                Slider {
                    anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                    width: 170
                    from: 0
                    to: 100
                    stepSize: 5
                    value: Clock.volume
                    onMoved: value => Clock.volume = value
                }
                Text {
                    anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                    text: Clock.volume + "%"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontBody
                }
            }
            SettingRow {
                title: qsTr("Snooze for new alarms")
                Row {
                    anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                    spacing: 8
                    ToolButton { icon: "list-remove"; onClicked: Clock.snoozeMinutes -= 1 }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("%1 min").arg(Clock.snoozeMinutes)
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                    }
                    ToolButton { icon: "list-add"; onClicked: Clock.snoozeMinutes += 1 }
                }
            }
        }

        // What is ringing, with what to do about it.
        Column {
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
            z: 50
            spacing: 6
            Repeater {
                model: Clock.ringing
                delegate: Rectangle {
                    id: ring
                    required property var modelData
                    width: parent.width
                    height: 56
                    radius: Theme.radius
                    color: Theme.accentFill
                    Column {
                        anchors { left: parent.left; leftMargin: 16; right: ringButtons.left; verticalCenter: parent.verticalCenter }
                        Text {
                            width: parent.width
                            text: ring.modelData.title
                            color: Theme.accentText
                            font.pixelSize: Theme.fontBody
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: ring.modelData.body
                            color: Theme.accentText
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideRight
                        }
                    }
                    Row {
                        id: ringButtons
                        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        spacing: 8
                        ToolButton {
                            visible: ring.modelData.snoozable
                            text: qsTr("Snooze")
                            onClicked: Clock.ringSnooze(ring.modelData.key)
                        }
                        ToolButton {
                            text: qsTr("Dismiss")
                            onClicked: Clock.ringDismiss(ring.modelData.key)
                        }
                    }
                }
            }
        }
    }

    // --- dialogs ---
    Dialog {
        id: timerDialog
        property int editId: -1
        title: editId < 0 ? qsTr("New timer") : qsTr("Edit timer")
        buttons: [{ text: qsTr("Save"), accent: true, value: "ok" }, { text: qsTr("Cancel"), value: "cancel" }]
        onChosen: value => {
            if (value !== "ok") return
            const ms = ((hours.value * 60 + minutes.value) * 60 + seconds.value) * 1000
            if (editId < 0) Clock.timerAdd(timerName.text, ms, true)
            else Clock.timerEdit(editId, timerName.text, ms)
        }
        function ask(id, name, ms) {
            editId = id
            timerName.text = name
            const s = Math.round(ms / 1000)
            hours.value = Math.floor(s / 3600)
            minutes.value = Math.floor(s % 3600 / 60)
            seconds.value = s % 60
            open()
        }
        Column {
            width: parent.width
            spacing: 16
            TextBox { id: timerName; width: parent.width; placeholder: qsTr("Timer name") }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 16
                NumberField { id: hours; to: 23; label: qsTr("hours") }
                NumberField { id: minutes; label: qsTr("minutes") }
                NumberField { id: seconds; label: qsTr("seconds") }
            }
        }
    }

    Dialog {
        id: alarmDialog
        property int editId: 0
        title: editId === 0 ? qsTr("New alarm") : qsTr("Edit alarm")
        buttons: [{ text: qsTr("Save"), accent: true, value: "ok" }, { text: qsTr("Cancel"), value: "cancel" }]
        onChosen: value => {
            if (value !== "ok") return
            let mask = 0
            for (let i = 0; i < 7; ++i) if (dayChips.itemAt(i).on) mask |= 1 << i
            Clock.alarmSave(editId, alarmHour.value, alarmMinute.value, alarmName.text, mask, alarmSnooze.value)
        }
        // `alarm`: the map of the alarm to edit, null for a new one.
        function ask(alarm) {
            editId = alarm ? alarm.id : 0
            const now = new Date()
            alarmHour.value = alarm ? alarm.hour : now.getHours()
            alarmMinute.value = alarm ? alarm.minute : now.getMinutes()
            alarmName.text = alarm ? alarm.name : ""
            alarmSnooze.value = alarm ? alarm.snooze : Clock.snoozeMinutes
            for (let i = 0; i < 7; ++i) dayChips.itemAt(i).on = alarm ? (alarm.days & (1 << i)) !== 0 : false
            open()
        }
        Column {
            width: parent.width
            spacing: 16
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 16
                NumberField { id: alarmHour; to: 23; label: qsTr("hour") }
                NumberField { id: alarmMinute; label: qsTr("minute") }
                NumberField { id: alarmSnooze; from: 1; to: 60; wrap: false; label: qsTr("snooze (min)") }
            }
            TextBox { id: alarmName; width: parent.width; placeholder: qsTr("Alarm name") }
            Row {
                spacing: 4
                Repeater {
                    id: dayChips
                    model: 7
                    delegate: Rectangle {
                        id: chip
                        required property int index
                        property bool on: false
                        width: 44
                        height: 32
                        radius: Theme.radius
                        color: on ? Theme.accentFill : chipMouse.containsMouse ? Theme.controlHover : Theme.control
                        border.width: 1
                        border.color: Theme.controlStroke
                        Text {
                            anchors.centerIn: parent
                            text: root.dayName(chip.index, Locale.ShortFormat)
                            color: chip.on ? Theme.accentText : Theme.text
                            font.pixelSize: Theme.fontCaption
                        }
                        MouseArea {
                            id: chipMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: chip.on = !chip.on
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: cityDialog
        title: qsTr("Add a city")
        buttons: [{ text: qsTr("Close"), value: "cancel" }]
        function ask() {
            citySearch.text = ""
            open()
            citySearch.focusInput()
        }
        Column {
            width: parent.width
            spacing: 8
            TextBox { id: citySearch; width: parent.width; placeholder: qsTr("Search for a city") }
            ListView {
                width: parent.width
                height: 220
                clip: true
                model: Clock.zoneSearch(citySearch.text)
                delegate: Rectangle {
                    id: hit
                    required property var modelData
                    width: ListView.view.width
                    height: 36
                    radius: Theme.radius
                    color: hitMouse.containsMouse ? Theme.subtleHover : "transparent"
                    Text {
                        anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                        text: hit.modelData.city
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                    }
                    Text {
                        anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                        text: hit.modelData.region
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                    }
                    MouseArea {
                        id: hitMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            Clock.zoneAdd(hit.modelData.id)
                            cityDialog.close("cancel")
                        }
                    }
                }
            }
        }
    }

    ResizeEdges {}
}
