// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Two panes sharing a place, side by side or one above the other, with the
// line between them to drag. Each can be a pane or another split.
Item {
    id: split
    readonly property bool isSplit: true
    property bool stacked: false // one above the other
    property real ratio: 0.5
    property Item first
    property Item second

    onWidthChanged: place()
    onHeightChanged: place()
    onRatioChanged: place()
    onFirstChanged: place()
    onSecondChanged: place()

    function place() {
        if (!first || !second) return
        const whole = stacked ? height : width
        const a = Math.round((whole - 1) * ratio)
        first.x = 0
        first.y = 0
        first.width = stacked ? width : a
        first.height = stacked ? a : height
        second.x = stacked ? 0 : a + 1
        second.y = stacked ? a + 1 : 0
        second.width = stacked ? width : width - a - 1
        second.height = stacked ? height - a - 1 : height
    }

    Rectangle {
        x: split.stacked ? 0 : Math.round((split.width - 1) * split.ratio)
        y: split.stacked ? Math.round((split.height - 1) * split.ratio) : 0
        width: split.stacked ? split.width : 1
        height: split.stacked ? 1 : split.height
        color: Theme.divider
        z: 2
        MouseArea {
            anchors.fill: parent
            anchors.margins: -3
            cursorShape: split.stacked ? Qt.SizeVerCursor : Qt.SizeHorCursor
            onPositionChanged: mouse => {
                const p = mapToItem(split, mouse.x, mouse.y)
                const whole = split.stacked ? split.height : split.width
                split.ratio = Math.max(0.1, Math.min(0.9, (split.stacked ? p.y : p.x) / Math.max(1, whole)))
            }
        }
    }
}
