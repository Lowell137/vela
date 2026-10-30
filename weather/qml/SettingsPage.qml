// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Units and the clock. What's stored is always metric; these only change how
// it's shown.
Item {
    id: page

    // A setting with a few values: the chosen one is the accent button.
    component Choice: Item {
        id: choice
        property string title
        property string current
        property var options: [] // [{value, text}]
        signal picked(string value)
        width: parent ? parent.width : 0
        height: 44
        Text {
            anchors { left: parent.left; verticalCenter: parent.verticalCenter }
            text: choice.title
            color: Theme.text
            font.pixelSize: Theme.fontBody
        }
        Row {
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            spacing: 4
            Repeater {
                model: choice.options
                delegate: Button {
                    required property var modelData
                    implicitWidth: 76
                    accent: choice.current === modelData.value
                    text: modelData.text
                    onClicked: choice.picked(modelData.value)
                }
            }
        }
    }

    Flickable {
        id: scroller
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: width
        contentHeight: card.height + 40

        Card {
            id: card
            x: 20
            y: 4
            width: Math.min(scroller.width - 40, 720)
            Column {
                width: parent.width
                Choice {
                    title: qsTr("Temperature")
                    current: Weather.temperatureUnit
                    options: [{ value: "c", text: "°C" }, { value: "f", text: "°F" }]
                    onPicked: value => Weather.temperatureUnit = value
                }
                Choice {
                    title: qsTr("Wind speed")
                    current: Weather.windUnit
                    options: [{ value: "kmh", text: "km/h" }, { value: "mph", text: "mph" }, { value: "ms", text: "m/s" }, { value: "kn", text: qsTr("knots") }]
                    onPicked: value => Weather.windUnit = value
                }
                Choice {
                    title: qsTr("Precipitation")
                    current: Weather.precipitationUnit
                    options: [{ value: "mm", text: "mm" }, { value: "in", text: "in" }]
                    onPicked: value => Weather.precipitationUnit = value
                }
                Choice {
                    title: qsTr("Pressure")
                    current: Weather.pressureUnit
                    options: [{ value: "hpa", text: "hPa" }, { value: "inhg", text: "inHg" }]
                    onPicked: value => Weather.pressureUnit = value
                }
                Item {
                    width: parent.width
                    height: 44
                    Text {
                        anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                        text: qsTr("24-hour clock")
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                    }
                    Switch {
                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                        checked: Weather.hour24
                        onToggled: value => Weather.hour24 = value
                    }
                }
            }
        }
    }
}
