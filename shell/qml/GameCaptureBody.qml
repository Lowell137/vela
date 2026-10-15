// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Capture widget: the whole output (Snipping Tool, wf-recorder) and the
// instant replay (gpu-screen-recorder).
Column {
    id: body
    spacing: 10

    function clock(s) {
        return Math.floor(s / 60) + ":" + String(s % 60).padStart(2, "0")
    }

    Flow {
        width: parent.width
        spacing: 8
        Button {
            icon: "accessories-screenshot"
            text: qsTr("Screenshot")
            onClicked: {
                const screen = GameNumbers.editScreen
                GameNumbers.closeRequested(() => Snip.captureOutput(screen))
            }
        }
        Button {
            icon: "media-record"
            text: Snip.recording ? qsTr("Stop %1").arg(body.clock(Snip.recordSeconds)) : qsTr("Record")
            onClicked: {
                if (Snip.recording) {
                    Snip.stopRecording()
                    return
                }
                const screen = GameNumbers.editScreen
                GameNumbers.closeRequested(() => Snip.recordOutput(screen))
            }
        }
        Button {
            visible: Game.replayAvailable
            usable: Game.replay
            icon: "document-save"
            text: qsTr("Last 30 s")
            onClicked: Game.saveReplay()
        }
    }
    GameSwitchRow {
        text: qsTr("Record the microphone")
        checked: Snip.microphone
        usable: !Snip.recording
        onToggled: Snip.toggleMicrophone()
    }
    GameSwitchRow {
        visible: Game.replayAvailable
        text: qsTr("Instant replay")
        checked: Game.replay
        onToggled: on => Game.setReplay(on)
    }
    GameCaption {
        text: Game.replayAvailable ? qsTr("Screenshots go to Pictures > Screenshot, recordings to Videos > Screen Recordings, replays to Videos > Captures.")
                                   : qsTr("Screenshots go to Pictures > Screenshot, recordings to Videos > Screen Recordings. Install gpu-screen-recorder for instant replay.")
    }
    Button {
        icon: "folder-pictures"
        text: qsTr("Show all captures")
        onClicked: {
            GameNumbers.closeRequested(() => System.openUrl("file://" + System.homeDirectory() + "/Pictures/Screenshot"))
        }
    }
}
