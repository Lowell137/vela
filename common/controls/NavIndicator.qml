// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The selected row's pill in a navigation list (Settings, Files, the apps'
// side panes). One per list, a sibling of the rows. It does not travel, as in
// Windows 11: the old pill stretches toward the new row, then closes, and a
// new pill opens on the new row, a little long, and settles to its length. A
// selection that comes while that runs jumps straight there. A row calls
// track(row, selected). `animated: false` makes every change a jump.
Item {
    id: pill
    property Item target: null
    property real length: 16
    property real inset: 0 // from the row's left edge
    property bool animated: Theme.navPillAnimation

    // Windows' measured values (a 144 fps recording of its settings sidebar)
    readonly property int stretchTime: Theme.animated(270) // the old pill reaching toward the row, linear
    readonly property real stretchLength: 11
    readonly property int closeTime: Theme.animated(27)
    readonly property real openExtra: 10 // the new pill's extra length, taken away over settleTime
    readonly property int settleTime: Theme.animated(405)

    // Where the pill goes for a row. `goal` reads the row's y in the binding,
    // so a moving row is followed.
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
    readonly property real px: target ? target.mapToItem(parent, inset, 0).x : 0
    property Item lastTarget: null

    property real oldRest: 0 // the old pill's resting y
    property int dir: 1 // 1: the new row is below
    property real stretch: 0
    property real close: 0
    property real extra: 0 // 1 -> 0: the new pill's extra length left
    property bool oldShown: false
    property bool newHidden: false

    readonly property real oldBottomRest: oldRest + length
    readonly property real reachBottom: oldBottomRest + stretchLength * stretch
    readonly property real reachTop: oldRest - stretchLength * stretch
    readonly property real oldTop: dir > 0 ? oldRest + (reachBottom - oldRest) * close : reachTop
    readonly property real oldBottom: dir > 0 ? reachBottom : oldBottomRest - (oldBottomRest - reachTop) * close

    function follow() {
        if (target === lastTarget) {
            return
        }
        const old = lastTarget
        const busy = slide.running
        lastTarget = target
        slide.stop()
        oldShown = false
        newHidden = false
        extra = 0
        // First row, no row, animation off, or a selection in the middle of the animation: jump
        if (!target || !old || !animated || busy) {
            return
        }
        const from = goalOf(old)
        if (from === goalOf(target)) {
            return
        }
        oldRest = from
        dir = goalOf(target) > from ? 1 : -1
        stretch = 0
        close = 0
        extra = 1
        oldShown = true
        newHidden = true
        slide.restart()
    }
    // A row calls this from onSelectedChanged (and once when created). The
    // old row letting go waits a turn, so a new row taking over in the same
    // change is one move, not a vanish and an appearance.
    function track(row, selected) {
        if (selected) {
            target = row
        } else if (target === row) {
            Qt.callLater(() => { if (target === row) target = null })
        }
    }
    onTargetChanged: follow()
    z: 1

    Rectangle { // the old pill, reaching and closing
        visible: pill.oldShown && pill.oldBottom > pill.oldTop
        x: pill.px
        y: pill.oldTop
        width: 3
        height: pill.oldBottom - pill.oldTop
        radius: 1.5
        color: Theme.accentFill
    }
    Rectangle { // the pill on the selected row
        visible: pill.target !== null && !pill.newHidden
        x: pill.px
        y: pill.goal - (pill.dir > 0 ? pill.openExtra * pill.extra : 0)
        width: 3
        height: pill.length + pill.openExtra * pill.extra
        radius: 1.5
        color: Theme.accentFill
    }

    SequentialAnimation {
        id: slide
        NumberAnimation { target: pill; property: "stretch"; from: 0; to: 1; duration: pill.stretchTime }
        ScriptAction { script: pill.newHidden = false }
        ParallelAnimation {
            NumberAnimation { target: pill; property: "close"; from: 0; to: 1; duration: pill.closeTime }
            NumberAnimation {
                target: pill; property: "extra"; from: 1; to: 0
                duration: pill.settleTime
                easing.type: Easing.BezierSpline
                easing.bezierCurve: Theme.decelerate
            }
        }
        ScriptAction { script: pill.oldShown = false }
    }
}
