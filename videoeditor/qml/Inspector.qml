// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The selected clip's settings, at the right: speed, volume and fades,
// turning, flipping and cropping; a text's words, size, color and place.
// A slider changes the clip when it's let go: one undo step.
Rectangle {
    id: panel
    property var win // Main.qml
    readonly property var clip: win.selected
    readonly property int track: win.selTrack
    readonly property string kind: clip ? clip.kind : ""
    readonly property bool visual: kind === "video" || kind === "image"
    color: Theme.background

    function set(key, value) { if (clip) Project.set(track, clip.index, key, value) }

    component Heading: Text {
        topPadding: 8
        color: Theme.text
        font.pixelSize: Theme.fontBody
        font.weight: Font.DemiBold
    }
    // A slider with its name and value above it.
    component SliderRow: Column {
        id: sliderRow
        property string title
        property string key
        property real value
        property real from: 0
        property real to: 1
        property real stepSize: 0
        property var format: v => v.toFixed(1)
        width: parent.width
        Item {
            width: parent.width
            height: 20
            Text { text: sliderRow.title; color: Theme.text; font.pixelSize: Theme.fontBody }
            Text {
                anchors.right: parent.right
                text: sliderRow.format(slider.value)
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }
        }
        Slider {
            id: slider
            width: parent.width
            from: sliderRow.from
            to: sliderRow.to
            stepSize: sliderRow.stepSize
            onReleased: v => panel.set(sliderRow.key, v)
        }
        Binding {
            target: slider
            property: "value"
            value: sliderRow.value
            when: !slider.pressed
            restoreMode: Binding.RestoreNone
        }
    }
    Rectangle { width: 1; height: parent.height; color: Theme.divider }

    Text {
        visible: !panel.clip
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 24 }
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("Select a clip on the timeline to change it")
        color: Theme.textSecondary
        font.pixelSize: Theme.fontBody
        wrapMode: Text.Wrap
    }

    Flickable {
        visible: !!panel.clip
        anchors { fill: parent; leftMargin: 17; rightMargin: 16; topMargin: 12 }
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentHeight: column.height + 16

        Column {
            id: column
            width: parent.width
            spacing: 10

            Text {
                width: parent.width
                text: !panel.clip ? "" : panel.kind === "text" ? qsTr("Text") : panel.kind === "gap" ? qsTr("Empty space") : panel.clip.name
                color: Theme.text
                font.pixelSize: Theme.fontBodyLarge
                font.weight: Font.DemiBold
                elide: Text.ElideMiddle
            }
            Text {
                text: panel.clip ? qsTr("Length %1").arg(panel.win.clock(panel.clip.length)) : ""
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }

            // --- a text ---
            TextBox {
                visible: panel.kind === "text"
                width: parent.width
                text: panel.clip ? panel.clip.text : ""
                placeholderText: qsTr("Your text")
                onEditingFinished: if (panel.clip && text !== panel.clip.text) panel.set("text", text)
            }
            SliderRow {
                visible: panel.kind === "text"
                title: qsTr("Size")
                key: "textSize"
                value: panel.clip ? panel.clip.textSize : 0
                from: 16
                to: 200
                stepSize: 2
                format: v => Math.round(v)
            }
            Heading { visible: panel.kind === "text"; text: qsTr("Color") }
            Flow {
                visible: panel.kind === "text"
                width: parent.width
                spacing: 6
                Repeater {
                    model: ["#ffffff", "#000000", "#ffd400", "#ff4b4b", "#3aa0ff", "#38c172", "#c264fe"]
                    delegate: Rectangle {
                        required property string modelData
                        width: 28
                        height: 28
                        radius: 14
                        color: modelData
                        border.width: panel.clip && panel.clip.color === modelData ? 3 : 1
                        border.color: panel.clip && panel.clip.color === modelData ? Theme.accentFill : Theme.controlStrokeStrong
                        MouseArea { anchors.fill: parent; onClicked: panel.set("color", parent.modelData) }
                    }
                }
            }
            Heading { visible: panel.kind === "text"; text: qsTr("Position") }
            Flow {
                visible: panel.kind === "text"
                width: parent.width
                spacing: 6
                Repeater {
                    model: [{ value: "top", text: qsTr("Top") }, { value: "center", text: qsTr("Center") }, { value: "bottom", text: qsTr("Bottom third") }]
                    delegate: Chip {
                        required property var modelData
                        text: modelData.text
                        checked: panel.clip && panel.clip.position === modelData.value
                        onClicked: panel.set("position", modelData.value)
                    }
                }
            }

            // --- how long a picture or a text stays ---
            SliderRow {
                visible: panel.kind === "image" || panel.kind === "text"
                title: qsTr("Duration")
                key: "length"
                value: panel.clip ? panel.clip.length : 0
                from: 0.5
                to: 30
                stepSize: 0.5
                format: v => qsTr("%1 s").arg(v.toFixed(1))
            }

            // --- sound and speed ---
            SliderRow {
                visible: panel.kind === "video" || panel.kind === "audio"
                title: qsTr("Speed")
                key: "speed"
                value: panel.clip ? panel.clip.speed : 1
                from: 0.25
                to: 4
                stepSize: 0.05
                format: v => v.toFixed(2) + "×"
            }
            SliderRow {
                visible: (panel.kind === "video" && panel.clip.hasAudio) || panel.kind === "audio"
                title: qsTr("Volume")
                key: "volume"
                value: panel.clip ? panel.clip.volume : 1
                from: 0
                to: 2
                stepSize: 0.05
                format: v => Math.round(v * 100) + "%"
            }
            Item {
                visible: (panel.kind === "video" && panel.clip.hasAudio) || panel.kind === "audio"
                width: parent.width
                height: 32
                Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Mute"); color: Theme.text; font.pixelSize: Theme.fontBody }
                Toggle {
                    anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                    showLabel: false
                    checked: panel.clip ? panel.clip.muted : false
                    onToggled: on => panel.set("muted", on)
                }
            }

            // --- fades ---
            SliderRow {
                visible: panel.visual || panel.kind === "audio"
                title: qsTr("Fade in")
                key: "fadeIn"
                value: panel.clip ? panel.clip.fadeIn : 0
                to: panel.clip ? Math.max(0.1, Math.min(5, panel.clip.length / 2)) : 1
                stepSize: 0.1
                format: v => qsTr("%1 s").arg(v.toFixed(1))
            }
            SliderRow {
                visible: panel.visual || panel.kind === "audio"
                title: qsTr("Fade out")
                key: "fadeOut"
                value: panel.clip ? panel.clip.fadeOut : 0
                to: panel.clip ? Math.max(0.1, Math.min(5, panel.clip.length / 2)) : 1
                stepSize: 0.1
                format: v => qsTr("%1 s").arg(v.toFixed(1))
            }

            // --- the picture's shape ---
            Heading { visible: panel.visual; text: qsTr("Rotate and flip") }
            Row {
                visible: panel.visual
                spacing: 4
                ToolButton { icon: "object-rotate-right"; tooltip: qsTr("Rotate 90°"); onClicked: panel.set("rotate", panel.clip.rotate + 90) }
                ToolButton { icon: "object-flip-horizontal"; tooltip: qsTr("Flip horizontally"); checked: !!panel.clip && panel.clip.flipH; onClicked: panel.set("flipH", !panel.clip.flipH) }
                ToolButton { icon: "object-flip-vertical"; tooltip: qsTr("Flip vertically"); checked: !!panel.clip && panel.clip.flipV; onClicked: panel.set("flipV", !panel.clip.flipV) }
            }
            Heading { visible: panel.visual; text: qsTr("Crop") }
            Flow {
                visible: panel.visual
                width: parent.width
                spacing: 6
                Repeater {
                    model: [{ value: "", text: qsTr("Original") }, { value: "16:9", text: "16:9" }, { value: "9:16", text: "9:16" },
                        { value: "1:1", text: "1:1" }, { value: "4:3", text: "4:3" }]
                    delegate: Chip {
                        required property var modelData
                        text: modelData.text
                        checked: panel.clip && panel.clip.crop === modelData.value
                        onClicked: panel.set("crop", modelData.value)
                    }
                }
            }

            // --- a gap ---
            Text {
                visible: panel.kind === "gap"
                width: parent.width
                text: qsTr("Black and silent in the video. Delete it to close the gap.")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                wrapMode: Text.Wrap
            }

            Item { width: 1; height: 8 }
            Row {
                spacing: 8
                Button {
                    visible: panel.kind !== "gap"
                    icon: "edit-cut"
                    text: qsTr("Split")
                    onClicked: panel.win.split()
                }
                Button {
                    icon: "edit-delete"
                    text: qsTr("Delete")
                    onClicked: panel.win.removeSelected(true)
                }
            }
        }
    }
}
