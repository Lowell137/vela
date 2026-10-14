// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// An app's notification area icon (Telegram, Discord, Steam...), on the
// taskbar or in the hidden icons flyout (TrayOverflow.qml). The app decides
// what the clicks do.
Item {
    id: trayItem
    required property int index
    required property string icon
    required property bool attention
    // Left click: the flyout closes.
    signal activated()

    width: 32

    Rectangle {
        id: hover
        anchors.centerIn: parent
        width: parent.width
        height: Math.min(parent.width, parent.height)
        radius: Theme.radius
        color: trayMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
        opacity: trayMouse.containsMouse ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.fast } }
    }
    Image {
        anchors.centerIn: parent
        width: 18
        height: 18
        source: trayItem.icon
        sourceSize: Qt.size(width, height)
        smooth: true
        mipmap: true
    }
    // Asks for attention (unread messages): a dot.
    Rectangle {
        visible: trayItem.attention
        width: 6
        height: 6
        radius: 3
        color: Theme.accent
        anchors { right: hover.right; top: hover.top; rightMargin: 6; topMargin: 4 }
    }

    MouseArea {
        id: trayMouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        onClicked: mouse => {
            const center = trayItem.mapToItem(null, trayItem.width / 2, 0).x
            Menus.trayAnchorY = trayItem.mapToItem(null, 0, 0).y
            if (mouse.button === Qt.RightButton) {
                Tray.requestMenu(trayItem.index, center)
            } else if (mouse.button === Qt.MiddleButton) {
                Tray.secondaryActivate(trayItem.index)
            } else {
                Tray.activate(trayItem.index, center)
                trayItem.activated()
            }
        }
        onWheel: wheel => Tray.scroll(trayItem.index, wheel.angleDelta.y)
    }
}
