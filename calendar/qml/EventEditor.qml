// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls
import "Dates.js" as D

// The flyout for one event: a new one at a time, or an existing one's
// details. Save writes it to the store; Delete asks the window (repeats ask
// "this event or all"). Times are typed as HH:mm.
Rectangle {
    id: editor
    property var win // Main.qml: the menu layer, the defaults
    property string eventId // empty: a new event
    property string occurrenceDay // the day of the one that was clicked, yyyy-MM-dd
    property bool repeats: false
    signal deleteRequested(string id, string day, bool repeats)

    visible: false
    width: 340
    radius: Theme.radiusOverlay
    color: Theme.flyout
    border.width: 1
    border.color: Theme.stroke

    // --- the form's state ---
    property string title
    property string location
    property string notes
    property bool allDay: false
    property date startDay: new Date()
    property int startMinutes: 540
    property date endDay: new Date()
    property int endMinutes: 600
    property int eventColor: 0
    property int freq: 0 // Store: 0 none, 1 daily, 2 weekly, 3 monthly, 4 yearly
    property int interval: 1
    property int weekdays: 0
    property int endMode: 0 // 0 never, 1 on a date, 2 after a count
    property date untilDay: new Date()
    property int count: 10
    property int reminder: -1

    readonly property var freqNames: [qsTr("Never"), qsTr("Daily"), qsTr("Weekly"), qsTr("Monthly"), qsTr("Yearly")]
    readonly property var unitNames: ["", qsTr("days"), qsTr("weeks"), qsTr("months"), qsTr("years")]
    readonly property var reminderValues: [-1, 0, 5, 15, 30, 60, 1440]
    readonly property var reminderNames: [qsTr("None"), qsTr("At the time of the event"), qsTr("5 minutes before"),
        qsTr("15 minutes before"), qsTr("30 minutes before"), qsTr("1 hour before"), qsTr("1 day before")]

    function stamp(day, minutes) { return new Date(day.getFullYear(), day.getMonth(), day.getDate(), 0, minutes).getTime() }
    function clock(minutes) {
        const m = ((minutes % 1440) + 1440) % 1440
        return String(Math.floor(m / 60)).padStart(2, "0") + ":" + String(m % 60).padStart(2, "0")
    }
    function parseClock(text, fallback) {
        const m = /^\s*(\d{1,2})[:.]?(\d{2})?\s*$/.exec(text)
        if (!m) return fallback
        const h = Number(m[1]), min = Number(m[2] || 0)
        return h < 24 && min < 60 ? h * 60 + min : fallback
    }
    function weekdayName(i) { return Qt.locale().dayName((i + 1) % 7, Locale.ShortFormat) } // i 0 is Monday

    function openNew(ms, wholeDay) {
        eventId = ""
        occurrenceDay = ""
        repeats = false
        const d = new Date(ms)
        title = ""
        location = ""
        notes = ""
        allDay = !!wholeDay
        startDay = D.startOfDay(d)
        startMinutes = D.minutesOfDay(ms)
        endDay = startDay
        endMinutes = startMinutes + 60
        if (endMinutes >= 1440) {
            endDay = D.addDays(startDay, 1)
            endMinutes -= 1440
        }
        eventColor = 0
        freq = 0
        interval = 1
        weekdays = 0
        endMode = 0
        untilDay = D.addDays(startDay, 30)
        count = 10
        reminder = win ? win.defaultReminder : -1
        show()
    }
    function openEvent(id, day) {
        const e = Cal.event(id)
        if (!e.id) return
        eventId = id
        occurrenceDay = day
        title = e.title
        location = e.location
        notes = e.notes
        allDay = e.allDay
        startDay = D.startOfDay(new Date(e.startMs))
        startMinutes = D.minutesOfDay(e.startMs)
        endDay = D.startOfDay(new Date(e.endMs))
        endMinutes = D.minutesOfDay(e.endMs)
        eventColor = e.color
        freq = e.freq
        repeats = e.freq !== 0
        interval = e.interval
        weekdays = e.weekdays
        count = e.count > 0 ? e.count : 10
        endMode = e.count > 0 ? 2 : e.untilMs > 0 ? 1 : 0
        untilDay = e.untilMs > 0 ? D.startOfDay(new Date(e.untilMs)) : D.addDays(startDay, 30)
        reminder = e.reminder
        show()
    }
    function show() {
        datePopup.visible = false
        visible = true
        syncTimes()
        titleInput.input.forceActiveFocus()
        if (eventId === "") titleInput.input.selectAll()
    }
    function save() {
        commitTimes()
        Cal.save({
            id: eventId, title: title.trim(), location: location.trim(), notes: notes,
            allDay: allDay, startMs: stamp(startDay, allDay ? 0 : startMinutes), endMs: stamp(endDay, allDay ? 0 : endMinutes),
            color: eventColor, freq: freq, interval: interval, weekdays: weekdays,
            count: freq !== 0 && endMode === 2 ? count : 0,
            untilMs: freq !== 0 && endMode === 1 ? stamp(untilDay, 0) : 0,
            reminder: reminder
        })
        visible = false
    }
    // The time fields hold what's typed, so they're written, not bound.
    function syncTimes() {
        startTime.text = clock(startMinutes)
        endTime.text = clock(endMinutes)
    }
    onStartMinutesChanged: syncTimes()
    onEndMinutesChanged: syncTimes()
    function commitTimes() {
        startMinutes = parseClock(startTime.text, startMinutes)
        endMinutes = parseClock(endTime.text, endMinutes)
    }
    // Moving the start moves the end with it, as Windows does.
    function setStart(day, minutes) {
        const delta = stamp(day, minutes) - stamp(startDay, startMinutes)
        const end = new Date(stamp(endDay, endMinutes) + delta)
        startDay = day
        startMinutes = minutes
        endDay = D.startOfDay(end)
        endMinutes = D.minutesOfDay(end.getTime())
    }

    Keys.onEscapePressed: visible = false

    // A button that opens a menu of choices.
    component Choice: Rectangle {
        id: choice
        property string text
        signal clicked()
        implicitHeight: 32
        radius: Theme.radius
        color: choiceMouse.containsMouse ? Theme.controlHover : Theme.control
        border.width: 1
        border.color: Theme.controlStroke
        Text {
            anchors { fill: parent; leftMargin: 10; rightMargin: 28 }
            verticalAlignment: Text.AlignVCenter
            text: choice.text
            elide: Text.ElideRight
            color: Theme.text
            font.pixelSize: Theme.fontBody
        }
        Image {
            anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
            width: 12
            height: 12
            source: Theme.icons + "go-down"
            sourceSize: Qt.size(width, height)
        }
        MouseArea { id: choiceMouse; anchors.fill: parent; hoverEnabled: true; onClicked: choice.clicked() }
    }
    component Label: Text {
        color: Theme.textSecondary
        font.pixelSize: Theme.fontCaption
    }
    component Input: Rectangle {
        id: box
        property alias text: field.text
        property alias input: field
        property string placeholder
        signal edited()
        implicitHeight: 32
        radius: Theme.radius
        color: field.activeFocus ? Theme.field : Theme.control
        border.width: 1
        border.color: field.activeFocus ? Theme.accentFill : Theme.controlStroke
        TextInput {
            id: field
            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
            verticalAlignment: TextInput.AlignVCenter
            clip: true
            color: Theme.text
            selectionColor: Theme.accent
            selectedTextColor: "white"
            font.pixelSize: Theme.fontBody
            onTextEdited: box.edited()
            Text {
                visible: field.text === "" && !field.preeditText
                anchors.verticalCenter: parent.verticalCenter
                text: box.placeholder
                color: Theme.textTertiary
                font.pixelSize: Theme.fontBody
            }
        }
    }
    // A date: opens the month under it.
    component DateField: Choice {
        id: dateField
        property date day
        property string target
        text: Qt.formatDate(day, Locale.ShortFormat)
        onClicked: {
            editor.commitTimes()
            const p = dateField.mapToItem(editor, 0, dateField.height + 4)
            datePopup.target = target
            datePopup.x = Math.min(editor.width - datePopup.width - 8, Math.max(8, p.x))
            datePopup.y = p.y
            mini.selected = dateField.day
            datePopup.visible = true
        }
    }
    component Stepper: Row {
        id: stepper
        property int value
        property int minimum: 1
        property int maximum: 999
        signal changed(int value)
        spacing: 2
        ToolButton { icon: "list-remove"; usable: stepper.value > stepper.minimum; onClicked: stepper.changed(stepper.value - 1) }
        Text {
            width: 36
            height: 32
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: stepper.value
            color: Theme.text
            font.pixelSize: Theme.fontBody
        }
        ToolButton { icon: "list-add"; usable: stepper.value < stepper.maximum; onClicked: stepper.changed(stepper.value + 1) }
    }

    // --- the header ---
    Item {
        id: top
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 44
        Text {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            text: editor.eventId === "" ? qsTr("New event") : qsTr("Event")
            color: Theme.text
            font.pixelSize: Theme.fontBodyLarge
            font.weight: Font.DemiBold
        }
        ToolButton {
            anchors { right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
            icon: "window-close"
            tooltip: qsTr("Close (Esc)")
            onClicked: editor.visible = false
        }
    }

    Flickable {
        id: flick
        anchors { left: parent.left; right: parent.right; top: top.bottom; bottom: footer.top }
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentHeight: form.height + 12
        Column {
            id: form
            x: 16
            width: parent.width - 32
            spacing: 6

            Input {
                id: titleInput
                width: parent.width
                placeholder: qsTr("Add a title")
                text: editor.title
                onEdited: editor.title = text
                input.Keys.onReturnPressed: editor.save()
                input.Keys.onEnterPressed: editor.save()
                input.Keys.onEscapePressed: editor.visible = false
            }
            Row {
                width: parent.width
                height: 32
                Text {
                    width: parent.width - allDayToggle.width
                    height: 32
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("All day")
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                }
                Toggle {
                    id: allDayToggle
                    showLabel: false
                    checked: editor.allDay
                    onToggled: on => editor.allDay = on
                }
            }

            Label { text: qsTr("Starts") }
            Row {
                spacing: 6
                DateField {
                    width: editor.allDay ? form.width : 168
                    day: editor.startDay
                    target: "start"
                }
                Input {
                    id: startTime
                    visible: !editor.allDay
                    width: form.width - 174
                    input.onEditingFinished: {
                        editor.setStart(editor.startDay, editor.parseClock(text, editor.startMinutes))
                        editor.syncTimes()
                    }
                }
            }
            Label { text: qsTr("Ends") }
            Row {
                spacing: 6
                DateField {
                    width: editor.allDay ? form.width : 168
                    day: editor.endDay
                    target: "end"
                }
                Input {
                    id: endTime
                    visible: !editor.allDay
                    width: form.width - 174
                    input.onEditingFinished: {
                        editor.endMinutes = editor.parseClock(text, editor.endMinutes)
                        editor.syncTimes()
                    }
                }
            }

            Label { text: qsTr("Repeat") }
            Choice {
                width: parent.width
                text: editor.freqNames[editor.freq]
                onClicked: {
                    const p = mapToItem(null, 0, height)
                    editor.win.menuLayer.open(editor.freqNames.map((name, i) => ({
                        text: name, checked: editor.freq === i,
                        action: () => {
                            editor.freq = i
                            if (i === 2 && editor.weekdays === 0) editor.weekdays = 1 << ((editor.startDay.getDay() + 6) % 7)
                        }
                    })), p.x, p.y, { minWidth: width })
                }
            }
            Row {
                visible: editor.freq !== 0
                spacing: 8
                Label { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Every") }
                Stepper { value: editor.interval; onChanged: v => editor.interval = v }
                Label { anchors.verticalCenter: parent.verticalCenter; text: editor.unitNames[editor.freq] }
            }
            Row {
                visible: editor.freq === 2
                spacing: 2
                Repeater {
                    model: 7
                    delegate: Rectangle {
                        id: chip
                        required property int index
                        readonly property bool on: (editor.weekdays & (1 << index)) !== 0
                        width: (form.width - 12) / 7
                        height: 32
                        radius: Theme.radius
                        color: on ? Theme.accentFill : chipMouse.containsMouse ? Theme.controlHover : Theme.control
                        border.width: 1
                        border.color: Theme.controlStroke
                        Text {
                            anchors.centerIn: parent
                            text: editor.weekdayName(chip.index).substring(0, 2)
                            color: chip.on ? Theme.accentText : Theme.text
                            font.pixelSize: Theme.fontCaption
                        }
                        MouseArea {
                            id: chipMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: editor.weekdays = editor.weekdays ^ (1 << chip.index)
                        }
                    }
                }
            }
            Row {
                visible: editor.freq !== 0
                spacing: 6
                Choice {
                    width: editor.endMode === 0 ? form.width : 150
                    text: [qsTr("Repeats forever"), qsTr("Until a date"), qsTr("A number of times")][editor.endMode]
                    onClicked: {
                        const p = mapToItem(null, 0, height)
                        editor.win.menuLayer.open([qsTr("Repeats forever"), qsTr("Until a date"), qsTr("A number of times")].map((name, i) => ({
                            text: name, checked: editor.endMode === i, action: () => editor.endMode = i
                        })), p.x, p.y, { minWidth: width })
                    }
                }
                DateField {
                    visible: editor.endMode === 1
                    width: form.width - 156
                    day: editor.untilDay
                    target: "until"
                }
                Stepper {
                    visible: editor.endMode === 2
                    value: editor.count
                    onChanged: v => editor.count = v
                }
            }

            Label { text: qsTr("Reminder") }
            Choice {
                width: parent.width
                text: editor.reminderNames[Math.max(0, editor.reminderValues.indexOf(editor.reminder))]
                onClicked: {
                    const p = mapToItem(null, 0, height)
                    editor.win.menuLayer.open(editor.reminderNames.map((name, i) => ({
                        text: name, checked: editor.reminder === editor.reminderValues[i],
                        action: () => editor.reminder = editor.reminderValues[i]
                    })), p.x, p.y, { minWidth: width })
                }
            }

            Label { text: qsTr("Color") }
            Row {
                spacing: 6
                Repeater {
                    model: D.colors
                    delegate: Rectangle {
                        id: swatch
                        required property int index
                        required property string modelData
                        width: 28
                        height: 28
                        radius: 14
                        color: modelData
                        border.width: editor.eventColor === index ? 2 : 0
                        border.color: Theme.text
                        Text {
                            visible: editor.eventColor === swatch.index
                            anchors.centerIn: parent
                            text: "\u2713"
                            color: "white"
                            font.pixelSize: Theme.fontBody
                        }
                        MouseArea { anchors.fill: parent; onClicked: editor.eventColor = swatch.index }
                    }
                }
            }

            Label { text: qsTr("Location") }
            Input {
                width: parent.width
                placeholder: qsTr("Add a location")
                text: editor.location
                onEdited: editor.location = text
            }
            Label { text: qsTr("Notes") }
            Rectangle {
                width: parent.width
                height: 96
                radius: Theme.radius
                color: notesEdit.activeFocus ? Theme.field : Theme.control
                border.width: 1
                border.color: notesEdit.activeFocus ? Theme.accentFill : Theme.controlStroke
                TextEdit {
                    id: notesEdit
                    anchors { fill: parent; margins: 8 }
                    wrapMode: TextEdit.Wrap
                    clip: true
                    text: editor.notes
                    color: Theme.text
                    selectionColor: Theme.accent
                    selectedTextColor: "white"
                    font.pixelSize: Theme.fontBody
                    onTextChanged: if (activeFocus) editor.notes = text
                }
            }
        }
    }
    ScrollBar { flickable: flick; anchors { right: parent.right; top: flick.top; bottom: flick.bottom } }

    // --- Save and Delete ---
    Rectangle {
        id: footer
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 1 }
        height: 56
        color: Theme.dialogFooter
        bottomLeftRadius: Theme.radiusOverlay - 1
        bottomRightRadius: Theme.radiusOverlay - 1
        Row {
            anchors { fill: parent; margins: 12 }
            spacing: 8
            Rectangle {
                width: editor.eventId === "" ? parent.width : (parent.width - 8) * 0.6
                height: 32
                radius: Theme.radius
                color: saveMouse.containsMouse ? Theme.accentFillHover : Theme.accentFill
                Text {
                    anchors.centerIn: parent
                    text: qsTr("Save")
                    color: Theme.accentText
                    font.pixelSize: Theme.fontBody
                }
                MouseArea { id: saveMouse; anchors.fill: parent; hoverEnabled: true; onClicked: editor.save() }
            }
            Rectangle {
                visible: editor.eventId !== ""
                width: (parent.width - 8) * 0.4
                height: 32
                radius: Theme.radius
                color: deleteMouse.containsMouse ? Theme.controlHover : Theme.control
                border.width: 1
                border.color: Theme.controlStroke
                Text {
                    anchors.centerIn: parent
                    text: qsTr("Delete")
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                }
                MouseArea {
                    id: deleteMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        editor.visible = false
                        editor.deleteRequested(editor.eventId, editor.occurrenceDay, editor.repeats)
                    }
                }
            }
        }
    }

    // --- the month under a date field ---
    Rectangle {
        id: datePopup
        property string target
        visible: false
        z: 10
        width: 252 + 16
        height: mini.implicitHeight + 16
        radius: Theme.radiusOverlay
        color: Theme.flyout
        border.width: 1
        border.color: Theme.stroke
        MiniMonth {
            id: mini
            x: 8
            y: 8
            width: 252
            onPicked: day => {
                if (datePopup.target === "start") editor.setStart(day, editor.startMinutes)
                else if (datePopup.target === "end") editor.endDay = day
                else editor.untilDay = day
                datePopup.visible = false
            }
        }
    }
}
