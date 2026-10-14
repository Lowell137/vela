// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls
import Vela.Photos.Backend

// Edit, over the viewer: crop (with turning and flipping), the color
// adjustments, the filters and drawing on the image. Nothing is written
// until one of the two Save buttons.
Rectangle {
    id: page
    color: Theme.dialogFooter

    property url source
    signal finished(url saved) // none: cancelled
    signal failed(string message)
    signal overwriteRequested() // the window asks first, then save(true)

    property string tab: "crop"
    property real aspect: 0 // the crop's width over its height, in pixels; 0: free
    property string tool: "pen"
    property color ink: "#e81123"
    property real thickness: 0.006 // of the image's longer side

    function save(overwrite) {
        const error = view.save(overwrite)
        if (error !== "") failed(error)
        else finished(view.saved)
    }
    // The largest frame of that shape, in the middle.
    function setAspect(value) {
        aspect = value
        if (value <= 0) return
        const size = view.imageSize
        let w = 1
        let h = size.width / (size.height * value)
        if (h > 1) {
            h = 1
            w = size.height * value / size.width
        }
        view.crop = Qt.rect((1 - w) / 2, (1 - h) / 2, w, h)
    }

    Shortcut { sequence: "Escape"; enabled: page.visible; onActivated: page.finished("") }
    Shortcut { sequence: "Ctrl+S"; enabled: page.visible; onActivated: page.save(false) }
    Shortcut { sequence: "Ctrl+Z"; enabled: page.visible; onActivated: view.undoStroke() }

    MouseArea { anchors.fill: parent; hoverEnabled: true; onWheel: {} } // the viewer below isn't reached

    // --- what's being changed, and the way out ---
    Item {
        id: bar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 48
        Row {
            x: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            Repeater {
                model: [
                    { tab: "crop", text: qsTr("Crop"), icon: "transform-crop" },
                    { tab: "adjust", text: qsTr("Adjust"), icon: "adjustlevels" },
                    { tab: "filter", text: qsTr("Filter"), icon: "color-management" },
                    { tab: "markup", text: qsTr("Markup"), icon: "draw-freehand" }
                ]
                delegate: ToolButton {
                    required property var modelData
                    icon: modelData.icon
                    text: modelData.text
                    checked: page.tab === modelData.tab
                    onClicked: page.tab = modelData.tab
                }
            }
        }
        Row {
            anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
            spacing: 8
            ToolButton {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Reset")
                onClicked: {
                    view.reset()
                    page.aspect = 0
                }
            }
            Button { text: qsTr("Save as copy"); accent: true; onClicked: page.save(false) }
            Button { text: qsTr("Save"); onClicked: page.overwriteRequested() }
            Button { text: qsTr("Cancel"); onClicked: page.finished("") }
        }
    }

    // --- the image ---
    Item {
        id: stage
        anchors { left: parent.left; right: parent.right; top: bar.bottom; bottom: tools.top }

        EditView {
            id: view
            anchors.centerIn: parent
            readonly property real fit: imageSize.width > 0
                ? Math.min((stage.width - 48) / imageSize.width, (stage.height - 48) / imageSize.height) : 0
            width: imageSize.width * fit
            height: imageSize.height * fit
            source: page.source
            cropping: page.tab === "crop"
        }

        // The crop frame: dragged whole, or by a corner.
        Item {
            id: frame
            anchors.fill: view
            visible: page.tab === "crop"
            readonly property real x0: view.crop.x * width
            readonly property real y0: view.crop.y * height
            readonly property real x1: (view.crop.x + view.crop.width) * width
            readonly property real y1: (view.crop.y + view.crop.height) * height
            readonly property color shade: Qt.rgba(0, 0, 0, 0.6)

            Rectangle { width: parent.width; height: frame.y0; color: frame.shade }
            Rectangle { y: frame.y1; width: parent.width; height: parent.height - frame.y1; color: frame.shade }
            Rectangle { y: frame.y0; width: frame.x0; height: frame.y1 - frame.y0; color: frame.shade }
            Rectangle { x: frame.x1; y: frame.y0; width: parent.width - frame.x1; height: frame.y1 - frame.y0; color: frame.shade }
            Rectangle {
                x: frame.x0
                y: frame.y0
                width: frame.x1 - frame.x0
                height: frame.y1 - frame.y0
                color: "transparent"
                border.width: 1
                border.color: "white"
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.SizeAllCursor
                    property point grab
                    onPressed: mouse => grab = Qt.point(mouse.x, mouse.y)
                    onPositionChanged: mouse => {
                        const c = view.crop
                        const p = mapToItem(frame, mouse.x - grab.x, mouse.y - grab.y)
                        view.crop = Qt.rect(Math.max(0, Math.min(1 - c.width, p.x / frame.width)),
                            Math.max(0, Math.min(1 - c.height, p.y / frame.height)), c.width, c.height)
                    }
                }
            }
            Repeater {
                model: [Qt.point(0, 0), Qt.point(1, 0), Qt.point(0, 1), Qt.point(1, 1)]
                delegate: MouseArea {
                    id: handle
                    required property point modelData // which corner: 0 or 1 across, 0 or 1 down
                    x: (modelData.x ? frame.x1 : frame.x0) - 12
                    y: (modelData.y ? frame.y1 : frame.y0) - 12
                    width: 24
                    height: 24
                    cursorShape: modelData.x === modelData.y ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
                    property point anchor // the opposite corner, which stays
                    onPressed: {
                        const c = view.crop
                        anchor = Qt.point(modelData.x ? c.x : c.x + c.width, modelData.y ? c.y : c.y + c.height)
                    }
                    onPositionChanged: mouse => {
                        const least = 0.05
                        const p = mapToItem(frame, mouse.x, mouse.y)
                        // How far the corner may go from the one that stays.
                        const roomX = modelData.x ? 1 - anchor.x : anchor.x
                        const roomY = modelData.y ? 1 - anchor.y : anchor.y
                        let w = Math.max(least, Math.min(roomX, (modelData.x ? 1 : -1) * (p.x / frame.width - anchor.x)))
                        let h = Math.max(least, Math.min(roomY, (modelData.y ? 1 : -1) * (p.y / frame.height - anchor.y)))
                        if (page.aspect > 0) {
                            const k = view.imageSize.width / (view.imageSize.height * page.aspect) // h for a w of 1
                            h = w * k
                            if (h > roomY) {
                                h = roomY
                                w = h / k
                            }
                        }
                        view.crop = Qt.rect(modelData.x ? anchor.x : anchor.x - w, modelData.y ? anchor.y : anchor.y - h, w, h)
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: 12
                        height: 12
                        radius: 6
                        color: "white"
                        border.width: 1
                        border.color: Qt.rgba(0, 0, 0, 0.4)
                    }
                }
            }
        }

        MouseArea {
            anchors.fill: view
            visible: page.tab === "markup"
            cursorShape: Qt.CrossCursor
            function act(mouse, begins) {
                const u = mouse.x / width
                const v = mouse.y / height
                if (page.tool === "eraser") view.eraseAt(u, v)
                else if (!begins) view.strokeTo(u, v)
                else if (page.tool === "highlighter") view.beginStroke(Qt.rgba(page.ink.r, page.ink.g, page.ink.b, 0.4), page.thickness * 4, u, v)
                else view.beginStroke(page.ink, page.thickness, u, v)
            }
            onPressed: mouse => act(mouse, true)
            onPositionChanged: mouse => act(mouse, false)
        }
    }

    // --- the open tab's tools ---
    Item {
        id: tools
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 72

        Row {
            anchors.centerIn: parent
            visible: page.tab === "crop"
            spacing: 4
            Repeater {
                model: [
                    { text: qsTr("Free"), value: 0 },
                    { text: qsTr("Original"), value: -1 },
                    { text: "1:1", value: 1 },
                    { text: "4:3", value: 4 / 3 },
                    { text: "3:2", value: 3 / 2 },
                    { text: "16:9", value: 16 / 9 }
                ]
                delegate: ToolButton {
                    required property var modelData
                    readonly property real value: modelData.value < 0 ? view.imageSize.width / view.imageSize.height : modelData.value
                    text: modelData.text
                    checked: modelData.value < 0 ? false : Math.abs(page.aspect - value) < 0.001
                    onClicked: page.setAspect(value)
                }
            }
            Item { width: 16; height: 1 }
            ToolButton {
                icon: "object-rotate-right"
                tooltip: qsTr("Rotate")
                onClicked: {
                    view.rotate()
                    if (page.aspect > 0) page.aspect = 1 / page.aspect
                }
            }
            ToolButton { icon: "object-flip-horizontal"; tooltip: qsTr("Flip horizontally"); onClicked: view.flip(true) }
            ToolButton { icon: "object-flip-vertical"; tooltip: qsTr("Flip vertically"); onClicked: view.flip(false) }
        }

        Row {
            anchors.centerIn: parent
            visible: page.tab === "adjust"
            spacing: 24
            Repeater {
                model: [
                    { name: "brightness", text: qsTr("Brightness") },
                    { name: "contrast", text: qsTr("Contrast") },
                    { name: "saturation", text: qsTr("Saturation") }
                ]
                delegate: Column {
                    id: adjustment
                    required property var modelData
                    Text {
                        x: 10
                        text: adjustment.modelData.text + "  " + Math.round(slider.value * 100)
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                    }
                    Slider {
                        id: slider
                        from: -1
                        to: 1
                        origin: 0
                        resetValue: 0
                        onMoved: value => view[adjustment.modelData.name] = value
                        Connections { // Reset
                            target: view
                            function onChanged() { if (!slider.pressed) slider.value = view[adjustment.modelData.name] }
                        }
                    }
                }
            }
        }

        Row {
            anchors.centerIn: parent
            visible: page.tab === "filter"
            spacing: 4
            Repeater {
                model: [
                    { name: "", text: qsTr("Original") },
                    { name: "mono", text: qsTr("Black & white") },
                    { name: "sepia", text: qsTr("Sepia") },
                    { name: "warm", text: qsTr("Warm") },
                    { name: "cool", text: qsTr("Cool") },
                    { name: "vivid", text: qsTr("Vivid") }
                ]
                delegate: ToolButton {
                    required property var modelData
                    text: modelData.text
                    checked: view.filter === modelData.name
                    onClicked: view.filter = modelData.name
                }
            }
        }

        Row {
            anchors.centerIn: parent
            visible: page.tab === "markup"
            spacing: 4
            Repeater {
                model: [
                    { tool: "pen", text: qsTr("Pen") },
                    { tool: "highlighter", text: qsTr("Highlighter") },
                    { tool: "eraser", text: qsTr("Eraser") }
                ]
                delegate: ToolButton {
                    required property var modelData
                    text: modelData.text
                    checked: page.tool === modelData.tool
                    onClicked: page.tool = modelData.tool
                }
            }
            Item { width: 12; height: 1 }
            Repeater {
                model: ["#e81123", "#ffb900", "#107c10", "#0078d4", "#000000", "#ffffff"]
                delegate: Rectangle {
                    required property string modelData
                    readonly property bool chosen: Qt.colorEqual(page.ink, modelData)
                    anchors.verticalCenter: parent.verticalCenter
                    width: 26
                    height: 26
                    radius: 13
                    color: "transparent"
                    border.width: chosen ? 2 : 0
                    border.color: Theme.text
                    Rectangle {
                        anchors.centerIn: parent
                        width: 18
                        height: 18
                        radius: 9
                        color: parent.modelData
                        border.width: 1
                        border.color: Theme.controlStrokeStrong
                    }
                    MouseArea { anchors.fill: parent; onClicked: page.ink = parent.modelData }
                }
            }
            Item { width: 12; height: 1 }
            Repeater {
                model: [0.003, 0.006, 0.012] // the line's width
                delegate: ToolButton {
                    id: widthButton
                    required property real modelData
                    required property int index
                    width: 32
                    checked: page.thickness === modelData
                    onClicked: page.thickness = modelData
                    Rectangle {
                        anchors.centerIn: parent
                        width: 4 + widthButton.index * 4
                        height: width
                        radius: width / 2
                        color: Theme.text
                    }
                }
            }
            Item { width: 12; height: 1 }
            ToolButton {
                icon: "edit-undo"
                tooltip: qsTr("Undo (Ctrl+Z)")
                usable: view.hasStrokes
                onClicked: view.undoStroke()
            }
        }
    }
}
