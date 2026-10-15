// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The timeline, like Clipchamp's: the ruler with the playhead, then the text,
// video and audio tracks. A clip is selected by a click, moved by its middle
// and trimmed by its edges; while dragged it snaps to the other clips' edges
// and the playhead, and the edit happens when it's let go (one undo step).
Rectangle {
    id: view
    property var win // Main.qml
    property real pps: 40 // pixels per second: the zoom
    readonly property int header: 44 // the tracks' icons
    readonly property int rulerHeight: 28
    readonly property real snapTolerance: 8 / pps
    color: Theme.background

    // The tracks, top to bottom: [track, height, icon].
    readonly property var rows: [
        { track: 2, y: rulerHeight + 6, height: 32, icon: "draw-text", tip: qsTr("Text") },
        { track: 0, y: rulerHeight + 42, height: 64, icon: "video-x-generic", tip: qsTr("Video") },
        { track: 1, y: rulerHeight + 110, height: 40, icon: "audio-x-generic", tip: qsTr("Audio") }
    ]
    function row(track) { return rows.find(r => r.track === track) }
    function clock(seconds) {
        const s = Math.max(0, Math.round(seconds))
        return Math.floor(s / 60) + ":" + String(s % 60).padStart(2, "0")
    }

    // Window coordinates to the timeline's seconds, and whether a point is
    // over the tracks (a file dropped there goes on the timeline).
    function timeAt(windowX) {
        return Math.max(0, (view.mapFromItem(null, windowX, 0).x - header + scroller.contentX) / pps)
    }
    function overTracks(windowX, windowY) {
        const p = view.mapFromItem(null, windowX, windowY)
        return p.x >= header && p.x < width && p.y >= rulerHeight && p.y < height
    }
    // Keeps the playhead in sight while playing.
    function follow() {
        const x = win.playhead * pps
        if (x < scroller.contentX || x > scroller.contentX + scroller.width - 40) {
            scroller.contentX = Math.max(0, Math.min(scroller.contentWidth - scroller.width, x - 40))
        }
    }
    // The zoom changes around a point of the view (the pointer, the playhead).
    function zoomTo(value, aroundX) {
        const at = (scroller.contentX + aroundX) / pps
        pps = Math.max(2, Math.min(400, value))
        scroller.contentX = Math.max(0, at * pps - aroundX)
    }
    function fit() {
        pps = Math.max(2, Math.min(400, (scroller.width - 40) / Math.max(1, Project.length)))
        scroller.contentX = 0
    }

    // The tracks' icons, at the left.
    Repeater {
        model: view.rows
        delegate: Image {
            required property var modelData
            x: (view.header - 16) / 2
            y: modelData.y + (modelData.height - 16) / 2
            width: 16
            height: 16
            source: Theme.icons + modelData.icon
            sourceSize: Qt.size(16, 16)
            opacity: 0.7
        }
    }

    Flickable {
        id: scroller
        anchors { fill: parent; leftMargin: view.header; bottomMargin: 10 }
        clip: true
        interactive: false // dragging is for the clips
        contentWidth: Math.max(width, (Project.length + 30) * view.pps)
        contentHeight: height

        // An empty place on a track or the ruler: the playhead goes there.
        MouseArea {
            width: scroller.contentWidth
            height: scroller.height
            onPressed: mouse => {
                view.win.select(-1, -1)
                view.win.seek(mouse.x / view.pps)
            }
            onPositionChanged: mouse => view.win.seek(Math.max(0, mouse.x / view.pps))
        }

        // The ruler: a label every step that leaves room for one.
        Item {
            id: ruler
            width: scroller.contentWidth
            height: view.rulerHeight
            readonly property real step: [0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 1800].find(s => s * view.pps >= 70) || 3600
            readonly property int first: Math.floor(scroller.contentX / view.pps / step)
            Repeater {
                model: Math.ceil(scroller.width / view.pps / ruler.step) + 2
                delegate: Item {
                    required property int index
                    readonly property real at: (ruler.first + index) * ruler.step
                    x: at * view.pps
                    height: ruler.height
                    Rectangle { y: 16; width: 1; height: 12; color: Theme.textTertiary }
                    Rectangle { x: ruler.step * view.pps / 2; y: 22; width: 1; height: 6; color: Theme.textTertiary }
                    Text {
                        x: 4
                        y: 2
                        text: ruler.step < 1 ? parent.at.toFixed(1) + "s" : view.clock(parent.at)
                        color: Theme.textSecondary
                        font.pixelSize: 11
                    }
                }
            }
        }

        // The tracks' lanes.
        Repeater {
            model: view.rows
            delegate: Rectangle {
                required property var modelData
                y: modelData.y
                width: scroller.contentWidth
                height: modelData.height
                radius: Theme.radius
                color: Theme.layer
                border.width: 1
                border.color: Theme.layerStroke
            }
        }

        Repeater {
            model: Project.texts
            delegate: ClipBlock { track: 2 }
        }
        Repeater {
            model: Project.video
            delegate: ClipBlock { track: 0 }
        }
        Repeater {
            model: Project.audio
            delegate: ClipBlock { track: 1 }
        }

        // Where a dragged video clip would go.
        Rectangle {
            id: insertMark
            visible: false
            y: view.row(0).y - 4
            width: 3
            height: view.row(0).height + 8
            radius: 1.5
            color: Theme.accentFill
            z: 20
        }

        // The playhead.
        Item {
            x: view.win.playhead * view.pps - 6
            width: 12
            height: scroller.height
            z: 30
            Rectangle { x: 5; y: 8; width: 2; height: parent.height - 8; color: Theme.text }
            Rectangle { x: 0; y: 4; width: 12; height: 12; radius: 6; color: Theme.text }
        }

        // Files from the file manager go on the timeline where they're dropped.
        DropArea {
            width: scroller.contentWidth
            height: scroller.height
            keys: ["text/uri-list"]
            onDropped: drop => {
                if (drop.hasUrls) Media.add(drop.urls, true, drop.x / view.pps)
            }
        }
    }

    // A horizontal scroll bar, thin until hovered.
    Item {
        anchors { left: scroller.left; right: scroller.right; bottom: parent.bottom }
        height: 10
        visible: scroller.contentWidth > scroller.width
        HoverHandler { id: barHover }
        Rectangle {
            x: scroller.contentX / scroller.contentWidth * parent.width
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(24, parent.width * scroller.width / scroller.contentWidth)
            height: barHover.hovered || barDrag.pressed ? 8 : 4
            radius: height / 2
            color: barHover.hovered || barDrag.pressed ? Theme.scrollThumbHover : Theme.scrollThumb
            MouseArea {
                id: barDrag
                anchors.fill: parent
                anchors.margins: -4
                property real from
                property real grab
                onPressed: mouse => { from = scroller.contentX; grab = mapToItem(view, mouse.x, 0).x }
                onPositionChanged: mouse => {
                    const dx = mapToItem(view, mouse.x, 0).x - grab
                    scroller.contentX = Math.max(0, Math.min(scroller.contentWidth - scroller.width, from + dx * scroller.contentWidth / scroller.width))
                }
            }
        }
    }

    // The wheel scrolls along; with Ctrl it zooms around the pointer.
    WheelHandler {
        target: null
        onWheel: event => {
            const delta = event.angleDelta.y !== 0 ? event.angleDelta.y : event.angleDelta.x
            if (event.modifiers & Qt.ControlModifier) {
                view.zoomTo(view.pps * (delta > 0 ? 1.25 : 0.8), event.x - view.header)
            } else {
                scroller.contentX = Math.max(0, Math.min(scroller.contentWidth - scroller.width, scroller.contentX - delta))
            }
        }
    }

    // A clip: its picture, name and badges; the middle moves it, the edges
    // trim it.
    component ClipBlock: Rectangle {
        id: block
        required property var modelData
        property int track
        readonly property var lane: view.row(track)
        readonly property bool selected: view.win.selTrack === track && view.win.selIndex === modelData.index
        readonly property bool gap: modelData.kind === "gap"
        // While dragged: the new start, head and tail (seconds).
        property real moveTo: modelData.start
        property real headTo: modelData.start
        property real tailTo: modelData.start + modelData.length
        property bool moving: false
        property bool trimming: false
        readonly property real shownStart: moving ? moveTo : trimming ? headTo : modelData.start
        readonly property real shownLength: moving ? modelData.length : trimming ? tailTo - headTo : modelData.length

        x: shownStart * view.pps
        y: lane.y + 2
        z: moving ? 10 : selected ? 2 : 1
        width: Math.max(2, shownLength * view.pps - 1)
        height: lane.height - 4
        radius: Theme.radius
        clip: true
        opacity: moving ? 0.85 : 1
        color: gap ? "transparent"
            : track === 2 ? Qt.rgba(0.53, 0.38, 0.85, 0.55)
            : track === 1 ? Qt.rgba(0.2, 0.62, 0.45, 0.55)
            : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
        border.width: selected ? 2 : 1
        border.color: selected ? Theme.text : gap ? Theme.controlStrokeStrong : Theme.controlStroke

        // The video's frames, side by side.
        Row {
            visible: block.track === 0 && !block.gap
            anchors.fill: parent
            anchors.margins: 2
            Repeater {
                model: visible ? Math.ceil(block.width / 96) : 0
                delegate: Image {
                    width: 96
                    height: block.height - 4
                    fillMode: Image.PreserveAspectCrop
                    source: Media.items.find(i => i.path === block.modelData.source)?.thumb ?? ""
                    sourceSize.height: height
                    asynchronous: true
                    opacity: 0.85
                }
            }
        }
        Text {
            visible: block.gap
            anchors.centerIn: parent
            width: Math.min(implicitWidth, parent.width - 8)
            text: qsTr("Empty")
            color: Theme.textTertiary
            font.pixelSize: Theme.fontCaption
            elide: Text.ElideRight
        }
        Rectangle {
            visible: !block.gap && block.width > 30
            x: 4
            y: 4
            width: Math.min(label.implicitWidth + 8, block.width - 8)
            height: label.implicitHeight + 2
            radius: 2
            color: Qt.rgba(0, 0, 0, 0.45)
            Text {
                id: label
                x: 4
                width: parent.width - 8
                text: (block.modelData.speed !== 1 ? block.modelData.speed + "× " : "")
                    + (block.track === 2 ? block.modelData.text : block.modelData.name)
                color: "white"
                font.pixelSize: 11
                elide: Text.ElideRight
            }
        }

        // Moving.
        MouseArea {
            id: body
            anchors { fill: parent; leftMargin: 6; rightMargin: 6 }
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            property real grabX
            onPressed: mouse => {
                view.win.select(block.track, block.modelData.index)
                grabX = mapToItem(scroller.contentItem, mouse.x, 0).x
                if (mouse.button === Qt.RightButton) {
                    const p = mapToItem(null, mouse.x, mouse.y)
                    view.win.openClipMenu(p.x, p.y)
                }
            }
            onPositionChanged: mouse => {
                if (!pressed || !(pressedButtons & Qt.LeftButton)) return
                const dx = mapToItem(scroller.contentItem, mouse.x, 0).x - grabX
                if (!block.moving && Math.abs(dx) < 4) return
                block.moving = true
                let to = Math.max(0, block.modelData.start + dx / view.pps)
                if (block.track !== 0) {
                    // Either edge snaps: whichever is nearer.
                    const head = Project.snap(to, view.snapTolerance, block.track, block.modelData.index, view.win.playhead)
                    const tail = Project.snap(to + block.modelData.length, view.snapTolerance, block.track, block.modelData.index, view.win.playhead) - block.modelData.length
                    to = Math.abs(head - to) <= Math.abs(tail - to) && head !== to ? head : tail !== to ? Math.max(0, tail) : to
                } else {
                    // The boundary it would land on, between the others.
                    let edge = 0
                    let best = 0
                    for (const c of Project.video) {
                        if (c.index === block.modelData.index) continue
                        edge += c.length
                        if (Math.abs(edge - to) < Math.abs(best - to)) best = edge
                    }
                    insertMark.x = best * view.pps - 1
                    insertMark.visible = true
                }
                block.moveTo = to
            }
            onReleased: {
                insertMark.visible = false
                if (!block.moving) return
                block.moving = false
                const index = Project.move(block.track, block.modelData.index, block.moveTo)
                view.win.select(block.track, index)
            }
            onCanceled: { block.moving = false; insertMark.visible = false }
            onDoubleClicked: view.win.seek(block.modelData.start)
        }

        // Trimming, from either edge.
        Edge { owner: block; head: true; visible: !block.gap }
        Edge { owner: block; head: false; x: block.width - width; visible: !block.gap }
    }

    // A clip's edge: dragged, it trims the clip there.
    component Edge: MouseArea {
        property var owner // its ClipBlock
        property bool head
        property real grab // the pointer's distance to the edge, in seconds
        width: 8
        height: parent.height
        cursorShape: Qt.SizeHorCursor
        hoverEnabled: true
        function pointer(mouse) { return mapToItem(scroller.contentItem, mouse.x, 0).x / view.pps }
        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: 2
            color: Theme.text
            opacity: parent.containsMouse || parent.pressed ? 0.5 : 0
        }
        onPressed: mouse => {
            const b = owner
            view.win.select(b.track, b.modelData.index)
            b.headTo = b.modelData.start
            b.tailTo = b.modelData.start + b.modelData.length
            grab = pointer(mouse) - (head ? b.headTo : b.tailTo)
            b.trimming = true
        }
        onPositionChanged: mouse => {
            if (!pressed) return
            const b = owner
            const at = Project.snap(pointer(mouse) - grab, view.snapTolerance, b.track, b.modelData.index, view.win.playhead)
            if (head) b.headTo = Math.min(at, b.tailTo - 0.1)
            else b.tailTo = Math.max(at, b.headTo + 0.1)
        }
        onReleased: {
            const b = owner
            b.trimming = false
            const at = head ? b.headTo : b.tailTo
            if (Math.abs(at - (head ? b.modelData.start : b.modelData.start + b.modelData.length)) > 0.001)
                Project.trim(b.track, b.modelData.index, head, at)
        }
        onCanceled: owner.trimming = false
    }
}
