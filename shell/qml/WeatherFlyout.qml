// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The weather flyout, like the hidden icons one (TrayOverflow.qml): a panel
// next to the taskbar button with the place, the current conditions, the next
// twelve hours and five days, and beside it the widgets board's other cards:
// the month's calendar and the system status. A click outside, Esc or the button close it.
// Opened with Menus.weather.
Window {
    id: root
    visible: false
    color: "transparent"

    property real anchorX: 0
    property real anchorY: 0
    property real offset: 0

    Connections {
        target: Menus
        function onWeatherChanged() {
            if (Menus.weather) {
                root.open()
            } else {
                root.visible = false
            }
        }
    }
    function open() {
        const request = Menus.weather
        if (!request) return
        const screen = Qt.application.screens.find(s => s.name === request.screen)
        if (screen && screen !== root.screen) {
            if (root.visible) {
                root.visible = false
                Qt.callLater(root.open)
                return
            }
            Shell.placeOnScreen(root, screen.name)
        }
        root.anchorX = request.center
        root.anchorY = request.centerY
        root.visible = true
        panel.opacity = 0
        root.offset = Config.taskbarLeft || Config.taskbarTop ? -6 : 6
        appear.restart()
        content.forceActiveFocus()
        Weather.refresh()
        board.today()
        SysStats.active = true
        root.updateBlur()
    }
    function close() {
        Menus.weather = null
    }
    onVisibleChanged: if (!visible) SysStats.active = false
    function updateBlur() {
        if (visible) {
            Effects.setBlur(root, [Qt.rect(panel.x, root.panelY, panel.width, panel.height)], panel.radius)
        }
    }
    onWidthChanged: updateBlur()
    onHeightChanged: updateBlur()
    onActiveChanged: if (!active && visible && !Menus.isOpen) close()

    readonly property real panelY: Config.taskbarVertical
        ? Math.max(8, Math.min(height - panel.height - 8, anchorY - panel.height / 2))
        : Config.taskbarTop ? Theme.taskbarHeight + 12 : height - Theme.taskbarHeight - 12 - panel.height
    readonly property real panelX: Config.taskbarVertical
        ? (Config.taskbarLeft ? Theme.taskbarHeight + 12 : width - Theme.taskbarHeight - 12 - panel.width)
        : Math.max(8, Math.min(width - panel.width - 8, anchorX - panel.width / 2))
    onPanelYChanged: updateBlur()

    ParallelAnimation {
        id: appear
        NumberAnimation { target: panel; property: "opacity"; to: 1; duration: Theme.fast }
        NumberAnimation { target: root; property: "offset"; to: 0; duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onClicked: root.close()
    }

    Item {
        id: content
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: root.close()
    }

    Rectangle {
        id: panel
        x: root.panelX + (Config.taskbarVertical ? root.offset : 0)
        y: root.panelY + (Config.taskbarVertical ? 0 : root.offset)
        width: 400 + 340
        height: Math.max(column.implicitHeight, board.implicitHeight) + 32
        radius: Theme.radiusOverlay
        color: Theme.popup
        border.width: 1
        border.color: Theme.stroke
        onXChanged: root.updateBlur()
        onHeightChanged: root.updateBlur()

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
        }

        Column {
            id: column
            x: 16
            y: 16
            width: 400 - 32
            spacing: 14

            // Place, and now.
            Row {
                spacing: 12
                Image {
                    width: 48
                    height: 48
                    source: Theme.icons + encodeURIComponent(Weather.available ? Weather.icon : "weather-none-available")
                    sourceSize: Qt.size(width, height)
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        text: Weather.city
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.DemiBold
                    }
                    Text {
                        visible: Weather.available
                        text: Weather.temperature + "°  " + Weather.text
                        color: Theme.text
                        font.pixelSize: Theme.fontSubtitle
                    }
                    Text {
                        visible: !Weather.available
                        text: qsTr("No weather data yet")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontBody
                    }
                }
            }

            Text {
                visible: Weather.stale && Weather.available
                width: parent.width
                wrapMode: Text.Wrap
                color: Theme.caution
                font.pixelSize: Theme.fontCaption
                text: qsTr("Out of date. Last updated %1.").arg(Qt.formatTime(Weather.updated, "HH:mm"))
            }

            Row {
                visible: Weather.available
                width: parent.width
                readonly property int cell: width / 3
                Repeater {
                    model: [
                        { label: qsTr("Feels like"), value: Weather.feelsLike + "°" },
                        { label: qsTr("Humidity"), value: Weather.humidity + "%" },
                        { label: qsTr("Wind"), value: Weather.windSpeed + " " + Weather.windUnit }
                    ]
                    delegate: Column {
                        required property var modelData
                        width: parent.cell
                        Text { text: modelData.label; color: Theme.textSecondary; font.pixelSize: Theme.fontCaption }
                        Text { text: modelData.value; color: Theme.text; font.pixelSize: Theme.fontBody }
                    }
                }
            }

            Rectangle { visible: Weather.available; width: parent.width; height: 1; color: Theme.divider }

            // The next twelve hours.
            ListView {
                visible: Weather.available
                width: parent.width
                height: 76
                orientation: ListView.Horizontal
                clip: true
                interactive: contentWidth > width
                model: Weather.hourly
                delegate: Column {
                    required property var modelData
                    width: 54
                    spacing: 2
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.time; color: Theme.textSecondary; font.pixelSize: Theme.fontCaption }
                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 24; height: 24
                        source: Theme.icons + encodeURIComponent(modelData.icon)
                        sourceSize: Qt.size(width, height)
                    }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.temp + "°"; color: Theme.text; font.pixelSize: Theme.fontBody }
                }
            }

            Rectangle { visible: Weather.available; width: parent.width; height: 1; color: Theme.divider }

            // Five days.
            Column {
                visible: Weather.available
                width: parent.width
                spacing: 4
                Repeater {
                    model: Weather.daily
                    delegate: Item {
                        required property var modelData
                        width: parent.width
                        height: 28
                        Text {
                            x: 0
                            anchors.verticalCenter: parent.verticalCenter
                            width: 90
                            elide: Text.ElideRight
                            text: modelData.day
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                        }
                        Image {
                            x: 96
                            anchors.verticalCenter: parent.verticalCenter
                            width: 22; height: 22
                            source: Theme.icons + encodeURIComponent(modelData.icon)
                            sourceSize: Qt.size(width, height)
                        }
                        Text {
                            x: 130
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 130 - 90
                            elide: Text.ElideRight
                            text: modelData.text
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                        }
                        Text {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.max + "°  " + modelData.min + "°"
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                        }
                    }
                }
            }
        }

        // The other cards: the calendar and the system status.
        Column {
            id: board
            x: 16 + 400 - 32 + 16
            y: 16
            width: 340 - 0 - 16
            spacing: 12

            property date shown: new Date()
            function today() { shown = new Date() }
            function step(months) { shown = new Date(shown.getFullYear(), shown.getMonth() + months, 1) }
            readonly property date now: new Date()

            Rectangle {
                width: parent.width
                height: calendarColumn.implicitHeight + 24
                radius: Theme.radius
                color: Theme.subtleHover

                Column {
                    id: calendarColumn
                    x: 12
                    y: 12
                    width: parent.width - 24
                    spacing: 6

                    Row {
                        width: parent.width
                        Text {
                            width: parent.width - 56
                            text: Qt.locale().standaloneMonthName(board.shown.getMonth()) + " " + board.shown.getFullYear()
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            font.weight: Font.DemiBold
                        }
                        Repeater {
                            model: [{ glyph: "‹", months: -1 }, { glyph: "›", months: 1 }]
                            delegate: Item {
                                required property var modelData
                                width: 28
                                height: 22
                                Text { anchors.centerIn: parent; text: modelData.glyph; color: Theme.text; font.pixelSize: Theme.fontSubtitle }
                                MouseArea { anchors.fill: parent; onClicked: board.step(modelData.months) }
                            }
                        }
                    }
                    Grid {
                        columns: 7
                        width: parent.width
                        readonly property real cell: width / 7
                        readonly property int offset: (new Date(board.shown.getFullYear(), board.shown.getMonth(), 1).getDay() + 6) % 7 // Monday first
                        readonly property int days: new Date(board.shown.getFullYear(), board.shown.getMonth() + 1, 0).getDate()

                        Repeater {
                            model: 7
                            delegate: Text {
                                required property int index
                                width: parent.cell
                                horizontalAlignment: Text.AlignHCenter
                                text: Qt.locale().dayName((index + 1) % 7, Locale.NarrowFormat)
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                            }
                        }
                        Repeater {
                            model: parent.offset + parent.days
                            delegate: Item {
                                required property int index
                                readonly property int day: index - parent.offset + 1
                                readonly property bool isToday: day === board.now.getDate()
                                    && board.shown.getMonth() === board.now.getMonth()
                                    && board.shown.getFullYear() === board.now.getFullYear()
                                width: parent.cell
                                height: 28
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 26
                                    height: 26
                                    radius: 13
                                    color: Theme.accent
                                    visible: parent.isToday
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: parent.day > 0 ? parent.day : ""
                                    color: parent.isToday ? Theme.accentText : Theme.text
                                    font.pixelSize: Theme.fontCaption + 1
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: statsColumn.implicitHeight + 24
                radius: Theme.radius
                color: Theme.subtleHover

                Column {
                    id: statsColumn
                    x: 12
                    y: 12
                    width: parent.width - 24
                    spacing: 10

                    Text { text: qsTr("System"); color: Theme.text; font.pixelSize: Theme.fontBody; font.weight: Font.DemiBold }
                    Repeater {
                        model: [
                            { label: qsTr("Processor"), value: SysStats.cpu, text: SysStats.cpu + "%" },
                            { label: qsTr("Memory"), value: SysStats.memory, text: SysStats.memoryText },
                            { label: qsTr("Disk"), value: SysStats.disk, text: SysStats.diskText }
                        ]
                        delegate: Column {
                            required property var modelData
                            width: statsColumn.width
                            spacing: 3
                            Item {
                                width: parent.width
                                height: label.implicitHeight
                                Text { id: label; text: modelData.label; color: Theme.textSecondary; font.pixelSize: Theme.fontCaption }
                                Text { anchors.right: parent.right; text: modelData.text; color: Theme.text; font.pixelSize: Theme.fontCaption }
                            }
                            Rectangle {
                                width: parent.width
                                height: 4
                                radius: 2
                                color: Theme.divider
                                Rectangle { width: parent.width * Math.min(100, modelData.value) / 100; height: parent.height; radius: 2; color: Theme.accent }
                            }
                        }
                    }
                }
            }
        }
    }
}
