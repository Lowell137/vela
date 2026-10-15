// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The Game Bar's pinned widgets over the game: one of these windows per
// output that has one (ScreenWindows), the whole output, transparent, the
// mouse goes through (but over a pinned Media card). They are dragged in the Game Bar itself, whose cards
// take their place there while it is open on this output.
// Over a fullscreen game any overlay surface can stop direct scanout (the
// game is composited again): MangoHud, drawn inside the game, doesn't.
Window {
    id: root
    visible: false
    color: "transparent"
    readonly property string screenName: screen ? screen.name : ""

    // The mouse goes through, except over a pinned Media card, which has
    // buttons and a position bar.
    function updateMask() {
        const rects = []
        for (let i = 0; i < cards.count; i++) {
            const card = cards.itemAt(i)
            if (card && card.visible && card.wid === "media") rects.push(Qt.rect(card.x, card.y, card.width, card.height))
        }
        Shell.setInputRegion(root, rects)
    }
    Timer {
        id: maskTimer
        interval: 0
        onTriggered: root.updateMask()
    }
    onVisibleChanged: maskTimer.restart()

    Repeater {
        id: cards
        model: GameNumbers.pinnedWidgets
        delegate: GameWidget {
            required property string modelData
            wid: modelData
            screenName: root.screenName
            visible: GameNumbers.screenOf(modelData) === root.screenName && GameNumbers.editScreen !== root.screenName
            onGeometryChanged2: maskTimer.restart()
        }
        onCountChanged: maskTimer.restart()
    }
}
