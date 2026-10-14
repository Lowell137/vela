// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The hidden tray icons, like Windows 11's "^": a small flyout above the
// taskbar button with the apps' icons in a grid. A click outside, Esc or the
// button again close it. Opened with Menus.trayOverflow.
Window {
    id: root
    visible: false
    color: "transparent"

    property real anchorX: 0

    Connections {
        target: Menus
        function onTrayOverflowChanged() {
            if (Menus.trayOverflow) {
                root.open()
            } else {
                root.visible = false
            }
        }
    }
    function open() {
        const request = Menus.trayOverflow
        if (!request) return
        const screen = Qt.application.screens.find(s => s.name === request.screen)
        if (screen && screen !== root.screen) {
            // Only while hidden: hiding and showing again at once breaks the
            // layer surface's configure handshake ("wrong configure serial").
            if (root.visible) {
                root.visible = false
                Qt.callLater(root.open)
                return
            }
            Shell.placeOnScreen(root, screen.name)
        }
        root.anchorX = request.center
        root.visible = true
        panel.opacity = 0
        panel.y = Qt.binding(() => panelY + (Config.taskbarTop ? -6 : 6))
        appear.restart()
        content.forceActiveFocus()
        root.updateBlur()
    }
    function close() {
        Menus.trayOverflow = null
    }
    function updateBlur() {
        if (visible) {
            Effects.setBlur(root, [Qt.rect(panel.x, root.panelY, panel.width, panel.height)])
        }
    }
    onWidthChanged: updateBlur()
    onHeightChanged: updateBlur()

    // An icon's menu goes away with the keyboard: the flyout stays under it.
    onActiveChanged: if (!active && visible && !Menus.isOpen) close()

    // The window covers the whole output, the taskbar too.
    readonly property real panelY: Config.taskbarTop ? Theme.taskbarHeight + 12 : height - Theme.taskbarHeight - 12 - panel.height
    onPanelYChanged: updateBlur()

    ParallelAnimation {
        id: appear
        NumberAnimation { target: panel; property: "opacity"; to: 1; duration: Theme.fast }
        NumberAnimation { target: panel; property: "y"; to: root.panelY; duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
    }

    // The icons' menus open above the flyout.
    Connections {
        target: Tray
        enabled: root.visible
        function onMenuReady(row, anchorX, entries) {
            Menus.open(Menus.trayEntries(row, entries), anchorX, Config.taskbarTop ? root.panelY + panel.height + 4 : root.panelY - 4,
                { screen: root.screen ? root.screen.name : "", above: !Config.taskbarTop, centered: true })
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onClicked: root.close()
    }

    Item {
        id: content
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: root.close()
    }

    Rectangle {
        id: panel
        x: Math.max(8, Math.min(root.width - width - 8, root.anchorX - width / 2))
        y: root.panelY
        width: grid.width + 16
        height: grid.height + 16
        radius: Theme.radiusOverlay
        color: Theme.popup
        border.width: 1
        border.color: Theme.stroke
        onXChanged: root.updateBlur()

        // Clicks between the icons don't close it.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
        }

        Grid {
            id: grid
            x: 8
            y: 8
            // At most five in a row, like Windows.
            columns: Math.max(1, Math.min(5, shown))
            spacing: 4
            readonly property int shown: {
                let n = 0
                for (const c of children) {
                    if (c.shown) n++
                }
                return n
            }

            Repeater {
                model: Tray
                delegate: TrayIcon {
                    readonly property bool shown: icon !== "" && !attention
                    width: 40
                    height: 40
                    visible: shown
                    onActivated: root.close()
                }
            }
        }
    }
}
