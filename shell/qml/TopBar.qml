// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The dock style's top panel, like the macOS menu bar: the logo menu and the
// focused app's name at the left; the notification area icons, the system
// icons and the clock at the right. The dock itself is the Taskbar window.
Window {
    id: root
    objectName: "topbar"
    property bool primary: false
    readonly property string screenName: screen ? screen.name : ""

    // It stays invisible until main.cpp has turned it into a layer-shell panel.
    visible: false
    width: 1280
    height: Theme.topBarHeight
    color: "transparent"

    function updateBlur() {
        Effects.setBlur(root, [Qt.rect(0, 0, width, height)])
    }
    Component.onCompleted: updateBlur()
    onWidthChanged: updateBlur()
    onHeightChanged: updateBlur()

    // The panels this bar opens (quick settings, notifications) go to its output.
    function fromHere() { Menus.panelScreen = root.screenName }

    // The focused app: the task whose window is active.
    property string activeName: ""
    function refreshActive() {
        let name = ""
        for (let i = 0; i < taskNames.count; ++i) {
            const item = taskNames.itemAt(i)
            if (item && item.windowActive) {
                name = item.name
                break
            }
        }
        activeName = name
    }
    Repeater {
        id: taskNames
        model: Tasks
        delegate: Item {
            required property string name
            required property bool windowActive
            onWindowActiveChanged: root.refreshActive()
            Component.onCompleted: root.refreshActive()
        }
        onCountChanged: root.refreshActive()
    }

    // The tray icons' menus hang from the bar.
    Connections {
        target: Tray
        enabled: root.primary
        function onMenuReady(row, anchorX, entries) {
            root.fromHere()
            Menus.open(Menus.trayEntries(row, entries), anchorX, root.height + 4,
                { screen: root.screenName, centered: true, above: false })
        }
    }

    // The logo menu: system actions, like the Apple menu.
    function logoEntries() {
        const item = (text, icon, name) => ({ text: text, icon: icon, enabled: System.available(name), action: () => System.trigger(name) })
        return [
            item(qsTr("S&ettings"), "preferences-system", "settings"),
            item(qsTr("Task manage&r"), "utilities-system-monitor", "task-manager"),
            item(qsTr("&Terminal"), "utilities-terminal", "terminal"),
            { separator: true },
            { text: qsTr("S&leep"), icon: "system-suspend", enabled: Shell.canSuspend(), action: () => Shell.suspend() },
            { text: qsTr("&Restart"), icon: "system-reboot", action: () => Shell.reboot() },
            { text: qsTr("Sh&ut down"), icon: "system-shutdown", action: () => Shell.powerOff() },
            { separator: true },
            { text: qsTr("Sign &out"), icon: "system-log-out", action: () => Shell.logout() }
        ]
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.taskbar

        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 1
            color: Theme.stroke
        }

        // The bar's empty space: Task Manager and settings.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onClicked: mouse => {
                root.fromHere()
                Menus.open([
                    { text: qsTr("Task &manager"), icon: "task-manager", enabled: System.available("task-manager"), action: () => System.trigger("task-manager") },
                    { text: qsTr("Taskbar &settings"), icon: "taskbar-settings", enabled: System.available("taskbar-settings"), action: () => System.trigger("taskbar-settings") }
                ], mouse.x, root.height + 4, { screen: root.screenName, above: false })
            }
        }
    }

    // --- left: logo and app name ---
    Row {
        id: left
        x: 8
        height: parent.height
        spacing: 2

        Item {
            id: logoButton
            width: 32
            height: parent.height
            Rectangle {
                anchors { fill: parent; topMargin: 3; bottomMargin: 3 }
                radius: Theme.radius
                color: logoMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
                opacity: logoMouse.containsMouse ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.fast } }
            }
            StartGlyph {
                anchors.centerIn: parent
                scale: 0.8
            }
            MouseArea {
                id: logoMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    root.fromHere()
                    Menus.open(root.logoEntries(), logoButton.mapToItem(null, 0, 0).x, root.height + 4,
                        { screen: root.screenName, above: false })
                }
            }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            leftPadding: 8
            text: root.activeName !== "" ? root.activeName : qsTr("Vela")
            color: Theme.text
            font.pixelSize: Theme.fontCaption
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            width: Math.min(implicitWidth, 320)
        }
    }

    // --- right: tray, system icons, clock ---
    Row {
        id: right
        anchors.right: parent.right
        anchors.rightMargin: 8
        height: parent.height
        spacing: 2

        Row {
            id: trayRow
            visible: root.primary
            height: parent.height
            Repeater {
                model: Tray
                delegate: TrayIcon {
                    width: 28
                    height: trayRow.height
                    visible: icon !== ""
                }
            }
        }

        Item {
            id: searchButton
            visible: root.primary
            width: 32
            height: parent.height
            Rectangle {
                anchors { fill: parent; topMargin: 3; bottomMargin: 3 }
                radius: Theme.radius
                color: searchMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
                opacity: searchMouse.containsMouse ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.fast } }
            }
            Image {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: Theme.icons + "search"
                sourceSize: Qt.size(width, height)
            }
            MouseArea {
                id: searchMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    root.fromHere()
                    Shell.spotlightRequested()
                }
            }
        }

        Item {
            id: systemIcons
            visible: root.primary
            width: iconsRow.width + 16
            height: parent.height
            Rectangle {
                anchors { fill: parent; topMargin: 3; bottomMargin: 3 }
                radius: Theme.radius
                color: systemMouse.pressed ? Theme.subtlePressed : Theme.subtleHover
                opacity: systemMouse.containsMouse || Menus.quickSettingsOpen ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.fast } }
            }
            Row {
                id: iconsRow
                anchors.centerIn: parent
                spacing: 10
                Image {
                    width: 16
                    height: 16
                    source: Theme.icons + encodeURIComponent(Status.networkIconName)
                    sourceSize: Qt.size(width, height)
                }
                Image {
                    visible: Audio.available
                    width: 16
                    height: 16
                    source: Theme.icons + encodeURIComponent(Audio.iconName)
                    sourceSize: Qt.size(width, height)
                }
                Image {
                    visible: Status.batteryPresent
                    width: 16
                    height: 16
                    source: Theme.icons + encodeURIComponent(Status.batteryCharging ? "battery-good-charging" : "battery-good")
                    sourceSize: Qt.size(width, height)
                }
            }
            MouseArea {
                id: systemMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    root.fromHere()
                    Shell.quickSettingsRequested()
                }
                // A step per notch, like the taskbar's.
                property real wheelRest: 0
                onWheel: wheel => {
                    wheelRest += wheel.angleDelta.y
                    while (Math.abs(wheelRest) >= 120) {
                        root.fromHere()
                        Sound.press(wheelRest > 0 ? "up" : "down")
                        wheelRest -= wheelRest > 0 ? 120 : -120
                    }
                }
            }
        }

        Item {
            id: clock
            width: clockText.implicitWidth + 16 + (badge ? 22 : 0)
            height: parent.height
            readonly property bool badge: Notifications.history.count > 0 || Notifications.silenced
            property date now: new Date()
            Timer {
                interval: 1000
                running: true
                repeat: true
                onTriggered: clock.now = new Date()
            }
            Rectangle {
                anchors { fill: parent; topMargin: 3; bottomMargin: 3 }
                radius: Theme.radius
                color: Theme.subtleHover
                opacity: clockMouse.containsMouse || Menus.notificationCenterOpen ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.fast } }
            }
            Text {
                id: clockText
                x: 8
                anchors.verticalCenter: parent.verticalCenter
                text: Qt.formatDateTime(clock.now, "ddd d MMM  HH:mm")
                color: Theme.text
                font.pixelSize: Theme.fontCaption
            }
            Rectangle {
                visible: clock.badge
                x: parent.width - width - 4
                y: (parent.height - height) / 2
                width: 16
                height: 16
                radius: 8
                color: Notifications.silenced ? "transparent" : Theme.accent
                Text {
                    anchors.centerIn: parent
                    visible: !Notifications.silenced
                    text: Math.min(Notifications.history.count, 9)
                    color: "white"
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                }
                Image {
                    anchors.centerIn: parent
                    visible: Notifications.silenced
                    width: 12
                    height: 12
                    source: Theme.icons + "notifications-disabled"
                    sourceSize: Qt.size(width, height)
                }
            }
            MouseArea {
                id: clockMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    root.fromHere()
                    Shell.notificationCenterRequested()
                }
            }
        }
    }
}
