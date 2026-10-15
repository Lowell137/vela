// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// A Game Bar widget: its own floating card, dragged by the title, with the
// settings, pin and close buttons. In the bar (editable) it moves and
// remembers where; in the overlay it is the same card, only drawn.
Rectangle {
    id: card
    required property string wid
    property bool editable: false
    property string screenName // the output this window is on
    property point auto: Qt.point(24, 80) // where the layout puts it until it is moved
    signal geometryChanged2()

    readonly property var stored: GameNumbers.posOf(wid)
    property bool dragging: false
    property real dragX
    property real dragY
    readonly property real wantX: dragging ? dragX : (stored.stored ? stored.x : auto.x)
    readonly property real wantY: dragging ? dragY : (stored.stored ? stored.y : auto.y)
    // Never off the screen: a card saved on a bigger output is pulled in.
    x: Math.max(0, Math.min((parent ? parent.width : 0) - width, wantX))
    y: Math.max(0, Math.min((parent ? parent.height : 0) - height, wantY))
    width: 360
    height: 44 + bodyLoader.height + 16
    radius: Theme.radiusOverlay
    color: Theme.surface
    border.width: 1
    border.color: Theme.stroke
    onXChanged: geometryChanged2()
    onYChanged: geometryChanged2()
    onHeightChanged: geometryChanged2()
    onVisibleChanged: geometryChanged2()

    // Clicks inside the card don't close the bar.
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }

    // The title: also the handle.
    Item {
        id: header
        width: parent.width
        height: 44
        MouseArea {
            id: handle
            anchors.fill: parent
            enabled: card.editable
            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            property point grab
            onPressed: mouse => {
                const p = mapToItem(card.parent, mouse.x, mouse.y)
                grab = Qt.point(p.x - card.x, p.y - card.y)
                card.dragX = card.x
                card.dragY = card.y
                card.dragging = true
            }
            onPositionChanged: mouse => {
                if (!pressed) return
                const p = mapToItem(card.parent, mouse.x, mouse.y)
                card.dragX = p.x - grab.x
                card.dragY = p.y - grab.y
            }
            onReleased: {
                GameNumbers.setPos(card.wid, card.screenName, card.x, card.y)
                card.dragging = false
            }
        }
        Row {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            Image {
                anchors.verticalCenter: parent.verticalCenter
                width: 16
                height: 16
                source: Theme.icons + GameNumbers.icon(card.wid)
                sourceSize: Qt.size(width, height)
            }
            Text {
                text: GameNumbers.title(card.wid)
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.weight: Font.DemiBold
            }
        }
        Row {
            visible: card.editable
            anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
            spacing: 2
            GameIconButton {
                visible: card.wid === "performance" || card.wid === "crosshair"
                icon: "preferences-system"
                tip: qsTr("Settings")
                onClicked: {
                    GameNumbers.closeRequested(() => {})
                    System.openSettingsPage("gaming")
                }
            }
            GameIconButton {
                checked: GameNumbers.isPinned(card.wid)
                icon: checked ? "window-unpin" : "window-pin"
                tip: checked ? qsTr("Unpin from the screen") : qsTr("Pin over the game")
                onClicked: GameNumbers.togglePin(card.wid, card.screenName, card.x, card.y)
            }
            GameIconButton {
                icon: "window-close"
                tip: qsTr("Close")
                onClicked: GameNumbers.setOpen(card.wid, false)
            }
        }
    }

    Loader {
        id: bodyLoader
        x: 16
        y: 44
        width: card.width - 32
        height: item ? item.implicitHeight : 0
        source: ({ capture: "GameCaptureBody.qml", audio: "GameAudioBody.qml", performance: "GamePerformanceBody.qml", media: "GameMediaBody.qml",
                   crosshair: "GameCrosshairBody.qml", gamemode: "GameModeBody.qml" })[card.wid]
        onLoaded: item.width = Qt.binding(() => bodyLoader.width)
    }
}
