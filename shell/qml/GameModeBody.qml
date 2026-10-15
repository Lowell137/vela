// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Game mode widget: fewer interruptions and more speed.
Column {
    id: body
    spacing: 10

    GameSwitchRow {
        text: qsTr("Do not disturb")
        checked: Notifications.doNotDisturb
        onToggled: on => Notifications.doNotDisturb = on
    }
    Rectangle { width: parent.width; height: 1; color: Theme.divider }
    // Feral's GameMode: priority for a game's process. It acts on the
    // focused app, so it says which one, and it isn't offered for apps that
    // aren't games.
    GameSwitchRow {
        visible: Game.gameModeInstalled
        text: qsTr("GameMode")
        usable: Game.isGame && Game.gameModeAvailable && Game.pid > 0
        opacity: usable ? 1 : 0.5
        checked: Game.gameMode
        onToggled: on => Game.setGameMode(on)
    }
    GameCaption {
        text: Game.gameModeInstalled
            ? qsTr("CPU and GPU priority for the game (Feral GameMode). Focused app: %1").arg(Game.appName !== "" ? Game.appName : qsTr("none"))
            : qsTr("The gamemode package isn't installed.")
    }
    GameCaption {
        visible: Game.gameModeInstalled && !Game.isGame
        text: qsTr("This app doesn't look like a game.")
    }
    GameCaption {
        visible: Status.powerSaverAvailable
        text: qsTr("Power mode")
    }
    Row {
        visible: Status.powerSaverAvailable
        spacing: 4
        Repeater {
            model: Status.powerProfiles
            delegate: Button {
                required property string modelData
                accent: Status.powerProfile === modelData
                text: modelData === "power-saver" ? qsTr("Power saver")
                    : modelData === "performance" ? qsTr("Performance") : qsTr("Balanced")
                onClicked: Status.powerProfile = modelData
            }
        }
    }
}
