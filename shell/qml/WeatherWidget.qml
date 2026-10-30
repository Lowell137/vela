// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The taskbar's weather: icon and temperature ("18°"), side by side, or one
// above the other along a vertical taskbar. A click opens WeatherFlyout.qml.
// The taskbar places it (anchors) and sets `vertical` and `screenName`; it is
// hidden, and takes no room, while the widget is off or has no place.
Item {
    id: root
    property bool vertical: false
    property string screenName: ""
    readonly property bool open: !!Menus.weather && Menus.weather.screen === screenName
    readonly property string summary: Weather.city + ": " + Weather.temperature + "°, " + Weather.text
        + (Weather.stale ? " (" + qsTr("out of date") + ")" : "")

    function toggle() {
        if (Menus.tooltip && Menus.tooltip.owner === "weather") Menus.tooltip = null
        const p = root.mapToItem(null, root.width / 2, root.height / 2)
        Menus.weather = root.open ? null : { center: p.x, centerY: p.y, screen: root.screenName }
    }

    visible: Weather.shown
    width: !visible ? 0 : vertical ? parent.width : row.implicitWidth + 16
    height: !visible ? 0 : vertical ? row.implicitHeight + 16 : parent.height
    onVisibleChanged: if (!visible && open) Menus.weather = null

    Rectangle {
        anchors { fill: parent; topMargin: root.vertical ? 0 : 4; bottomMargin: root.vertical ? 0 : 4
                  leftMargin: root.vertical ? 4 : 0; rightMargin: root.vertical ? 4 : 0 }
        radius: Theme.radius
        color: mouse.pressed ? Theme.subtlePressed : Theme.subtleHover
        opacity: mouse.containsMouse || root.open ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.fast } }
    }

    Grid {
        id: row
        anchors.centerIn: parent
        columns: root.vertical ? 1 : 2
        spacing: root.vertical ? 2 : 6
        horizontalItemAlignment: Grid.AlignHCenter
        verticalItemAlignment: Grid.AlignVCenter
        opacity: Weather.stale ? 0.6 : 1

        Image {
            width: 20
            height: 20
            source: Theme.icons + encodeURIComponent(Weather.available ? Weather.icon : "weather-none-available")
            sourceSize: Qt.size(width, height)
        }
        Text {
            text: Weather.available ? Weather.temperature + "°" : "--°"
            color: Theme.text
            font.pixelSize: Theme.fontCaption + 1
        }
    }

    // Win+W: the same as a click.
    Connections {
        target: Shell
        function onWidgetsRequested() { if (root.visible) root.toggle() }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        Accessible.role: Accessible.Button
        Accessible.name: root.summary
        onEntered: {
            const p = root.mapToItem(null, root.width / 2, root.height / 2)
            Menus.tooltip = { text: root.summary, center: p.x, centerY: p.y, screen: root.screenName, owner: "weather" }
        }
        onExited: {
            if (Menus.tooltip && Menus.tooltip.owner === "weather") Menus.tooltip = null
        }
        onClicked: root.toggle()
    }
}
