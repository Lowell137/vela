// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The notification center and calendar (Win+N, or a click on the clock), like
// Windows 11: past notifications on top, grouped by app, with "Do not disturb"
// and "Clear all"; the month's calendar below. Two acrylic panels at the
// bottom right.
Window {
    id: root
    visible: false
    width: Theme.notificationsWidth
    height: column.height + 24
    color: "transparent"

    readonly property var history: Notifications.history
    property date month: new Date() // the month the calendar shows
    property var collapsed: ({}) // app name -> its group is folded
    property bool monthFolded: false // notifications-clock: only the clock
    property date now: new Date()
    Timer {
        interval: 1000
        repeat: true
        running: root.visible && Theme.notificationsClock
        triggeredOnStart: true
        onTriggered: root.now = new Date()
    }
    // notifications-focus: do not disturb for a while, like Windows' focus
    // session; it ends by itself (or when do not disturb is turned off).
    property int focusMinutes: 30
    property double focusEnd: 0
    readonly property bool focusing: focusEnd > 0 && Notifications.doNotDisturb
    Timer {
        running: root.focusEnd > 0
        interval: Math.max(1, root.focusEnd - Date.now())
        onTriggered: {
            root.focusEnd = 0
            Notifications.doNotDisturb = false
        }
    }
    Connections {
        target: Notifications
        function onDoNotDisturbChanged() { if (!Notifications.doNotDisturb) root.focusEnd = 0 }
    }
    readonly property var counts: { history.count; return history.appCounts() }

    function open() {
        Menus.quickSettingsOpen = false
        Notifications.collectPopups()
        month = new Date()
        if (!Menus.placeOnTargetScreen(root)) {
            Qt.callLater(root.open)
            return
        }
        visible = true
        Menus.notificationCenterOpen = true
        column.opacity = 0
        slide.x = Config.taskbarLeft ? -24 : 24
        appear.restart()
        column.forceActiveFocus()
        updateBlur()
    }
    function close() {
        visible = false
        Menus.notificationCenterOpen = false
    }

    Connections {
        target: Shell
        function onNotificationCenterRequested() { root.visible ? root.close() : root.open() }
        function onQuickSettingsRequested() { root.close() }
    }

    onActiveChanged: {
        if (!active && visible && !Menus.isOpen) {
            close()
        }
    }

    function updateBlur() {
        Effects.setBlur(root, [Qt.rect(column.x, column.y + notifications.y, column.width, notifications.height),
            Qt.rect(column.x, column.y + calendar.y, column.width, calendar.height)], Theme.notificationsRadius)
    }

    ParallelAnimation {
        id: appear
        NumberAnimation { target: column; property: "opacity"; to: 1; duration: Theme.fast; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        NumberAnimation { target: slide; property: "x"; to: 0; duration: Theme.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
    }

    component SmallButton: Item {
        id: smallButton
        property string label
        property string icon
        property bool checked: false
        signal clicked()
        width: icon !== "" ? 32 : buttonText.implicitWidth + 20
        height: 28
        Rectangle {
            anchors.fill: parent
            radius: Theme.radius
            color: smallButton.checked ? Theme.accent : Theme.subtleHover
            opacity: smallButton.checked || buttonMouse.containsMouse ? 1 : 0
        }
        Text {
            id: buttonText
            visible: smallButton.label !== ""
            anchors.centerIn: parent
            text: smallButton.label
            color: Theme.text
            font.pixelSize: Theme.fontCaption
        }
        Image {
            visible: smallButton.icon !== ""
            anchors.centerIn: parent
            width: 16
            height: 16
            source: smallButton.icon !== "" ? Theme.icons + encodeURIComponent(smallButton.icon) : ""
            sourceSize: Qt.size(width, height)
        }
        MouseArea {
            id: buttonMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: smallButton.clicked()
        }
    }

    Column {
        id: column
        x: 12
        y: 12
        width: root.width - 24
        spacing: 12
        transform: Translate { id: slide }
        focus: true
        Keys.onEscapePressed: root.close()
        onHeightChanged: root.updateBlur()

        // --- notifications ---
        Rectangle {
            id: notifications
            width: parent.width
            height: Math.min(Screen.height - Theme.taskbarHeight - calendar.height - 60, header.height + Math.max(list.contentHeight, 64) + 24)
            radius: Theme.notificationsRadius
            color: Theme.notificationsSurface
            border.width: 1
            border.color: Theme.stroke
            onHeightChanged: root.updateBlur()

            Item {
                id: header
                x: 16
                y: 10
                width: parent.width - 32
                height: 36
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        text: qsTr("Notifications")
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.DemiBold
                    }
                    Text {
                        visible: Notifications.captureSilenced
                        text: qsTr("Silenced while your screen is shared")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                    }
                    Text {
                        visible: Notifications.fullscreenSilenced && !Notifications.captureSilenced
                        text: qsTr("Silenced while a window is full screen")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                    }
                }
                Row {
                    anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                    spacing: 4
                    SmallButton {
                        icon: Notifications.silenced ? "notifications-disabled" : "notifications"
                        checked: Notifications.doNotDisturb
                        onClicked: Notifications.doNotDisturb = !Notifications.doNotDisturb
                    }
                    SmallButton {
                        visible: root.history.count > 0
                        label: qsTr("Clear all")
                        onClicked: Notifications.clearHistory()
                    }
                }
            }

            ListView {
                id: list
                anchors { top: header.bottom; left: parent.left; right: parent.right; bottom: parent.bottom; margins: 8; topMargin: 4 }
                clip: true
                spacing: 6
                model: root.history
                boundsBehavior: Flickable.StopAtBounds
                section.property: "appName"
                section.delegate: Item {
                    id: group
                    required property string section
                    readonly property bool folded: root.collapsed[section] === true
                    width: list.width
                    height: 32
                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radius
                        color: Theme.subtleHover
                        opacity: groupMouse.containsMouse ? 1 : 0
                    }
                    MouseArea {
                        id: groupMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            const c = Object.assign({}, root.collapsed)
                            c[group.section] = !group.folded
                            root.collapsed = c
                        }
                    }
                    Text {
                        id: groupName
                        anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                        text: (group.folded ? "\u25B8 " : "\u25BE ") + group.section
                            + "  \u00B7  " + (root.counts[group.section] || 0)
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                    }
                    SmallButton {
                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                        label: qsTr("Clear all")
                        onClicked: Notifications.clearApp(group.section)
                    }
                }

                delegate: Rectangle {
                    id: item
                    required property int notificationId
                    required property string icon
                    required property string summary
                    required property string body
                    required property var actions
                    required property bool hasDefaultAction
                    required property date time
                    required property string appName
                    visible: root.collapsed[appName] !== true
                    width: list.width
                    height: visible ? itemContent.height + 20 : 0
                    radius: Theme.notificationsCardRadius
                    color: itemHover.hovered ? Theme.subtleHover : Theme.control

                    HoverHandler { id: itemHover }
                    MouseArea {
                        id: itemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (item.hasDefaultAction) {
                                Notifications.invokeFromHistory(item.notificationId, "default")
                                root.close()
                            }
                        }
                    }
                    Row {
                        id: itemContent
                        x: 12
                        y: 10
                        width: parent.width - 24
                        spacing: 12
                        Image {
                            width: 28
                            height: 28
                            source: item.icon
                            sourceSize: Qt.size(width, height)
                            fillMode: Image.PreserveAspectFit
                        }
                        Column {
                            width: parent.width - 40
                            spacing: 2
                            Item {
                                width: parent.width
                                height: summaryText.height
                                Text {
                                    id: summaryText
                                    width: parent.width - timeText.width - 30
                                    text: item.summary
                                    color: Theme.text
                                    font.pixelSize: Theme.fontBody
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    id: timeText
                                    anchors.right: parent.right
                                    anchors.rightMargin: 24
                                    text: Qt.formatTime(item.time, "HH:mm")
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontCaption
                                }
                            }
                            Text {
                                visible: item.body !== ""
                                width: parent.width
                                text: item.body
                                textFormat: Text.StyledText
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                                wrapMode: Text.Wrap
                                maximumLineCount: 4
                                elide: Text.ElideRight
                            }
                            Row {
                                visible: item.actions.length > 0
                                spacing: 6
                                topPadding: 6
                                Repeater {
                                    model: item.actions
                                    delegate: SmallButton {
                                        required property var modelData
                                        label: modelData.label
                                        onClicked: Notifications.invokeFromHistory(item.notificationId, modelData.key)
                                    }
                                }
                            }
                        }
                    }
                    // Close (on hover), like on Windows.
                    SmallButton {
                        anchors { right: parent.right; top: parent.top; margins: 6 }
                        visible: itemHover.hovered
                        icon: "window-close"
                        onClicked: Notifications.dismissFromHistory(item.notificationId)
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: list.count === 0
                    text: qsTr("No new notifications")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontBody
                }
            }
        }

        // --- calendar ---
        Rectangle {
            id: calendar
            width: parent.width
            height: calendarContent.height + 32
            radius: Theme.notificationsRadius
            color: Theme.notificationsSurface
            border.width: 1
            border.color: Theme.stroke

            // notifications-week-strip: the weekdays on a rounded band.
            Rectangle {
                visible: Theme.notificationsWeekStrip && days.visible
                x: calendarContent.x
                y: calendarContent.y + days.y
                width: calendarContent.width
                height: 28
                radius: 8
                color: Theme.control
            }

            Column {
                id: calendarContent
                x: 16
                y: 16
                width: parent.width - 32
                spacing: 12

                // A theme's notifications-clock: the time with seconds, the
                // date below, and a button folding the month away.
                Item {
                    visible: Theme.notificationsClock
                    width: parent.width
                    height: visible ? clockText.height + dateText.height + 2 : 0
                    Text {
                        id: clockText
                        text: Qt.locale().toString(root.now, "HH:mm:ss")
                        color: Theme.text
                        font.pixelSize: 28
                        font.weight: Font.Bold
                    }
                    Text {
                        id: dateText
                        anchors { top: clockText.bottom; topMargin: 2 }
                        text: {
                            const s = Qt.locale().toString(root.now, "dddd, d MMMM")
                            return s.charAt(0).toUpperCase() + s.slice(1)
                        }
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                    }
                    SmallButton {
                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                        icon: root.monthFolded ? "go-up" : "go-down"
                        onClicked: root.monthFolded = !root.monthFolded
                    }
                }
                Rectangle {
                    visible: Theme.notificationsClock && !root.monthFolded
                    width: parent.width
                    height: 1
                    color: Theme.stroke
                }

                // Today, in full ("Saturday, October 4"), like Windows.
                Text {
                    visible: !Theme.notificationsClock
                    text: {
                        const s = Qt.locale().toString(new Date(), "dddd d MMMM")
                        return s.charAt(0).toUpperCase() + s.slice(1)
                    }
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.weight: Font.DemiBold
                }

                Item {
                    visible: !root.monthFolded
                    width: parent.width
                    height: 28
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        font.weight: Theme.notificationsClock ? Font.DemiBold : Font.Normal
                        text: {
                            const s = Qt.locale().toString(root.month, "MMMM yyyy")
                            return s.charAt(0).toUpperCase() + s.slice(1)
                        }
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                    }
                    Row {
                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                        spacing: 4
                        SmallButton {
                            icon: "go-up"
                            onClicked: root.month = new Date(root.month.getFullYear(), root.month.getMonth() - 1, 1)
                        }
                        SmallButton {
                            icon: "go-down"
                            onClicked: root.month = new Date(root.month.getFullYear(), root.month.getMonth() + 1, 1)
                        }
                    }
                }

                // The grid: the week starts on Monday.
                Grid {
                    id: days
                    visible: !root.monthFolded
                    columns: 7
                    readonly property real cell: (parent.width) / 7
                    readonly property int offset: (new Date(root.month.getFullYear(), root.month.getMonth(), 1).getDay() + 6) % 7
                    readonly property int length: new Date(root.month.getFullYear(), root.month.getMonth() + 1, 0).getDate()

                    Repeater {
                        model: ["lu", "ma", "me", "gi", "ve", "sa", "do"]
                        delegate: Text {
                            required property string modelData
                            required property int index
                            width: days.cell
                            height: 28
                            // On the strip: the locale's names, upper case, bold.
                            text: Theme.notificationsWeekStrip
                                ? Qt.locale().dayName((index + 1) % 7, Locale.ShortFormat).slice(0, 2).toUpperCase() : modelData
                            color: Theme.notificationsWeekStrip ? Theme.text : Theme.textSecondary
                            font.weight: Theme.notificationsWeekStrip ? Font.DemiBold : Font.Normal
                            font.pixelSize: Theme.fontCaption
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                    Repeater {
                        model: 42
                        delegate: Item {
                            required property int index
                            readonly property date day: new Date(root.month.getFullYear(), root.month.getMonth(), index - days.offset + 1)
                            readonly property bool inMonth: day.getMonth() === root.month.getMonth()
                            readonly property bool today: {
                                const now = new Date()
                                return day.getFullYear() === now.getFullYear() && day.getMonth() === now.getMonth()
                                    && day.getDate() === now.getDate()
                            }
                            width: days.cell
                            height: 36
                            Rectangle {
                                anchors.centerIn: parent
                                width: 32
                                height: 32
                                radius: Theme.notificationsTodayRadius
                                color: parent.today ? Theme.accent : Theme.subtleHover
                                opacity: parent.today || dayMouse.containsMouse ? 1 : 0
                            }
                            Text {
                                anchors.centerIn: parent
                                text: parent.day.getDate()
                                color: parent.today ? "white" : parent.inMonth ? Theme.text : Theme.textSecondary
                                opacity: parent.inMonth || parent.today ? 1 : 0.5
                                font.pixelSize: Theme.fontCaption
                            }
                            MouseArea {
                                id: dayMouse
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                        }
                    }
                }

                // notifications-focus: minutes, then start (or stop).
                Item {
                    visible: Theme.notificationsFocus
                    width: parent.width
                    height: visible ? 40 : 0
                    Rectangle { width: parent.width; height: 1; color: Theme.stroke }
                    Row {
                        anchors { left: parent.left; verticalCenter: parent.verticalCenter; verticalCenterOffset: 4 }
                        spacing: 4
                        SmallButton {
                            label: "\u2212"
                            onClicked: root.focusMinutes = Math.max(5, root.focusMinutes - (root.focusMinutes > 30 ? 15 : 5))
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 64
                            horizontalAlignment: Text.AlignHCenter
                            text: qsTr("%1 mins").arg(root.focusMinutes)
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                        }
                        SmallButton {
                            label: "+"
                            onClicked: root.focusMinutes = Math.min(240, root.focusMinutes + (root.focusMinutes >= 30 ? 15 : 5))
                        }
                    }
                    SmallButton {
                        anchors { right: parent.right; verticalCenter: parent.verticalCenter; verticalCenterOffset: 4 }
                        label: root.focusing ? qsTr("\u25A0 Stop focus") : qsTr("\u25B6 Focus")
                        onClicked: {
                            if (root.focusing) {
                                Notifications.doNotDisturb = false
                            } else {
                                root.focusEnd = Date.now() + root.focusMinutes * 60000
                                Notifications.doNotDisturb = true
                            }
                        }
                    }
                }
            }
        }
    }
}
