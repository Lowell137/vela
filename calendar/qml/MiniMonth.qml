// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls
import "Dates.js" as D

// A month to pick a day from: the one at the left of the Calendar, and in the
// date fields' popups. `rangeStart` and `rangeDays` tint what the view shows.
Item {
    id: mini
    property date selected: new Date()
    property date shown: new Date()
    property date rangeStart: selected
    property int rangeDays: 1
    property int firstDay: Qt.locale().firstDayOfWeek
    property bool weekNumbers: false
    signal picked(date day)

    implicitWidth: 252
    implicitHeight: 28 + 24 + 6 * 32
    onSelectedChanged: shown = selected

    readonly property date gridStart: D.weekStart(new Date(shown.getFullYear(), shown.getMonth(), 1), firstDay)
    readonly property int cellW: Math.floor((width - (weekNumbers ? 24 : 0)) / 7)

    Text {
        x: 8
        y: 0
        height: 28
        verticalAlignment: Text.AlignVCenter
        text: Qt.locale().monthName(mini.shown.getMonth(), Locale.LongFormat) + " " + mini.shown.getFullYear()
        color: Theme.text
        font.pixelSize: Theme.fontBody
        font.weight: Font.DemiBold
    }
    Row {
        anchors { right: parent.right; top: parent.top }
        ToolButton { width: 28; height: 28; icon: "go-up"; onClicked: mini.shown = D.addMonths(mini.shown, -1) }
        ToolButton { width: 28; height: 28; icon: "go-down"; onClicked: mini.shown = D.addMonths(mini.shown, 1) }
    }
    Row {
        y: 28
        x: mini.weekNumbers ? 24 : 0
        Repeater {
            model: 7
            delegate: Text {
                required property int index
                width: mini.cellW
                height: 24
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: Qt.locale().dayName((mini.firstDay + index) % 7, Locale.NarrowFormat)
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }
        }
    }
    Grid {
        x: mini.weekNumbers ? 24 : 0
        y: 52
        columns: 7
        Repeater {
            model: 42
            delegate: Rectangle {
                id: cell
                required property int index
                readonly property date day: D.addDays(mini.gridStart, index)
                readonly property bool today: D.sameDay(day, new Date())
                readonly property bool inRange: { const n = D.dayDiff(mini.rangeStart, day); return n >= 0 && n < mini.rangeDays }
                width: mini.cellW
                height: 32
                radius: Theme.radius
                color: today ? Theme.accentFill : cellMouse.containsMouse ? Theme.subtleHover : inRange ? Theme.selection : "transparent"
                Text {
                    anchors.centerIn: parent
                    text: cell.day.getDate()
                    color: cell.today ? Theme.accentText : cell.day.getMonth() === mini.shown.getMonth() ? Theme.text : Theme.textTertiary
                    font.pixelSize: Theme.fontCaption
                }
                MouseArea {
                    id: cellMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: mini.picked(cell.day)
                }
            }
        }
    }
    // Week numbers, left of each row.
    Repeater {
        model: mini.weekNumbers ? 6 : 0
        delegate: Text {
            required property int index
            y: 52 + index * 32
            width: 24
            height: 32
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: D.isoWeek(D.addDays(mini.gridStart, index * 7 + 3))
            color: Theme.textTertiary
            font.pixelSize: Theme.fontCaption
        }
    }
}
