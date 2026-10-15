// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Shapes
import Vela.Controls

// Media widget: what plays now (MPRIS through Media, see mediakeys.h): the
// cover, the track, the controls, the position and the volume. Pinned over
// the game it stays clickable on its own card (GameOverlay.qml).
Column {
    id: body
    spacing: 10

    readonly property var player: Media.current
    readonly property bool has: player.service !== undefined
    readonly property real length: has ? (player.length ?? 0) : 0
    readonly property real position: Math.max(0, Media.position)
    property real volumeBeforeMute: 0.5
    readonly property bool ownVolume: has && (player.volume ?? -1) >= 0
    readonly property real volume: ownVolume ? player.volume : Audio.volume
    readonly property bool muted: ownVolume ? player.volume === 0 : Audio.muted

    Component.onCompleted: Media.watchPosition(true)
    Component.onDestruction: Media.watchPosition(false)

    function clock(s) {
        const t = Math.max(0, Math.floor(s))
        return Math.floor(t / 60) + ":" + String(t % 60).padStart(2, "0")
    }

    component Glyph: Item {
        id: button
        property string kind
        property string label
        property bool usable: true
        signal clicked()
        width: 44
        height: 36
        opacity: usable ? 1 : 0.4
        Accessible.role: Accessible.Button
        Accessible.name: label
        Rectangle {
            anchors.fill: parent
            radius: Theme.radius
            color: Theme.subtleHover
            opacity: glyphMouse.containsMouse && button.usable ? 1 : 0
        }
        Shape {
            anchors.centerIn: parent
            width: 20
            height: 20
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillColor: Theme.text
                strokeColor: "transparent"
                PathSvg {
                    path: ({
                        previous: "M3.5 4h2v12h-2z M16.5 4L7 10l9.5 6z",
                        play: "M6 3.5L17 10L6 16.5z",
                        pause: "M5 3.5h3.5v13H5z M11.5 3.5H15v13h-3.5z",
                        next: "M14.5 4h2v12h-2z M3.5 4L13 10l-9.5 6z"
                    })[button.kind]
                }
            }
        }
        MouseArea {
            id: glyphMouse
            anchors.fill: parent
            hoverEnabled: true
            enabled: button.usable
            onClicked: button.clicked()
        }
    }

    GameCaption {
        visible: !body.has
        text: qsTr("Nothing is playing.")
    }

    Row {
        visible: body.has
        width: parent.width
        spacing: 12
        Rectangle {
            id: cover
            width: 72
            height: 72
            radius: Theme.radius
            color: Theme.subtleHover
            clip: true
            Image {
                anchors.centerIn: parent
                width: 32
                height: 32
                visible: art.status !== Image.Ready
                source: Theme.icons + "audio-x-generic"
                sourceSize: Qt.size(width, height)
            }
            Image {
                id: art
                anchors.fill: parent
                visible: status === Image.Ready
                source: body.player.artUrl || ""
                sourceSize: Qt.size(144, 144)
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: false // players reuse one file for every track
            }
        }
        Column {
            width: parent.width - cover.width - 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text {
                width: parent.width
                text: body.player.title || body.player.identity || ""
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: body.player.artist || ""
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                elide: Text.ElideRight
            }
            Row {
                width: parent.width
                spacing: 6
                Text {
                    width: parent.width - (more.visible ? more.width + 6 : 0)
                    text: body.player.identity || ""
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    elide: Text.ElideRight
                }
                // Several players: the next one, and which.
                Rectangle {
                    id: more
                    visible: (body.player.count ?? 0) > 1
                    width: moreText.implicitWidth + 12
                    height: 20
                    radius: Theme.radius
                    color: moreMouse.containsMouse ? Theme.subtleHover : Theme.control
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Next player")
                    Text {
                        id: moreText
                        anchors.centerIn: parent
                        text: body.player.index + "/" + body.player.count + " \u203a"
                        color: Theme.text
                        font.pixelSize: Theme.fontCaption
                    }
                    MouseArea {
                        id: moreMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: Media.cycle(1)
                    }
                }
            }
        }
    }

    // The position: a click seeks, if the player can.
    Item {
        visible: body.has
        width: parent.width
        height: 28
        Rectangle {
            id: track
            x: 0
            width: parent.width
            y: 6
            height: 4
            radius: 2
            color: Theme.control
            Rectangle {
                width: body.length > 0 ? parent.width * Math.min(1, body.position / body.length) : 0
                height: parent.height
                radius: 2
                color: Theme.accent
            }
        }
        MouseArea {
            x: 0
            y: 0
            width: parent.width
            height: 16
            enabled: body.player.canSeek === true && body.length > 0
            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: mouse => Media.seekTo(body.length * Math.max(0, Math.min(1, mouse.x / width)))
        }
        Text {
            anchors { left: parent.left; bottom: parent.bottom }
            text: body.clock(body.position)
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            font.features: { "tnum": 1 }
        }
        Text {
            anchors { right: parent.right; bottom: parent.bottom }
            text: body.length > 0 ? body.clock(body.length) : ""
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            font.features: { "tnum": 1 }
        }
    }

    Row {
        visible: body.has
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 12
        Glyph {
            kind: "previous"
            label: qsTr("Previous")
            usable: body.player.canGoPrevious === true
            onClicked: Media.command("previous")
        }
        Glyph {
            kind: body.player.playing ? "pause" : "play"
            label: body.player.playing ? qsTr("Pause") : qsTr("Play")
            onClicked: Media.command("play-pause")
        }
        Glyph {
            kind: "next"
            label: qsTr("Next")
            usable: body.player.canGoNext === true
            onClicked: Media.command("next")
        }
    }

    // The player's own volume, or the system output's if it has none.
    GameVolume {
        visible: body.has
        icon: "audio-volume-high"
        value: body.volume
        muted: body.muted
        onMoved: value => {
            if (body.ownVolume) Media.setVolume(value)
            else Audio.setVolume("output", Audio.defaultOutput, value)
        }
        onMuteClicked: {
            if (body.ownVolume) {
                if (body.muted) Media.setVolume(body.volumeBeforeMute)
                else {
                    body.volumeBeforeMute = body.volume
                    Media.setVolume(0)
                }
            } else {
                Audio.setMuted("output", Audio.defaultOutput, !Audio.muted)
            }
        }
    }
}
