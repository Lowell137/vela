// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Performance widget: the numbers on the left, the last minute of the chosen
// one on the right.
Column {
    id: body
    spacing: 10

    readonly property var metrics: ["fps", "cpu", "gpu", "vram", "ram"]
    readonly property string metric: {
        const m = Config.gameBar["perf-metric"]
        return metrics.includes(m) ? m : "fps"
    }
    readonly property var history: metric === "fps" ? Game.fpsHistory : metric === "cpu" ? Game.cpuHistory
        : metric === "gpu" ? Game.gpuHistory : metric === "vram" ? Game.vramHistory : Game.ramHistory

    Row {
        width: parent.width
        spacing: 12
        Column {
            id: list
            width: 124
            spacing: 4
            Repeater {
                model: body.metrics
                delegate: Rectangle {
                    id: item
                    required property string modelData
                    width: list.width
                    height: 48
                    radius: Theme.radius
                    color: body.metric === modelData ? Theme.control : itemMouse.containsMouse ? Theme.subtleHover : "transparent"
                    border.width: body.metric === modelData ? 1 : 0
                    border.color: Theme.accent
                    Column {
                        x: 10
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            text: GameNumbers.label(item.modelData)
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                        }
                        Text {
                            text: GameNumbers.value(item.modelData)
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            font.features: { "tnum": 1 }
                        }
                    }
                    MouseArea {
                        id: itemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: Config.setGameBar("perf-metric", item.modelData)
                    }
                }
            }
        }
        Column {
            width: parent.width - list.width - 12
            spacing: 6
            Text {
                text: GameNumbers.label(body.metric)
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }
            Text {
                text: GameNumbers.value(body.metric)
                color: Theme.text
                font.pixelSize: Theme.fontTitle
                font.weight: Font.DemiBold
                font.features: { "tnum": 1 }
            }
            Sparkline {
                width: parent.width
                height: 90
                ceiling: body.metric === "fps" ? 0 : 100
                values: body.history
            }
            Text {
                text: body.metric === "fps" && Game.perfStale ? qsTr("Nothing is being drawn") : qsTr("Last minute")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }
        }
    }
    Flow {
        width: parent.width
        spacing: 12
        Repeater {
            model: ["frametime", "cputemp", "gputemp", "gpupower"]
            delegate: Text {
                required property string modelData
                text: GameNumbers.label(modelData) + " " + GameNumbers.value(modelData)
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                font.features: { "tnum": 1 }
            }
        }
    }
    Text {
        text: Game.mangoHudInstalled ? qsTr("MangoHud settings") : qsTr("MangoHud isn't installed")
        color: Game.mangoHudInstalled ? (linkMouse.containsMouse ? Theme.accentFillHover : Theme.link) : Theme.textSecondary
        font.pixelSize: Theme.fontCaption
        MouseArea {
            id: linkMouse
            anchors.fill: parent
            enabled: Game.mangoHudInstalled
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                GameNumbers.closeRequested(() => {})
                System.openSettingsPage("gaming")
            }
        }
    }
}
