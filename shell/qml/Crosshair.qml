// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The crosshair over the game (Win+Alt+C, the Game Bar's Crosshair widget,
// Settings > Gaming): the mark in the center of its output, moved by
// gamebar/crosshair-x and -y (dragged in the Game Bar). It stays while it is
// on and pinned (the pin is on by default); while the Game Bar is open on
// that output the bar draws it instead, where it can be dragged. Click-through.
// The window is centered by the compositor: it grows by twice the offset and
// the mark is drawn off its center.
Window {
    id: root
    visible: false
    color: "transparent"

    readonly property bool want: GameNumbers.crosshairOn && GameNumbers.isPinned("crosshair")
        && GameNumbers.editScreen !== mark.screenName

    width: mark.width + 2 * Math.abs(mark.dx)
    height: mark.height + 2 * Math.abs(mark.dy)

    Connections {
        target: Shell
        function onCrosshairToggleRequested() { Config.setGameBar("crosshair", !GameNumbers.crosshairOn) }
    }
    function sync() {
        if (!want) {
            visible = false
            return
        }
        if (visible && (!root.screen || root.screen.name !== mark.screenName)) visible = false
        if (!visible) {
            Shell.placeOnScreen(root, mark.screenName)
            visible = true
        }
    }
    onWantChanged: sync()
    Connections {
        target: mark
        function onScreenNameChanged() { root.sync() }
    }
    Component.onCompleted: Qt.callLater(sync)

    CrosshairMark {
        id: mark
        x: (root.width - width) / 2 + dx
        y: (root.height - height) / 2 + dy
    }
}
