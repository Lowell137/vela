// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls
import Vela.Terminal.Backend

// One terminal of a tab: the program's screen, the bar to go back through
// what it wrote, and a line around it when it's the one typed in among others.
Item {
    id: pane
    property var win // Main.qml
    property var page // its tab
    property var profile // { name, command, directory }
    readonly property bool isSplit: false
    readonly property alias term: term
    readonly property string title: term.title !== "" ? term.title : (profile ? profile.name : "")

    Terminal {
        id: term
        anchors.fill: parent
        fontFamily: pane.win.fontFamily
        fontSize: pane.win.fontSize
        scheme: pane.win.scheme
        backgroundOpacity: pane.win.backgroundOpacity
        onActiveFocusChanged: if (activeFocus) pane.page.active = pane
        onFinished: pane.win.closePane(pane)
    }

    Rectangle {
        anchors.fill: parent
        visible: pane.page && pane.page.content !== pane && pane.page.active === pane
        color: "transparent"
        border.width: 1
        border.color: Theme.accentFill
    }

    // The scroll bar: where the view is in the history.
    Item {
        id: bar
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom; margins: 2 }
        width: barHover.hovered || barMouse.pressed ? 12 : 6
        visible: term.history > 0
        Behavior on width { NumberAnimation { duration: Theme.fast } }
        readonly property real total: term.history + term.rows
        Rectangle {
            x: 2
            width: parent.width - 4
            height: Math.max(24, bar.height * term.rows / bar.total)
            y: (bar.height - height) * (term.history > 0 ? 1 - term.scroll / term.history : 1)
            radius: width / 2
            color: barHover.hovered || barMouse.pressed ? Theme.scrollThumbHover : Theme.scrollThumb
        }
        HoverHandler { id: barHover }
        MouseArea {
            id: barMouse
            anchors.fill: parent
            function move(y) { term.scroll = Math.round((1 - Math.max(0, Math.min(1, y / height))) * term.history) }
            onPressed: mouse => move(mouse.y)
            onPositionChanged: mouse => move(mouse.y)
        }
    }
}
