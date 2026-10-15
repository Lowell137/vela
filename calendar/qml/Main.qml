// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtCore
import Vela.Controls
import "Dates.js" as D

// The Calendar's window, the one of Windows 11's: its own title bar, the
// toolbar (new event, today, back and forward, the views), a month at the
// left and the chosen view. Events come from Cal (src/store.h).
Window {
    id: root
    flags: Qt.Window | Qt.FramelessWindowHint // no title bar from Vela
    width: 1180
    height: 760
    minimumWidth: 560
    minimumHeight: 460
    visible: true
    color: Theme.background
    title: qsTr("Calendar")

    onActiveChanged: Theme.windowActive = active
    // Without a window the process stays only while reminders are pending.
    onVisibleChanged: Cal.setWindowVisible(visible)

    readonly property bool maximized: visibility === Window.Maximized
    function toggleMaximized() { maximized ? showNormal() : showMaximized() }

    Settings {
        id: settings
        property string view: "month"
        property bool weekNumbers: false
        property int defaultReminder: 15
    }
    property alias view: settings.view
    property alias weekNumbers: settings.weekNumbers
    property alias defaultReminder: settings.defaultReminder
    property alias menuLayer: menuLayer

    // --- what is shown ---
    property date current: D.startOfDay(new Date())
    readonly property int firstDay: Qt.locale().firstDayOfWeek
    readonly property bool wide: width >= 880
    readonly property int shownDays: view === "day" ? 1 : view === "workweek" ? 5 : 7
    // The first day of the grid shown and how many days it covers.
    readonly property date rangeStart: {
        if (view === "day") return current
        if (view === "workweek") return D.weekStart(current, 1) // Monday: shortcut, the work days aren't set by locale
        if (view === "week") return D.weekStart(current, firstDay)
        return D.weekStart(new Date(current.getFullYear(), current.getMonth(), 1), firstDay)
    }
    readonly property int rangeDays: view === "month" ? 42 : shownDays
    readonly property var occs: {
        Cal.revision
        const a = rangeStart
        const b = D.addDays(rangeStart, rangeDays - 1)
        return Cal.occurrences(a.getFullYear(), a.getMonth() + 1, a.getDate(), b.getFullYear(), b.getMonth() + 1, b.getDate())
    }
    readonly property string heading: {
        const loc = Qt.locale()
        if (view === "month") return loc.monthName(current.getMonth(), Locale.LongFormat) + " " + current.getFullYear()
        if (view === "day") return Qt.formatDate(current, Locale.LongFormat)
        const a = rangeStart
        const b = D.addDays(a, shownDays - 1)
        if (a.getMonth() === b.getMonth()) return a.getDate() + " – " + b.getDate() + " " + loc.monthName(a.getMonth(), Locale.LongFormat) + " " + a.getFullYear()
        return a.getDate() + " " + loc.monthName(a.getMonth(), Locale.ShortFormat) + " – " + b.getDate() + " " + loc.monthName(b.getMonth(), Locale.ShortFormat) + " " + b.getFullYear()
    }
    function step(direction) {
        if (view === "month") current = D.addMonths(current, direction)
        else current = D.addDays(current, direction * (view === "day" ? 1 : 7))
    }
    function showDay(day) {
        current = D.startOfDay(day)
        view = "day"
    }

    // A reminder fired or a second start asked for the window.
    Connections {
        target: Cal
        function onShowRequested() {
            root.show()
            root.raise()
            root.requestActivate()
        }
        function onImportRequested(path) { root.askImport(path) }
    }
    Component.onCompleted: if (StartFile !== "") askImport(StartFile)

    // --- import and export ---
    property string importPath
    function askImport(path) {
        const n = Cal.icsCount(path)
        importPath = path
        if (n < 0) {
            notice.text = qsTr("This file could not be read.")
            notice.open()
        } else if (n === 0) {
            notice.text = qsTr("There are no events in this file.")
            notice.open()
        } else {
            importDialog.text = qsTr("This file has %n event(s). Add them to the calendar?", "", n)
            importDialog.open()
        }
    }
    function pickFile(save, callback) {
        if (!pickerLoader.item) {
            const component = Qt.createComponent("FilePicker.qml")
            pickerLoader.item = component.createObject(root)
        }
        pickerLoader.item.pick(save, callback)
    }
    QtObject { id: pickerLoader; property var item: null }

    Shortcut { sequence: "Ctrl+N"; onActivated: root.newEvent() }
    Shortcut { sequence: "Ctrl+G"; onActivated: root.goTo() }
    Shortcut { sequence: "Ctrl+T"; onActivated: root.current = D.startOfDay(new Date()) }
    Shortcut { sequence: "Ctrl+1"; onActivated: root.view = "day" }
    Shortcut { sequence: "Ctrl+2"; onActivated: root.view = "workweek" }
    Shortcut { sequence: "Ctrl+3"; onActivated: root.view = "week" }
    Shortcut { sequence: "Ctrl+4"; onActivated: root.view = "month" }
    Shortcut { sequence: "Ctrl+Left"; onActivated: root.step(-1) }
    Shortcut { sequence: "Ctrl+Right"; onActivated: root.step(1) }

    function newEvent(ms) {
        if (ms === undefined) {
            const now = new Date()
            const day = D.sameDay(current, now) ? now : new Date(current.getFullYear(), current.getMonth(), current.getDate(), 9)
            ms = new Date(day.getFullYear(), day.getMonth(), day.getDate(), day.getHours() + 1).getTime()
        }
        editor.openNew(ms, false)
    }
    function goTo() {
        gotoPicker.selected = current
        gotoDialog.open()
    }
    function mainMenu(x, y) {
        menuLayer.open([
            { text: qsTr("&Go to date..."), shortcut: "Ctrl+G", icon: "go-jump", action: () => root.goTo() },
            { separator: true },
            { text: qsTr("&Import events..."), icon: "document-open", action: () => root.pickFile(false, url => root.askImport(url)) },
            { text: qsTr("&Export events..."), icon: "document-save-as", action: () => root.pickFile(true, url => {
                if (!Cal.exportIcs(url)) {
                    notice.text = qsTr("The file could not be written.")
                    notice.open()
                }
            }) }
        ], x, y, {})
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
            source: "image://appicon/calendar"
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

    // --- the toolbar ---
    Item {
        id: bar
        anchors { left: parent.left; right: parent.right; top: strip.bottom; leftMargin: 8; rightMargin: 8 }
        height: 44

        Row {
            id: leftTools
            spacing: 4
            anchors.verticalCenter: parent.verticalCenter
            Rectangle {
                width: newRow.width + 24
                height: 32
                radius: Theme.radius
                color: newMouse.containsMouse ? Theme.accentFillHover : Theme.accentFill
                Row {
                    id: newRow
                    anchors.centerIn: parent
                    spacing: 8
                    Image {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        source: Theme.iconsOnAccent + "list-add"
                        sourceSize: Qt.size(width, height)
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("New event")
                        color: Theme.accentText
                        font.pixelSize: Theme.fontBody
                    }
                }
                MouseArea { id: newMouse; anchors.fill: parent; hoverEnabled: true; onClicked: root.newEvent() }
            }
            ToolButton { text: qsTr("Today"); onClicked: root.current = D.startOfDay(new Date()) }
            ToolButton { icon: "go-previous"; tooltip: qsTr("Previous (Ctrl+Left)"); onClicked: root.step(-1) }
            ToolButton { icon: "go-next"; tooltip: qsTr("Next (Ctrl+Right)"); onClicked: root.step(1) }
            ToolButton {
                text: root.heading
                tooltip: qsTr("Go to date (Ctrl+G)")
                onClicked: root.goTo()
            }
        }
        Row {
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            spacing: 2
            Repeater {
                model: [
                    { id: "day", name: qsTr("Day") }, { id: "workweek", name: qsTr("Work week") },
                    { id: "week", name: qsTr("Week") }, { id: "month", name: qsTr("Month") }
                ]
                delegate: ToolButton {
                    required property var modelData
                    text: modelData.name
                    checked: root.view === modelData.id
                    onClicked: root.view = modelData.id
                }
            }
            ToolButton {
                icon: "configure"
                tooltip: qsTr("Settings")
                onClicked: settingsLoader.active = true
            }
            ToolButton {
                id: moreButton
                icon: "application-menu"
                tooltip: qsTr("More")
                onClicked: { const p = mapToItem(null, 0, height); root.mainMenu(p.x, p.y) }
            }
        }
    }

    // --- the mini month and the view ---
    MiniMonth {
        id: side
        visible: root.wide
        anchors { left: parent.left; top: bar.bottom; leftMargin: 8; topMargin: 4 }
        width: 252
        selected: root.current
        rangeStart: root.rangeStart
        rangeDays: root.view === "month" ? 0 : root.shownDays
        firstDay: root.firstDay
        weekNumbers: root.weekNumbers
        onPicked: day => root.current = D.startOfDay(day)
    }

    Rectangle {
        id: content
        anchors { left: root.wide ? side.right : parent.left; right: parent.right; top: bar.bottom; bottom: parent.bottom; leftMargin: 8; topMargin: 4 }
        color: Theme.layer
        topLeftRadius: Theme.radiusCard
        border.width: 1
        border.color: Theme.layerStroke
        clip: true

        MonthView {
            anchors.fill: parent
            visible: root.view === "month"
            gridStart: root.rangeStart
            shownMonth: root.current.getMonth()
            occs: root.occs
            weekNumbers: root.weekNumbers
            onSlotClicked: ms => editor.openNew(ms, false)
            onEventClicked: (id, day) => editor.openEvent(id, day)
            onDayClicked: day => root.showDay(day)
        }
        TimeGrid {
            id: grid
            anchors.fill: parent
            visible: root.view !== "month"
            start: root.rangeStart
            days: root.shownDays
            occs: root.occs
            weekNumbers: root.weekNumbers && root.view !== "day"
            onSlotClicked: ms => editor.openNew(ms, false)
            onEventClicked: (id, day) => editor.openEvent(id, day)
            onDayClicked: day => root.showDay(day)
        }
    }

    // --- the event's flyout, at the right ---
    EventEditor {
        id: editor
        win: root
        z: 20
        anchors { right: content.right; top: content.top; bottom: content.bottom; margins: 10 }
        onDeleteRequested: (id, day, repeats) => {
            if (!repeats) {
                Cal.remove(id)
                return
            }
            deleteDialog.eventId = id
            deleteDialog.day = day
            deleteDialog.open()
        }
    }

    MenuLayer {
        id: menuLayer
        anchors.fill: parent
        z: 100
    }

    // --- dialogs ---
    Dialog {
        id: notice
        title: qsTr("Calendar")
    }
    Dialog {
        id: importDialog
        title: qsTr("Import events")
        buttons: [{ text: qsTr("Import"), accent: true, value: "ok" }, { text: qsTr("Cancel"), value: "cancel" }]
        onChosen: value => {
            if (value === "ok" && Cal.importIcs(root.importPath) < 0) {
                notice.text = qsTr("This file could not be read.")
                notice.open()
            }
        }
    }
    Dialog {
        id: deleteDialog
        property string eventId
        property string day
        title: qsTr("Delete a repeating event")
        text: qsTr("Delete only this event, or every event of the series?")
        buttons: [{ text: qsTr("This event"), value: "one" }, { text: qsTr("All events"), accent: true, value: "all" },
                  { text: qsTr("Cancel"), value: "cancel" }]
        onChosen: value => {
            if (value === "one") Cal.removeOccurrence(eventId, day)
            else if (value === "all") Cal.remove(eventId)
        }
    }
    Dialog {
        id: gotoDialog
        title: qsTr("Go to date")
        buttons: [{ text: qsTr("Go"), accent: true, value: "ok" }, { text: qsTr("Cancel"), value: "cancel" }]
        dialogWidth: 330
        onChosen: value => { if (value === "ok") root.current = D.startOfDay(gotoPicker.selected) }
        MiniMonth {
            id: gotoPicker
            width: 252
            weekNumbers: false
            onPicked: day => selected = day
        }
    }

    // Settings, over the window; loaded when first opened.
    Loader {
        id: settingsLoader
        anchors { left: parent.left; right: parent.right; top: strip.bottom; bottom: parent.bottom }
        z: 50
        active: false
        source: "SettingsPage.qml"
        onLoaded: {
            item.win = root
            item.forceActiveFocus()
            item.closed.connect(() => active = false)
        }
    }

    ResizeEdges {}
}
