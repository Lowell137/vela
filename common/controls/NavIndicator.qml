// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The selected row's pill in a navigation list (Settings, Files, the apps'
// side panes). One per list, a sibling of the rows: when the selection moves
// it slides to the new row stretching, its leading edge ahead of the trailing
// one, like Fluent-Qt's ListView (ListView.cpp, MIT: 250 ms OutCubic, lead
// 1.35x, trail delayed 0.18). A row calls track(row, selected).
Rectangle {
    id: pill
    property Item target: null
    property real length: 16
    property real inset: 0 // from the row's left edge

    // Where the pill goes for a row. `goal` reads the row's y in the binding,
    // so a moving row is followed; follow() calls goalOf() itself, as the
    // binding may not have caught up yet when the target changes.
    function goalOf(row) {
        return row ? row.mapToItem(parent, 0, (row.height - length) / 2).y : 0
    }
    readonly property real goal: {
        if (!target) {
            return 0
        }
        target.y
        target.height
        return goalOf(target)
    }
    property Item lastTarget: null
    property real from: 0
    property real to: 0
    property real p: 1
    readonly property real lead: Math.min(1, 1.35 * p)
    readonly property real trail: Math.max(0, Math.min(1, (p - 0.18) / 0.82))
    readonly property bool down: to >= from
    readonly property real edgeTop: from + (to - from) * (down ? trail : lead)
    readonly property real edgeBottom: from + length + (to - from) * (down ? lead : trail)

    function follow() {
        if (target === lastTarget) {
            // The same row moved (a scroll, a row added above): follow it.
            const g = goalOf(target)
            if (p < 1) {
                to = g
            } else {
                from = g
                to = g
            }
            return
        }
        from = lastTarget ? edgeTop : goalOf(target)
        to = goalOf(target)
        lastTarget = target
        if (from !== to) {
            p = 0 // at once: the goal's own change comes next and must see it
            slide.restart()
        }
    }
    // A row calls this from onSelectedChanged (and once when created). The
    // old row letting go waits a turn, so a new row taking over in the same
    // change makes the pill slide instead of jump.
    function track(row, selected) {
        if (selected) {
            target = row
        } else if (target === row) {
            Qt.callLater(() => { if (target === row) target = null })
        }
    }
    onGoalChanged: follow()
    onTargetChanged: follow()

    visible: target !== null
    z: 1
    x: target ? target.mapToItem(parent, inset, 0).x : 0
    y: edgeTop
    width: 3
    height: edgeBottom - edgeTop
    radius: 1.5
    color: Theme.accentFill

    NumberAnimation on p {
        id: slide
        running: false
        from: 0
        to: 1
        duration: Theme.normal
        easing.type: Easing.BezierSpline
        easing.bezierCurve: Theme.decelerate
    }
}
