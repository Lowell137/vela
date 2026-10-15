// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The Game Bar (Win+G), like Windows': over the game, without dimming it, a
// top bar with the focused app and cards below: performance, audio, capture
// and game mode. Esc, the same shortcut or a click outside the cards close
// it. Every number can be pinned to the always-on overlay (GameOverlay.qml).
Window {
    id: root
    visible: false
    color: "transparent"

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
        updateBlur()
    }
    function close() {
        visible = false
    }
    // The panel leaves the picture before a screenshot or a recording starts.
    function closeThen(action) {
        close()
        after.action = action
        after.restart()
    }
    Timer {
        id: after
        property var action
        interval: 300
        onTriggered: action()
    }

    Connections {
        target: Shell
        function onGameBarRequested() {
            if (root.visible) root.close()
            else root.open()
        }
    }
    onActiveChanged: if (!active && visible && !Menus.isOpen) close()

    // The sampling runs while the panel or the overlay is on screen.
    Binding { target: Game; property: "active"; value: root.visible || GameNumbers.overlayShown }

    function updateBlur() {
        if (!visible) return
        const rects = [topBar, ...Array.prototype.filter.call(cards.children, c => c.visible && c.width > 0)]
            .map(i => { const p = i.mapToItem(null, 0, 0); return Qt.rect(p.x, p.y, i.width, i.height) })
        Effects.setBlur(root, rects)
    }

    ParallelAnimation {
        id: appear
        NumberAnimation { target: content; property: "opacity"; to: 1; duration: Theme.fast; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        NumberAnimation { target: slide; property: "y"; to: 0; duration: Theme.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        onFinished: root.updateBlur()
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onClicked: root.close()
    }

    // A card: a title and what goes in it.
    component GameCard: Rectangle {
        id: card
        property string title
        property string icon
        default property alias content: column.data
        width: 340
        height: column.height + 32
        radius: Theme.radiusOverlay
        color: Theme.surface
        border.width: 1
        border.color: Theme.stroke
        onHeightChanged: root.updateBlur()
        // Clicks inside the card don't close the bar.
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }
        Column {
            id: column
            x: 16
            y: 16
            width: parent.width - 32
            spacing: 10
            Row {
                spacing: 8
                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16
                    height: 16
                    source: Theme.icons + card.icon
                    sourceSize: Qt.size(width, height)
                }
                Text {
                    text: card.title
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.weight: Font.DemiBold
                }
            }
        }
    }

    // A small icon button (pins, close).
    component IconButton: Rectangle {
        id: button
        property string icon
        property string tip
        property bool checked: false
        signal clicked()
        width: 28
        height: 28
        radius: Theme.radius
        color: mouse.containsMouse ? Theme.subtleHover : checked ? Theme.control : "transparent"
        Image {
            anchors.centerIn: parent
            width: 16
            height: 16
            source: Theme.icons + button.icon
            sourceSize: Qt.size(width, height)
            opacity: button.checked || mouse.containsMouse ? 1 : 0.6
        }
        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: button.clicked()
        }
        Accessible.role: Accessible.Button
        Accessible.name: tip
    }

    component PinButton: IconButton {
        property string key
        checked: GameNumbers.isPinned(key)
        icon: checked ? "window-unpin" : "window-pin"
        tip: checked ? qsTr("Unpin from the overlay") : qsTr("Pin to the overlay")
        onClicked: GameNumbers.togglePin(key)
    }

    // A number with its label and pin.
    component Stat: Rectangle {
        id: stat
        property string key
        width: (parent.width - 8) / 2
        height: 52
        radius: Theme.radius
        color: Theme.card
        Column {
            x: 10
            anchors.verticalCenter: parent.verticalCenter
            Text {
                text: GameNumbers.label(stat.key)
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }
            Text {
                text: GameNumbers.value(stat.key)
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.features: { "tnum": 1 }
            }
        }
        PinButton {
            key: stat.key
            anchors { right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
        }
    }

    Item {
        id: content
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: root.close()
        transform: Translate { id: slide; onYChanged: root.updateBlur() }

        // --- the top bar: the title, the focused app, close ---
        Rectangle {
            id: topBar
            anchors.horizontalCenter: parent.horizontalCenter
            y: 16
            width: topRow.width + 24
            height: 48
            radius: Theme.radiusOverlay
            color: Theme.surface
            border.width: 1
            border.color: Theme.stroke
            MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }
            Row {
                id: topRow
                x: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 12
                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 20
                    height: 20
                    source: Theme.icons + "input-gaming"
                    sourceSize: Qt.size(width, height)
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Game Bar")
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.weight: Font.DemiBold
                }
                Rectangle { width: 1; height: 20; color: Theme.divider; anchors.verticalCenter: parent.verticalCenter }
                Image {
                    visible: Game.appIcon !== ""
                    anchors.verticalCenter: parent.verticalCenter
                    width: 20
                    height: 20
                    source: visible ? Theme.icons + encodeURIComponent(Game.appIcon) : ""
                    sourceSize: Qt.size(width, height)
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: Game.appName !== "" ? Game.appName : qsTr("No game in focus")
                    color: Game.appName !== "" ? Theme.text : Theme.textSecondary
                    font.pixelSize: Theme.fontBody
                    width: Math.min(implicitWidth, 320)
                    elide: Text.ElideRight
                }
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "window-close"
                    tip: qsTr("Close")
                    onClicked: root.close()
                }
            }
        }

        // --- the cards ---
        Flow {
            id: cards
            anchors.horizontalCenter: parent.horizontalCenter
            y: topBar.y + topBar.height + 16
            width: Math.min(parent.width - 32, 340 * 4 + 16 * 3 + 80)
            spacing: 16
            onPositioningComplete: root.updateBlur()

            // Performance: the compositor's frames and the machine's load.
            GameCard {
                title: qsTr("Performance")
                icon: "utilities-system-monitor"
                width: 420

                Row {
                    width: parent.width
                    spacing: 16
                    Column {
                        width: (parent.width - 16) / 2
                        Row {
                            spacing: 6
                            Text {
                                id: fpsText
                                text: GameNumbers.value("fps")
                                color: Theme.text
                                font.pixelSize: Theme.fontTitle * 1.4
                                font.weight: Font.DemiBold
                                font.features: { "tnum": 1 }
                            }
                            Text {
                                anchors.baseline: fpsText.baseline
                                text: qsTr("FPS")
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontBody
                            }
                            PinButton { key: "fps"; anchors.verticalCenter: parent.verticalCenter }
                        }
                        Text {
                            text: Game.perfStale ? qsTr("Nothing is being drawn") : qsTr("%n missed frame(s)", "", Game.missed)
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                        }
                    }
                    Column {
                        width: (parent.width - 16) / 2
                        Row {
                            spacing: 6
                            Text {
                                text: GameNumbers.value("frametime")
                                color: Theme.text
                                font.pixelSize: Theme.fontSubtitle
                                font.features: { "tnum": 1 }
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            PinButton { key: "frametime"; anchors.verticalCenter: parent.verticalCenter }
                        }
                        Text {
                            text: qsTr("Frame time")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                        }
                    }
                }
                Text {
                    text: qsTr("FPS, last minute")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                }
                Sparkline { width: parent.width; height: 36; values: Game.fpsHistory }
                Text {
                    text: qsTr("Frame time, last minute")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                }
                Sparkline { width: parent.width; height: 36; values: Game.frameHistory; color: Theme.caution }
                Flow {
                    width: parent.width
                    spacing: 8
                    Repeater {
                        model: ["cpu", "cputemp", "gpu", "gputemp", "gpupower", "vram", "ram"]
                        delegate: Stat { required property string modelData; key: modelData }
                    }
                }
                Item {
                    width: parent.width
                    height: 32
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: GameNumbers.pinned.length > 0 ? qsTr("Show the pinned numbers") : qsTr("Pin numbers to show them over the game")
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                    }
                    Toggle {
                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                        showLabel: false
                        checked: GameNumbers.overlayOn
                        onToggled: on => Config.setGameBar("overlay", on)
                    }
                }
                // MangoHud draws inside the game itself (Settings > Gaming).
                Text {
                    text: Game.mangoHudInstalled ? qsTr("MangoHud settings") : qsTr("MangoHud isn't installed")
                    color: Game.mangoHudInstalled ? (linkMouse.containsMouse ? Theme.accentFillHover : Theme.link) : Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    MouseArea {
                        id: linkMouse
                        anchors.fill: parent
                        enabled: Game.mangoHudInstalled
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.close()
                            System.openSettingsPage("gaming")
                        }
                    }
                }
            }
        }
    }
}
