// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls
import "Dates.js" as D

// Day, work week and week: the days as columns over the hours, the all-day
// and multi-day events as bars above. Overlapping events share a column's
// width. `occs` is Store.occurrences() for the days shown.
Item {
    id: grid
    property date start: new Date()
    property int days: 7
    property var occs: []
    property bool weekNumbers: false
    property int rowH: 48
    signal slotClicked(real ms)
    signal eventClicked(string id, string day)
    signal dayClicked(date day)

    readonly property int gutter: 64
    readonly property real colW: (width - gutter) / days
    readonly property var barLayout: D.layoutBars(occs, start, days)
    readonly property var columns: D.perDay(occs, start, days).map(list => D.layoutColumn(list))
    property date now: new Date()
    Timer { interval: 30000; running: true; repeat: true; onTriggered: grid.now = new Date() }

    // --- the days' names ---
    Item {
        id: head
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 52
        Text {
            visible: grid.weekNumbers
            x: 0
            width: grid.gutter
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("W%1").arg(D.isoWeek(D.addDays(grid.start, 3)))
            color: Theme.textTertiary
            font.pixelSize: Theme.fontCaption
        }
        Repeater {
            model: grid.days
            delegate: Item {
                id: heading
                required property int index
                readonly property date day: D.addDays(grid.start, index)
                readonly property bool today: D.sameDay(day, grid.now)
                x: grid.gutter + index * grid.colW
                width: grid.colW
                height: head.height
                Rectangle {
                    anchors { fill: parent; margins: 3 }
                    radius: Theme.radius
                    color: headMouse.containsMouse ? Theme.subtleHover : "transparent"
                }
                Column {
                    anchors.centerIn: parent
                    spacing: 2
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: Qt.locale().dayName(heading.day.getDay(), Locale.ShortFormat)
                        color: heading.today ? Theme.accentFill : Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: heading.day.getDate()
                        color: heading.today ? Theme.accentFill : Theme.text
                        font.pixelSize: Theme.fontBodyLarge
                        font.weight: heading.today ? Font.DemiBold : Font.Normal
                    }
                }
                MouseArea {
                    id: headMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: grid.dayClicked(heading.day)
                }
            }
        }
    }

    // --- the bars: all-day and multi-day ---
    Item {
        id: band
        anchors { left: parent.left; right: parent.right; top: head.bottom }
        height: grid.barLayout.lanes > 0 ? Math.min(grid.barLayout.lanes, 4) * 24 + 8 : 0
        clip: true
        Text {
            visible: grid.barLayout.lanes > 0
            x: 0
            y: 4
            width: grid.gutter - 8
            horizontalAlignment: Text.AlignRight
            text: qsTr("All day")
            color: Theme.textTertiary
            font.pixelSize: Theme.fontCaption
        }
        Repeater {
            model: grid.barLayout.bars
            delegate: Rectangle {
                id: bar
                required property var modelData
                x: grid.gutter + modelData.col * grid.colW + 2
                y: 4 + modelData.lane * 24
                width: modelData.span * grid.colW - 4
                height: 21
                visible: modelData.lane < 4
                radius: 3
                color: D.colors[modelData.occ.color]
                opacity: barMouse.containsMouse ? 0.85 : 1
                Text {
                    anchors { fill: parent; leftMargin: 6; rightMargin: 4 }
                    verticalAlignment: Text.AlignVCenter
                    text: bar.modelData.occ.title || qsTr("(No title)")
                    elide: Text.ElideRight
                    color: "white"
                    font.pixelSize: Theme.fontCaption
                }
                MouseArea {
                    id: barMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: grid.eventClicked(bar.modelData.occ.id, bar.modelData.occ.day)
                }
            }
        }
    }
    Rectangle { anchors { left: parent.left; right: parent.right; top: band.bottom } height: 1; color: Theme.divider }

    // --- the hours ---
    Flickable {
        id: flick
        anchors { left: parent.left; right: parent.right; top: band.bottom; bottom: parent.bottom; topMargin: 1 }
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentHeight: 24 * grid.rowH
        Component.onCompleted: {
            const today = D.sameDay(grid.now, D.addDays(grid.start, 0)) || grid.days > 1
            contentY = Math.max(0, Math.min(contentHeight - height, (today ? Math.max(6, grid.now.getHours() - 1) : 7) * grid.rowH))
        }

        Item {
            width: flick.width
            height: flick.contentHeight

            Repeater {
                model: 24
                delegate: Item {
                    id: hour
                    required property int index
                    y: index * grid.rowH
                    width: parent.width
                    height: grid.rowH
                    Text {
                        visible: hour.index > 0
                        width: grid.gutter - 8
                        y: -8
                        horizontalAlignment: Text.AlignRight
                        text: D.timeText(new Date(2000, 0, 1, hour.index).getTime())
                        color: Theme.textTertiary
                        font.pixelSize: Theme.fontCaption
                    }
                    Rectangle { x: grid.gutter; width: parent.width - grid.gutter; height: 1; color: Theme.divider }
                }
            }
            Repeater {
                model: grid.days
                delegate: Item {
                    id: column
                    required property int index
                    readonly property date day: D.addDays(grid.start, index)
                    x: grid.gutter + index * grid.colW
                    width: grid.colW
                    height: parent.height

                    Rectangle {
                        visible: D.sameDay(column.day, grid.now)
                        anchors.fill: parent
                        color: Theme.accent
                        opacity: 0.06
                    }
                    Rectangle { width: 1; height: parent.height; color: Theme.divider }
                    // An empty slot: a click there starts an event at that half hour.
                    MouseArea {
                        anchors.fill: parent
                        onClicked: mouse => {
                            const half = Math.floor(mouse.y / (grid.rowH / 2))
                            const d = column.day
                            grid.slotClicked(new Date(d.getFullYear(), d.getMonth(), d.getDate(), 0, half * 30).getTime())
                        }
                    }
                    Repeater {
                        model: grid.columns[column.index] || []
                        delegate: Rectangle {
                            id: card
                            required property var modelData
                            readonly property real startMin: D.minutesOfDay(modelData.occ.startMs)
                            readonly property real lengthMin: Math.min(1440 - startMin, Math.max(20, (modelData.occ.endMs - modelData.occ.startMs) / 60000))
                            x: 2 + modelData.lane * (column.width - 6) / modelData.lanes
                            y: startMin / 60 * grid.rowH + 1
                            width: (column.width - 6) / modelData.lanes - 1
                            height: lengthMin / 60 * grid.rowH - 2
                            radius: 3
                            color: D.colors[modelData.occ.color]
                            border.width: 1
                            border.color: Theme.background
                            opacity: cardMouse.containsMouse ? 0.88 : 1
                            clip: true
                            Column {
                                anchors { fill: parent; margins: 4 }
                                Text {
                                    width: parent.width
                                    text: card.modelData.occ.title || qsTr("(No title)")
                                    elide: Text.ElideRight
                                    color: "white"
                                    font.pixelSize: Theme.fontCaption
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    width: parent.width
                                    visible: card.height > 40
                                    text: D.timeText(card.modelData.occ.startMs)
                                          + (card.modelData.occ.location ? "  " + card.modelData.occ.location : "")
                                    elide: Text.ElideRight
                                    color: "white"
                                    opacity: 0.85
                                    font.pixelSize: Theme.fontCaption
                                }
                            }
                            MouseArea {
                                id: cardMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: grid.eventClicked(card.modelData.occ.id, card.modelData.occ.day)
                            }
                        }
                    }
                    // The time now.
                    Rectangle {
                        visible: D.sameDay(column.day, grid.now)
                        y: (grid.now.getHours() * 60 + grid.now.getMinutes()) / 60 * grid.rowH - 1
                        width: parent.width
                        height: 2
                        color: Theme.critical
                    }
                }
            }
        }
    }
    ScrollBar {
        flickable: flick
        anchors { right: parent.right; top: flick.top; bottom: parent.bottom }
    }
}
