// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The weather flyout, like the hidden icons one (TrayOverflow.qml): a panel
// next to the taskbar button with the place, the current conditions, the next
// twelve hours and five days. A click outside, Esc or the button close it.
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
        root.updateBlur()
    }
    function close() {
        Menus.weather = null
    }
    function updateBlur() {
        if (visible) {
            Effects.setBlur(root, [Qt.rect(panel.x, root.panelY, panel.width, panel.height)])
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
        width: 400
        height: column.implicitHeight + 32
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
            width: parent.width - 32
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
    }
}
