// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The Calendar's settings: a page over the window with what the views show and
// the reminder new events start with.
Rectangle {
    id: page
    property var win // Main.qml
    signal closed()
    color: Theme.dialogFooter
    focus: true
    Keys.onEscapePressed: closed()

    MouseArea { anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.AllButtons; onWheel: {} }

    // One setting: its name, a line about it, the control at the right.
    component Card: Rectangle {
        property string title
        property string description
        default property alias control: slot.data
        width: parent.width
        height: 64
        radius: Theme.radiusCard
        color: Theme.card
        border.width: 1
        border.color: Theme.cardStroke
        Column {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text {
                text: title
                color: Theme.text
                font.pixelSize: Theme.fontBody
            }
            Text {
                visible: description !== ""
                text: description
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }
        }
        Item {
            id: slot
            anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
            width: childrenRect.width
            height: childrenRect.height
        }
    }
    component Heading: Text {
        topPadding: 12
        color: Theme.text
        font.pixelSize: Theme.fontBody
        font.weight: Font.DemiBold
    }

    readonly property var reminderValues: [-1, 0, 5, 15, 30, 60, 1440]
    readonly property var reminderNames: [qsTr("None"), qsTr("At the time of the event"), qsTr("5 minutes before"),
        qsTr("15 minutes before"), qsTr("30 minutes before"), qsTr("1 hour before"), qsTr("1 day before")]
    readonly property var viewNames: ({ day: qsTr("Day"), workweek: qsTr("Work week"), week: qsTr("Week"), month: qsTr("Month") })

    Row {
        id: header
        x: 8
        y: 8
        spacing: 8
        ToolButton {
            anchors.verticalCenter: parent.verticalCenter
            icon.name: "go-previous"
            tooltip: qsTr("Back (Esc)")
            onClicked: page.closed()
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Settings")
            color: Theme.text
            font.pixelSize: Theme.fontTitle
            font.weight: Font.DemiBold
        }
    }

    Column {
        x: Math.max(24, (parent.width - width) / 2)
        y: header.y + header.height + 16
        width: Math.min(720, page.width - 48)
        spacing: 4

        Heading { text: qsTr("Views") }
        Card {
            title: qsTr("Week numbers")
            description: qsTr("Show the number of the week in the month and the week views")
            Switch { checked: page.win ? page.win.weekNumbers : false; onToggled: on => page.win.weekNumbers = on }
        }
        Card {
            title: qsTr("Start with")
            Button {
                text: page.win ? page.viewNames[page.win.view] : ""
                onClicked: {
                    const p = mapToItem(null, 0, height)
                    page.win.menuLayer.open(Object.keys(page.viewNames).map(k => ({
                        text: page.viewNames[k], checked: page.win.view === k, action: () => page.win.view = k
                    })), p.x, p.y, {})
                }
            }
        }

        Heading { text: qsTr("New events") }
        Card {
            title: qsTr("Reminder")
            description: qsTr("What a new event is set to remind with")
            Button {
                text: page.win ? page.reminderNames[Math.max(0, page.reminderValues.indexOf(page.win.defaultReminder))] : ""
                onClicked: {
                    const p = mapToItem(null, 0, height)
                    page.win.menuLayer.open(page.reminderNames.map((name, i) => ({
                        text: name, checked: page.win.defaultReminder === page.reminderValues[i],
                        action: () => page.win.defaultReminder = page.reminderValues[i]
                    })), p.x, p.y, {})
                }
            }
        }

        Heading { text: qsTr("About") }
        Card {
            title: qsTr("Calendar")
            description: qsTr("Events stay on this computer. Part of the Vela desktop")
        }
    }
}
