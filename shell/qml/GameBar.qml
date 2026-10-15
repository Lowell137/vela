// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The Game Bar (Win+G), like Windows': a slim toolbar at the top center (the
// focused app, the widget menu, the widget buttons, the clock, the battery,
// settings) and, under it, the widgets as separate floating cards you drag by
// their title (GameWidget.qml): Capture, Audio, Performance, Media, Crosshair
// and Game mode. A light veil (Settings > Gaming) dims the game; a click on
// it, Esc or Win+G close the bar. Pinned widgets stay over the game
// (GameOverlay.qml) and the crosshair too (Crosshair.qml); the others go.
Window {
    id: root
    visible: false
    color: "transparent"

    readonly property string screenName: screen ? screen.name : ""
    readonly property bool dim: (Config.gameBar.dim ?? true) === true

    function open() {
        if (!Menus.placeOnTargetScreen(root)) {
            Qt.callLater(root.open)
            return
        }
        visible = true
        content.opacity = 0
        slide.y = -12
        appear.restart()
        content.forceActiveFocus()
        clockTimer.triggered()
        layoutTimer.restart()
    }
    function close() {
        visible = false
    }
    // The bar leaves the picture before a screenshot or a recording starts.
    function closeThen(action) {
        close()
        after.action = action
        after.restart()
    }
    Timer {
        id: after
        property var action
        interval: 300
        onTriggered: if (action) action()
    }

    Connections {
        target: Shell
        function onGameBarRequested() {
            if (root.visible) root.close()
            else root.open()
        }
    }
    Connections {
        target: GameNumbers
        function onCloseRequested(action) { root.closeThen(action) }
    }
    onActiveChanged: if (!active && visible && !Menus.isOpen) close()

    // The sampling runs while the bar or a pinned widget is on screen; the
    // overlay and crosshair windows of this output step aside while it's open.
    Binding { target: Game; property: "active"; value: root.visible || GameNumbers.overlayShown }
    Binding { target: GameNumbers; property: "editScreen"; value: root.visible ? root.screenName : "" }

    // The widgets' automatic layout: left to right under the toolbar, a new
    // row when the screen is full, for the ones not moved yet.
    Timer {
        id: layoutTimer
        interval: 0
        onTriggered: {
            if (!root.visible) return
            let x = 24
            let y = toolbar.y + toolbar.height + 16
            let rowHeight = 0
            for (let i = 0; i < widgets.count; i++) {
                const card = widgets.itemAt(i)
                if (!card || !card.visible || card.stored.stored) continue
                if (x > 24 && x + card.width + 24 > content.width) {
                    x = 24
                    y += rowHeight + 16
                    rowHeight = 0
                }
                card.auto = Qt.point(x, y)
                x += card.width + 16
                rowHeight = Math.max(rowHeight, card.height)
            }
        }
    }

    function updateBlur() {
        if (!visible) return
        const items = [toolbar]
        for (let i = 0; i < widgets.count; i++) {
            const card = widgets.itemAt(i)
            if (card && card.visible) items.push(card)
        }
        Effects.setBlur(root, items.map(i => { const p = i.mapToItem(null, 0, 0); return Qt.rect(p.x, p.y, i.width, i.height) }))
    }
    Timer {
        id: blurTimer
        interval: 0
        onTriggered: root.updateBlur()
    }

    ParallelAnimation {
        id: appear
        NumberAnimation { target: content; property: "opacity"; to: 1; duration: Theme.fast; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        NumberAnimation { target: slide; property: "y"; to: 0; duration: Theme.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        onFinished: blurTimer.restart()
    }

    // Outside the cards: a click closes.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onClicked: root.close()
    }

    // The widgets of the toolbar (the others are in the menu).
    readonly property var barWidgets: ["capture", "audio", "performance", "media"]

    function chooseWidgets(button) {
        const entries = GameNumbers.widgetIds.map(id => ({ text: GameNumbers.title(id), icon: GameNumbers.icon(id),
                                                           checked: GameNumbers.isOpen(id),
                                                           action: () => GameNumbers.toggleOpen(id) }))
        entries.push({ text: qsTr("Reset positions"), icon: "view-restore", action: () => GameNumbers.resetPositions() })
        const p = button.mapToItem(null, 0, button.height + 4)
        Menus.open(entries, p.x, p.y, { minWidth: 200, screen: root.screenName })
    }

    Item {
        id: content
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: root.close()
        transform: Translate { id: slide; onYChanged: blurTimer.restart() }

        // A light veil over the game (about a quarter black).
        Rectangle {
            anchors.fill: parent
            color: "black"
            opacity: root.dim ? 0.22 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
        }

        // --- the toolbar ---
        Rectangle {
            id: toolbar
            anchors.horizontalCenter: parent.horizontalCenter
            y: 16
            width: toolbarRow.width + 20
            height: 44
            radius: Theme.radiusOverlay
            color: Theme.surface
            border.width: 1
            border.color: Theme.stroke
            onWidthChanged: blurTimer.restart()
            MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }

            Timer {
                id: clockTimer
                interval: 10000
                running: root.visible
                repeat: true
                triggeredOnStart: true
                onTriggered: clockText.text = Qt.formatTime(new Date(), Qt.locale().timeFormat(Locale.ShortFormat))
            }

            Row {
                id: toolbarRow
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: Game.appIcon !== ""
                    width: 22
                    height: 22
                    source: visible ? Theme.icons + encodeURIComponent(Game.appIcon) : ""
                    sourceSize: Qt.size(width, height)
                    opacity: 0.95
                }
                Rectangle { width: 1; height: 20; color: Theme.divider; anchors.verticalCenter: parent.verticalCenter; visible: Game.appIcon !== "" }
                GameIconButton {
                    id: menuButton
                    anchors.verticalCenter: parent.verticalCenter
                    width: 32
                    height: 32
                    glyph: 18
                    icon: "view-list-icons"
                    tip: qsTr("Widget menu")
                    onClicked: root.chooseWidgets(menuButton)
                }
                Repeater {
                    model: root.barWidgets
                    delegate: GameIconButton {
                        required property string modelData
                        anchors.verticalCenter: parent.verticalCenter
                        width: 32
                        height: 32
                        glyph: 18
                        icon: GameNumbers.icon(modelData)
                        tip: GameNumbers.title(modelData)
                        checked: GameNumbers.isOpen(modelData)
                        onClicked: GameNumbers.toggleOpen(modelData)
                    }
                }
                Rectangle { width: 1; height: 20; color: Theme.divider; anchors.verticalCenter: parent.verticalCenter }
                Text {
                    id: clockText
                    anchors.verticalCenter: parent.verticalCenter
                    leftPadding: 6
                    rightPadding: 6
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.features: { "tnum": 1 }
                }
                Row {
                    visible: Status.batteryPresent
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4
                    Image {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        source: Theme.icons + (Status.batteryCharging ? "battery-full-charging" : "battery-full")
                        sourceSize: Qt.size(width, height)
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: Status.batteryPercent + " %"
                        color: Theme.text
                        font.pixelSize: Theme.fontCaption
                    }
                }
                Rectangle { width: 1; height: 20; color: Theme.divider; anchors.verticalCenter: parent.verticalCenter }
                GameIconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 32
                    height: 32
                    glyph: 18
                    icon: "preferences-system"
                    tip: qsTr("Settings")
                    onClicked: {
                        root.close()
                        System.openSettingsPage("gaming")
                    }
                }
            }
        }

        // --- the widgets ---
        Repeater {
            id: widgets
            model: GameNumbers.widgetIds
            delegate: GameWidget {
                required property string modelData
                wid: modelData
                editable: true
                screenName: root.screenName
                // A pinned card belongs to the output it was pinned on.
                visible: GameNumbers.isOpen(modelData)
                    && (modelData === "crosshair" || !GameNumbers.isPinned(modelData) || GameNumbers.screenOf(modelData) === root.screenName)
                onGeometryChanged2: {
                    layoutTimer.restart()
                    blurTimer.restart()
                }
            }
        }

        // --- the crosshair, where it can be dragged ---
        // While the bar is open on its output the bar draws it (Crosshair.qml
        // steps aside): drag to move, double-click to center.
        Item {
            id: aim
            visible: GameNumbers.crosshairOn && mark.screenName === root.screenName
            readonly property bool dragging: aimMouse.pressed
            property real dragX
            property real dragY
            readonly property real centerX: content.width / 2 + (dragging ? dragX : mark.dx)
            readonly property real centerY: content.height / 2 + (dragging ? dragY : mark.dy)
            width: Math.max(mark.width, 32)
            height: Math.max(mark.height, 32)
            x: Math.max(0, Math.min(content.width - width, centerX - width / 2))
            y: Math.max(0, Math.min(content.height - height, centerY - height / 2))

            CrosshairMark {
                id: mark
                anchors.centerIn: parent
            }
            // A frame, so it's clear it can be grabbed.
            Rectangle {
                anchors.fill: parent
                color: "transparent"
                radius: 4
                border.width: aimMouse.containsMouse || aim.dragging ? 1 : 0
                border.color: Theme.accent
            }
            MouseArea {
                id: aimMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                property point grab
                property bool moved: false // a click or a double-click doesn't write the position
                function offset(mouse) {
                    const p = mapToItem(content, mouse.x, mouse.y)
                    // As far as the mark can go and stay on the output.
                    const hx = content.width / 2 - mark.width / 2
                    const hy = content.height / 2 - mark.height / 2
                    return Qt.point(Math.max(-hx, Math.min(hx, p.x - grab.x - content.width / 2)),
                                    Math.max(-hy, Math.min(hy, p.y - grab.y - content.height / 2)))
                }
                onPressed: mouse => {
                    const p = mapToItem(content, mouse.x, mouse.y)
                    grab = Qt.point(p.x - aim.centerX, p.y - aim.centerY)
                    aim.dragX = mark.dx
                    aim.dragY = mark.dy
                    moved = false
                }
                onPositionChanged: mouse => {
                    if (!pressed) return
                    const o = offset(mouse)
                    moved = true
                    aim.dragX = o.x
                    aim.dragY = o.y
                }
                onReleased: {
                    if (!moved) return
                    const x = Math.round(aim.dragX)
                    const y = Math.round(aim.dragY)
                    Config.setGameBar("crosshair-screen", root.screenName)
                    Config.setGameBar("crosshair-x", x)
                    Config.setGameBar("crosshair-y", y)
                }
                onDoubleClicked: {
                    Config.setGameBar("crosshair-x", 0)
                    Config.setGameBar("crosshair-y", 0)
                }
            }
        }
    }
}
