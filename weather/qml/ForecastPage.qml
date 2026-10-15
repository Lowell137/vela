// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The chosen place's weather: now, the next hours, the details, the sun, the
// air and the coming days, on a tint chosen by the sky and the time of day.
Item {
    id: page
    signal addPlace()

    // Translucent: the page's own layer stays under it, so the text colors
    // of the theme stay readable in light and dark.
    function tint(sky, night) {
        if (night) {
            switch (sky) {
            case "rain": return "#3b5878"
            case "snow": return "#7088a8"
            case "storm": return "#4a3f78"
            case "fog": return "#5a6270"
            case "cloudy": return "#3c4a66"
            default: return "#1d2f6b"
            }
        }
        switch (sky) {
        case "rain": return "#5b84b0"
        case "snow": return "#a9c4de"
        case "storm": return "#7a68b8"
        case "fog": return "#9aa4ae"
        case "cloudy": return "#7f9bbd"
        default: return "#3d95f0"
        }
    }
    readonly property color skyColor: tint(Weather.sky, Weather.night)

    Rectangle {
        anchors.fill: parent
        visible: Weather.hasData
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.rgba(page.skyColor.r, page.skyColor.g, page.skyColor.b, Theme.light ? 0.38 : 0.34) }
            GradientStop { position: 0.55; color: Qt.rgba(page.skyColor.r, page.skyColor.g, page.skyColor.b, 0.08) }
            GradientStop { position: 1; color: "transparent" }
        }
    }

    // No place yet.
    Column {
        anchors.centerIn: parent
        visible: !Weather.hasPlace
        spacing: 14
        width: Math.min(360, parent.width - 40)
        Text {
            width: parent.width
            text: qsTr("Search for a city to see its weather.")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontBodyLarge
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }
        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            accent: true
            text: qsTr("Add a place")
            onClicked: page.addPlace()
        }
    }

    // A place, but nothing to show yet (first start without a cache, or offline).
    Column {
        anchors.centerIn: parent
        visible: Weather.hasPlace && !Weather.hasData
        spacing: 14
        width: Math.min(400, parent.width - 40)
        Text {
            width: parent.width
            text: Weather.offline ? qsTr("The weather can't be loaded. Check your connection and try again.") : qsTr("Loading the weather…")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontBodyLarge
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }
        Button {
            visible: Weather.offline
            anchors.horizontalCenter: parent.horizontalCenter
            accent: true
            text: qsTr("Retry")
            onClicked: Weather.refresh()
        }
    }

    Flickable {
        id: scroller
        anchors.fill: parent
        visible: Weather.hasData
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: width
        contentHeight: content.height + 40

        Column {
            id: content
            x: Math.max(20, (scroller.width - width) / 2)
            y: 8
            width: Math.min(scroller.width - 40, 920)
            spacing: 12

            // The old data is on screen: say so, and offer another try.
            Rectangle {
                visible: Weather.offline
                width: parent.width
                height: 44
                radius: Theme.radiusCard
                color: Theme.card
                border.width: 1
                border.color: Theme.cardStroke
                Text {
                    anchors { left: parent.left; leftMargin: 16; right: retry.left; rightMargin: 12; verticalCenter: parent.verticalCenter }
                    text: qsTr("Can't reach the weather service. Showing the last saved forecast.")
                    color: Theme.text
                    font.pixelSize: Theme.fontCaption
                    elide: Text.ElideRight
                }
                Button {
                    id: retry
                    anchors { right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
                    implicitHeight: 32
                    text: qsTr("Retry")
                    onClicked: Weather.refresh()
                }
            }

            // --- now ---
            Item {
                width: parent.width
                height: 150
                Image {
                    id: nowIcon
                    x: 4
                    anchors.verticalCenter: parent.verticalCenter
                    width: 88
                    height: 88
                    source: "image://appicon/" + encodeURIComponent(Weather.now.icon || "weather-none-available")
                    sourceSize: Qt.size(width, height)
                }
                Column {
                    anchors { left: nowIcon.right; leftMargin: 20; verticalCenter: parent.verticalCenter }
                    spacing: 2
                    Text {
                        text: Weather.now.temp || ""
                        color: Theme.text
                        font.pixelSize: 64
                        font.weight: Font.Light
                    }
                    Text {
                        text: Weather.now.text || ""
                        color: Theme.text
                        font.pixelSize: Theme.fontSubtitle
                    }
                }
                Column {
                    anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                    spacing: 4
                    Text {
                        anchors.right: parent.right
                        text: Weather.now.feels || ""
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontBody
                    }
                    Text {
                        anchors.right: parent.right
                        text: qsTr("High %1 · Low %2").arg(Weather.now.high || "").arg(Weather.now.low || "")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontBody
                    }
                }
            }

            // --- hourly ---
            Card {
                width: parent.width
                title: qsTr("Hourly forecast")
                Flickable {
                    id: strip
                    width: parent.width
                    height: chart.height + 100
                    contentWidth: chart.width
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    flickableDirection: Flickable.HorizontalFlick

                    HourlyChart {
                        id: chart
                        temps: Weather.hourly.map(h => h.value)
                        chances: Weather.hourly.map(h => h.chance)
                    }
                    Row {
                        y: chart.height + 6
                        Repeater {
                            model: Weather.hourly
                            delegate: Column {
                                required property var modelData
                                width: chart.step
                                spacing: 4
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: modelData.temp
                                    color: Theme.text
                                    font.pixelSize: Theme.fontBody
                                }
                                Image {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 28
                                    height: 28
                                    source: "image://appicon/" + encodeURIComponent(modelData.icon)
                                    sourceSize: Qt.size(width, height)
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: modelData.chance >= 0 ? modelData.chance + "%" : ""
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontCaption
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: modelData.time
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontCaption
                                }
                            }
                        }
                    }
                }
            }

            // --- details ---
            Grid {
                id: grid
                readonly property int cols: width >= 700 ? 4 : 2
                width: parent.width
                columns: grid.cols
                spacing: 8
                Repeater {
                    model: Weather.details
                    delegate: Rectangle {
                        required property var modelData
                        width: (grid.width - grid.spacing * (grid.cols - 1)) / grid.cols
                        height: 68
                        radius: Theme.radiusCard
                        color: Theme.card
                        border.width: 1
                        border.color: Theme.cardStroke
                        Column {
                            anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter }
                            width: parent.width - 28
                            spacing: 4
                            Text {
                                width: parent.width
                                text: modelData.label
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.width
                                text: modelData.value
                                color: Theme.text
                                font.pixelSize: Theme.fontBodyLarge
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            // --- sun and air ---
            Flow {
                id: pair
                width: parent.width
                spacing: 12
                readonly property real half: width >= 640 ? (width - spacing) / 2 : width

                Card {
                    visible: Weather.sun.valid === true
                    width: pair.half
                    title: qsTr("Sunrise and sunset")
                    Column {
                        width: parent.width
                        spacing: 6
                        SunArc {
                            width: parent.width
                            progress: Weather.sun.progress === undefined ? -1 : Weather.sun.progress
                        }
                        Item {
                            width: parent.width
                            height: 22
                            Text {
                                anchors.left: parent.left
                                text: "↑ " + (Weather.sun.rise || "")
                                color: Theme.text
                                font.pixelSize: Theme.fontBody
                            }
                            Text {
                                anchors.right: parent.right
                                text: "↓ " + (Weather.sun.set || "")
                                color: Theme.text
                                font.pixelSize: Theme.fontBody
                            }
                        }
                        Text {
                            visible: text !== ""
                            text: Weather.sun.daylight || ""
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                        }
                    }
                }

                Card {
                    visible: Weather.air.available === true
                    width: pair.half
                    title: qsTr("Air quality")
                    Column {
                        width: parent.width
                        spacing: 8
                        Row {
                            spacing: 10
                            Text {
                                text: Weather.air.value === undefined ? "" : Weather.air.value
                                color: Theme.text
                                font.pixelSize: 32
                                font.weight: Font.Light
                            }
                            Text {
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 6
                                text: Weather.air.text || ""
                                color: Theme.text
                                font.pixelSize: Theme.fontBodyLarge
                            }
                        }
                        // The European AQI scale: six bands of 20, the marker where we are.
                        Item {
                            width: parent.width
                            height: 14
                            Row {
                                anchors.verticalCenter: parent.verticalCenter
                                Repeater {
                                    model: 6
                                    delegate: Rectangle {
                                        required property int index
                                        width: parent.parent.width / 6
                                        height: 6
                                        color: ["#50f0e6", "#50ccaa", "#f0e641", "#ff5050", "#960032", "#7d2181"][index]
                                        radius: index === 0 || index === 5 ? 3 : 0
                                    }
                                }
                            }
                            Rectangle {
                                x: Math.min(1, Math.max(0, (Weather.air.value || 0) / 120)) * (parent.width - width)
                                anchors.verticalCenter: parent.verticalCenter
                                width: 6
                                height: 14
                                radius: 3
                                color: Theme.text
                                border.width: 1
                                border.color: Theme.layer
                            }
                        }
                        Row {
                            spacing: 24
                            Text {
                                text: qsTr("PM2.5: %1").arg(Weather.air.pm25 || "")
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                            }
                            Text {
                                text: qsTr("PM10: %1").arg(Weather.air.pm10 || "")
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                            }
                        }
                    }
                }
            }

            // --- daily ---
            Card {
                id: dailyCard
                width: parent.width
                title: qsTr("10-day forecast")
                readonly property real lowest: Weather.daily.reduce((m, d) => Math.min(m, d.minValue), 1000)
                readonly property real highest: Weather.daily.reduce((m, d) => Math.max(m, d.maxValue), -1000)
                Column {
                    width: parent.width
                    Repeater {
                        model: Weather.daily
                        delegate: Item {
                            id: row
                            required property var modelData
                            width: parent.width
                            height: 40
                            Text {
                                x: 0
                                anchors.verticalCenter: parent.verticalCenter
                                width: 72
                                text: row.modelData.day
                                color: Theme.text
                                font.pixelSize: Theme.fontBody
                            }
                            Image {
                                x: 76
                                anchors.verticalCenter: parent.verticalCenter
                                width: 24
                                height: 24
                                source: "image://appicon/" + encodeURIComponent(row.modelData.icon)
                                sourceSize: Qt.size(width, height)
                            }
                            Text {
                                x: 108
                                anchors.verticalCenter: parent.verticalCenter
                                width: 44
                                text: row.modelData.chance >= 0 ? row.modelData.chance + "%" : ""
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                            }
                            Text {
                                id: low
                                x: 156
                                anchors.verticalCenter: parent.verticalCenter
                                width: 40
                                text: row.modelData.min
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontBody
                                horizontalAlignment: Text.AlignRight
                            }
                            Item {
                                id: range
                                x: 204
                                anchors.verticalCenter: parent.verticalCenter
                                width: Math.max(40, parent.width - 204 - 52)
                                height: 6
                                readonly property real span: Math.max(1, dailyCard.highest - dailyCard.lowest)
                                Rectangle {
                                    anchors.fill: parent
                                    radius: 3
                                    color: Theme.divider
                                }
                                Rectangle {
                                    x: (row.modelData.minValue - dailyCard.lowest) / range.span * range.width
                                    width: Math.max(6, (row.modelData.maxValue - row.modelData.minValue) / range.span * range.width)
                                    height: parent.height
                                    radius: 3
                                    color: Theme.accentFill
                                }
                            }
                            Text {
                                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                                width: 44
                                text: row.modelData.max
                                color: Theme.text
                                font.pixelSize: Theme.fontBody
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }
                }
            }

            Text {
                width: parent.width
                text: qsTr("Weather data by Open-Meteo.com")
                color: Theme.textTertiary
                font.pixelSize: Theme.fontCaption
                horizontalAlignment: Text.AlignRight
            }
        }
    }

    ScrollBar {
        flickable: scroller
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
    }
}
