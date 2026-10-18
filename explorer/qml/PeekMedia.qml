// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtMultimedia
import QtQuick.Templates as T
import Vela.Controls

// A video or a sound for the quick preview, playing at once; a click on the
// video pauses it. The bar: 10 s back, play/pause, 10 s forward, seek, time, speed (a click goes to
// the next one), mute and the volume, which opens under the pointer. Only built when QtMultimedia is there.
Item {
    id: media
    property string path: ""
    property string icon: "" // shown for a sound

    function clock(ms) {
        const s = Math.floor(ms / 1000)
        return Math.floor(s / 60) + ":" + String(s % 60).padStart(2, "0")
    }
    // A thin slider (the fork's controls have none): track, filled part, knob.
    component Bar: T.Slider {
        id: bar
        property alias usable: bar.enabled
        implicitHeight: 32
        padding: 6
        hoverEnabled: true
        wheelEnabled: true
        opacity: enabled ? 1 : 0.45
        background: Rectangle {
            x: bar.leftPadding
            y: bar.height / 2 - 2
            width: bar.availableWidth
            height: 4
            radius: 2
            color: Theme.controlStrokeStrong
            Rectangle { width: bar.visualPosition * parent.width; height: parent.height; radius: 2; color: Theme.accentFill }
        }
        handle: Rectangle {
            x: bar.leftPadding + bar.visualPosition * bar.availableWidth - width / 2
            y: bar.height / 2 - height / 2
            width: bar.hovered || bar.pressed ? 14 : 12
            height: width
            radius: width / 2
            color: Theme.accentFill
        }
    }
    readonly property var speeds: [0.5, 1, 1.25, 1.5, 2]
    function skip(ms) { player.position = Math.max(0, Math.min(player.duration, player.position + ms)) }
    function toggle() { if (player.playbackState === MediaPlayer.PlayingState) player.pause(); else player.play() }

    MediaPlayer {
        id: player
        objectName: "player"
        source: media.path !== "" ? "file://" + encodeURI(media.path).replace(/#/g, "%23").replace(/\?/g, "%3F") : ""
        videoOutput: video
        audioOutput: AudioOutput { id: sound }
        onSourceChanged: if (source.toString() !== "") play()
    }
    VideoOutput {
        id: video
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: bar.top; bottomMargin: 8 }
        visible: player.hasVideo
        MouseArea { anchors.fill: parent; onClicked: media.toggle() }
    }
    Image {
        anchors.centerIn: video
        visible: !player.hasVideo
        width: 128
        height: 128
        source: media.icon !== "" ? "image://fileicon/" + encodeURIComponent(media.icon) : ""
        sourceSize: Qt.size(width, height)
    }
    Row {
        id: bar
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        spacing: 8
        ToolButton {
            icon: "media-seek-backward"
            tooltip: qsTr("Back 10 seconds")
            usable: player.seekable
            onClicked: media.skip(-10000)
            width: 36
        }
        ToolButton {
            icon: player.playbackState === MediaPlayer.PlayingState ? "media-playback-pause" : "media-playback-start"
            tooltip: player.playbackState === MediaPlayer.PlayingState ? qsTr("Pause") : qsTr("Play")
            onClicked: media.toggle()
            width: 36
        }
        ToolButton {
            icon: "media-seek-forward"
            tooltip: qsTr("Forward 10 seconds")
            usable: player.seekable
            onClicked: media.skip(10000)
            width: 36
        }
        Bar {
            id: seek
            width: bar.width - 3 * 36 - time.width - speed.width - volumeBox.width - 6 * bar.spacing
            from: 0
            to: Math.max(1, player.duration)
            value: player.position
            usable: player.seekable
            onMoved: player.position = value
        }
        Text {
            id: time
            anchors.verticalCenter: parent.verticalCenter
            text: media.clock(player.position) + " / " + media.clock(player.duration)
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
        }
        ToolButton {
            id: speed
            text: player.playbackRate + "x"
            tooltip: qsTr("Speed")
            onClicked: player.playbackRate = media.speeds[(media.speeds.indexOf(player.playbackRate) + 1) % media.speeds.length]
        }
        // The volume slider opens while the pointer is over the speaker or it.
        Row {
            id: volumeBox
            HoverHandler { id: volumeHover }
            ToolButton {
                icon: sound.muted || sound.volume === 0 ? "audio-volume-muted" : sound.volume < 0.5 ? "audio-volume-low" : "audio-volume-high"
                tooltip: sound.muted ? qsTr("Unmute") : qsTr("Mute")
                onClicked: sound.muted = !sound.muted
                width: 36
            }
            Bar {
                id: volume
                width: volumeHover.hovered || pressed ? 90 : 0
                visible: width > 0
                Behavior on width { NumberAnimation { duration: Theme.fast } }
                from: 0
                to: 1
                value: sound.volume
                onMoved: { sound.volume = value; sound.muted = false }
            }
        }
    }
}
