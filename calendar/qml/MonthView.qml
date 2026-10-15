// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls
import "Dates.js" as D

// A month as six weeks: the days' numbers, the multi-day and all-day events as
// bars over the days, the others as lines; "+N" when a day has more than fit.
// `occs` is Store.occurrences() for the six weeks from `gridStart`.
Item {
    id: view
    property date gridStart: new Date()
    property int shownMonth: 0 // the shown month, the other days are dimmed
    property var occs: []
    property bool weekNumbers: false
    signal slotClicked(real ms)
    signal eventClicked(string id, string day)
    signal dayClicked(date day)

    readonly property int gutter: weekNumbers ? 32 : 0
    readonly property real colW: (width - gutter) / 7
    readonly property real rowH: (height - 28) / 6
    property date now: new Date()
    Timer { interval: 60000; running: true; repeat: true; onTriggered: view.now = new Date() }

    Row {
        x: view.gutter
        Repeater {
            model: 7
            delegate: Text {
                required property int index
                width: view.colW
                height: 28
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: Qt.locale().dayName(D.addDays(view.gridStart, index).getDay(), Locale.ShortFormat)
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }
        }
    }
    Repeater {
        model: 6
        delegate: Item {
            id: week
            required property int index
            readonly property date first: D.addDays(view.gridStart, index * 7)
            readonly property var bars: D.layoutBars(view.occs, first, 7)
            readonly property var singles: D.perDay(view.occs, first, 7)
            readonly property int fits: Math.max(0, Math.floor((view.rowH - 26 - bars.lanes * 20) / 18))
            y: 28 + index * view.rowH
            width: view.width
            height: view.rowH

            Text {
                visible: view.weekNumbers
                width: view.gutter
                y: 4
                horizontalAlignment: Text.AlignHCenter
                text: D.isoWeek(D.addDays(week.first, 3))
                color: Theme.textTertiary
                font.pixelSize: Theme.fontCaption
            }
            Repeater {
                model: 7
                delegate: Rectangle {
                    id: cell
                    required property int index
                    readonly property date day: D.addDays(week.first, index)
                    readonly property bool today: D.sameDay(day, view.now)
                    readonly property var list: week.singles[index]
                    x: view.gutter + index * view.colW
                    width: view.colW
                    height: week.height
                    color: today ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08) : cellMouse.containsMouse ? Theme.subtleHover : "transparent"
                    border.width: 1
                    border.color: Theme.divider
                    MouseArea {
                        id: cellMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: view.slotClicked(new Date(cell.day.getFullYear(), cell.day.getMonth(), cell.day.getDate(), 9).getTime())
                    }
                    Rectangle {
                        x: 4
                        y: 3
                        width: 22
                        height: 22
                        radius: 11
                        color: cell.today ? Theme.accentFill : "transparent"
                        Text {
                            anchors.centerIn: parent
                            text: cell.day.getDate()
                            color: cell.today ? Theme.accentText : cell.day.getMonth() === view.shownMonth ? Theme.text : Theme.textTertiary
                            font.pixelSize: Theme.fontCaption
                            font.weight: cell.today ? Font.DemiBold : Font.Normal
                        }
                    }
                    Column {
                        x: 2
                        y: 26 + week.bars.lanes * 20
                        width: parent.width - 4
                        Repeater {
                            model: Math.min(cell.list.length, week.fits >= cell.list.length ? week.fits : Math.max(0, week.fits - 1))
                            delegate: Item {
                                id: line
                                required property int index
                                readonly property var occ: cell.list[index]
                                width: parent.width
                                height: 18
                                Rectangle {
                                    x: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 6
                                    height: 6
                                    radius: 3
                                    color: D.colors[line.occ.color]
                                }
                                Text {
                                    anchors { fill: parent; leftMargin: 14; rightMargin: 2 }
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    text: D.timeText(line.occ.startMs) + " " + (line.occ.title || qsTr("(No title)"))
                                    color: Theme.text
                                    font.pixelSize: Theme.fontCaption
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: view.eventClicked(line.occ.id)
                                }
                            }
                        }
                        Text {
                            visible: cell.list.length > week.fits
                            width: parent.width
                            leftPadding: 6
                            height: 18
                            verticalAlignment: Text.AlignVCenter
                            text: qsTr("+%1 more").arg(cell.list.length - Math.max(0, week.fits - 1))
                            color: Theme.link
                            font.pixelSize: Theme.fontCaption
                            MouseArea { anchors.fill: parent; onClicked: view.dayClicked(cell.day) }
                        }
                    }
                }
            }
            Repeater {
                model: week.bars.bars
                delegate: Rectangle {
                    id: bar
                    required property var modelData
                    x: view.gutter + modelData.col * view.colW + 2
                    y: 26 + modelData.lane * 20
                    width: modelData.span * view.colW - 4
                    height: 18
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
                        onClicked: view.eventClicked(bar.modelData.occ.id, bar.modelData.occ.day)
                    }
                }
            }
        }
    }
}
