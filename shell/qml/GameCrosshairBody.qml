// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Crosshair widget: the same gamebar/crosshair-* keys as Settings > Gaming.
// The mark itself is dragged on the screen (GameBar.qml).
Column {
    id: body
    spacing: 10

    readonly property var bar: Config.gameBar
    readonly property string style: bar["crosshair-style"] ?? "cross"
    function pick(key, fallback) { return bar[key] ?? fallback }
    readonly property var styles: [
        { id: "dot", text: qsTr("Dot") }, { id: "cross", text: qsTr("Cross") },
        { id: "gap", text: qsTr("Gap") }, { id: "circle", text: qsTr("Circle") }
    ]
    readonly property var colors: ["#00ff66", "#ff3040", "#00e5ff", "#ffe100", "#ffffff", "#ff40ff"]

    component SliderRow: Item {
        id: row
        property string key
        property string text
        property real from
        property real to
        property real def
        width: body.width
        height: 32
        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: 84
            text: row.text
            color: Theme.text
            font.pixelSize: Theme.fontBody
        }
        BarSlider {
            x: 88
            width: parent.width - 88 - 44
            anchors.verticalCenter: parent.verticalCenter
            from: row.from
            to: row.to
            stepSize: (row.to - row.from) > 5 ? 1 : 0.05
            live: false
            value: body.pick(row.key, row.def)
            onReleased: value => Config.setGameBar(row.key, Math.round(value * 100) / 100)
        }
        Text {
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            text: Math.round(body.pick(row.key, row.def) * 100) / 100
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            font.features: { "tnum": 1 }
        }
    }

    GameSwitchRow {
        text: qsTr("Show the crosshair")
        checked: GameNumbers.crosshairOn
        onToggled: on => Config.setGameBar("crosshair", on)
    }
    Flow {
        width: parent.width
        spacing: 4
        Repeater {
            model: body.styles
            delegate: Button {
                required property var modelData
                text: modelData.text
                accent: body.style === modelData.id
                onClicked: Config.setGameBar("crosshair-style", modelData.id)
            }
        }
    }
    Row {
        spacing: 8
        Repeater {
            model: body.colors
            delegate: Rectangle {
                required property string modelData
                width: 24
                height: 24
                radius: 12
                color: modelData
                border.width: String(body.pick("crosshair-color", "#00ff66")).toLowerCase() === modelData ? 2 : 1
                border.color: String(body.pick("crosshair-color", "#00ff66")).toLowerCase() === modelData ? Theme.text : Theme.stroke
                MouseArea { anchors.fill: parent; onClicked: Config.setGameBar("crosshair-color", parent.modelData) }
            }
        }
    }
    SliderRow { key: "crosshair-size"; text: qsTr("Size"); from: 4; to: 128; def: 24 }
    SliderRow { key: "crosshair-thickness"; text: qsTr("Thickness"); from: 1; to: 16; def: 2 }
    SliderRow { visible: body.style === "gap"; key: "crosshair-gap"; text: qsTr("Gap"); from: 0; to: 64; def: 4 }
    SliderRow { key: "crosshair-opacity"; text: qsTr("Opacity"); from: 0.1; to: 1; def: 1 }
    GameSwitchRow {
        text: qsTr("Outline")
        checked: body.pick("crosshair-outline", true) === true
        onToggled: on => Config.setGameBar("crosshair-outline", on)
    }
    Row {
        spacing: 8
        Button {
            icon: "view-restore"
            text: qsTr("Center")
            onClicked: {
                // On this output, in its exact center.
                if (GameNumbers.editScreen !== "") Config.setGameBar("crosshair-screen", GameNumbers.editScreen)
                Config.setGameBar("crosshair-x", 0)
                Config.setGameBar("crosshair-y", 0)
            }
        }
    }
    GameCaption { text: qsTr("Drag the crosshair on the screen to move it; double-click it to center it. The pin keeps it over the game when the Game Bar closes.") }
}
