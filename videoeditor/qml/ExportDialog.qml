// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Export: the choices, then where to save, then ffmpeg's progress with the
// time left, and at the end the file in its folder or what went wrong.
Dialog {
    id: dialog
    property var win // Main.qml
    property string phase: "options" // options, running, done, failed
    property string resolution: "source"
    property real fps: 0
    property string quality: "high"
    property string format: "mp4"
    dialogWidth: 520

    readonly property var resolutions: [
        { value: "source", text: qsTr("Source"), width: 0, height: 0 },
        { value: "1080p", text: "1080p", width: 1920, height: 1080 },
        { value: "720p", text: "720p", width: 1280, height: 720 },
        { value: "480p", text: "480p", width: 854, height: 480 },
        { value: "vertical", text: qsTr("Vertical 1080×1920"), width: 1080, height: 1920 }
    ]
    readonly property bool audioOnly: format === "mp3" || format === "ogg"

    function start() {
        quality = win.defaultQuality
        phase = "options"
        open()
    }

    title: phase === "options" ? qsTr("Export the video")
         : phase === "running" ? qsTr("Exporting...")
         : phase === "done" ? qsTr("Your video is ready") : qsTr("The export failed")
    text: phase === "done" ? Exporter.output : phase === "failed" ? Exporter.error : ""
    buttons: phase === "options" ? [{ text: qsTr("Export"), accent: true, value: "export" }, { text: qsTr("Cancel"), value: "cancel" }]
           : phase === "running" ? [{ text: qsTr("Cancel"), value: "cancel" }]
           : phase === "done" ? [{ text: qsTr("Show in folder"), accent: true, value: "show" }, { text: qsTr("Close"), value: "cancel" }]
           : [{ text: qsTr("Close"), value: "cancel" }]

    onChosen: value => {
        if (phase === "running") {
            Exporter.cancel()
            phase = "options"
        } else if (value === "export") {
            const extension = format === "mp4" ? ".mp4" : "." + format
            win.pickSave(win.exportFolder, win.projectName + extension, [qsTr("%1 files (*%2)").arg(format.toUpperCase()).arg(extension)], file => {
                const r = resolutions.find(x => x.value === resolution)
                Exporter.start(Project, {
                    format: format, width: r.width, height: r.height, fps: fps, quality: quality, hardware: win.hardwareEncoder
                }, win.localPath(file))
                phase = "running"
                open()
            })
        } else if (value === "show") {
            Media.showInFolder(Exporter.output)
        }
    }
    Connections {
        target: Exporter
        function onFinished(ok) {
            dialog.phase = ok ? "done" : "failed"
            dialog.open()
        }
    }

    component Choices: Column {
        id: choices
        property string title
        property var model
        property var current
        signal picked(var value)
        width: parent.width
        spacing: 6
        Text { text: choices.title; color: Theme.text; font.pixelSize: Theme.fontBody }
        Flow {
            width: parent.width
            spacing: 6
            Repeater {
                model: choices.model
                delegate: Chip {
                    required property var modelData
                    text: modelData.text
                    checked: modelData.value === choices.current
                    onClicked: choices.picked(modelData.value)
                }
            }
        }
    }

    Column {
        width: parent.width
        spacing: 14
        visible: dialog.phase === "options"
        Choices {
            title: qsTr("Format")
            model: [{ value: "mp4", text: qsTr("MP4 video") }, { value: "webm", text: qsTr("WebM video") },
                { value: "mp3", text: qsTr("MP3 sound only") }, { value: "ogg", text: qsTr("OGG sound only") }]
            current: dialog.format
            onPicked: value => dialog.format = value
        }
        Choices {
            visible: !dialog.audioOnly
            title: qsTr("Resolution")
            model: dialog.resolutions
            current: dialog.resolution
            onPicked: value => dialog.resolution = value
        }
        Choices {
            visible: !dialog.audioOnly
            title: qsTr("Frame rate")
            model: [{ value: 0, text: qsTr("Source") }, { value: 24, text: "24" }, { value: 30, text: "30" }, { value: 60, text: "60" }]
            current: dialog.fps
            onPicked: value => dialog.fps = value
        }
        Choices {
            title: qsTr("Quality")
            model: [{ value: "high", text: qsTr("High") }, { value: "medium", text: qsTr("Medium") }, { value: "small", text: qsTr("Small file") }]
            current: dialog.quality
            onPicked: value => dialog.quality = value
        }
    }

    Column {
        width: parent.width
        spacing: 8
        visible: dialog.phase === "running"
        Rectangle {
            width: parent.width
            height: 4
            radius: 2
            color: Theme.controlStrokeStrong
            Rectangle {
                width: parent.width * Exporter.progress
                height: parent.height
                radius: 2
                color: Theme.accentFill
            }
        }
        Text {
            text: Math.round(Exporter.progress * 100) + "%"
                + (Exporter.remaining >= 0 ? " · " + qsTr("about %1 left").arg(dialog.win.duration(Exporter.remaining)) : "")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
        }
    }
}
