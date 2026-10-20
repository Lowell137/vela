// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtCore
import Vela.Controls

// Vela's Weather: its own title bar, the pages at the left (Forecast, Places,
// Settings) and the chosen one beside them. What's shown comes from Weather
// (src/weathermodel.h).
Window {
    id: root
    flags: Qt.Window | Qt.FramelessWindowHint // no title bar from Vela
    width: 1000
    height: 720
    minimumWidth: 420
    minimumHeight: 420
    visible: true
    color: Theme.background
    title: qsTr("Weather")

    onActiveChanged: Theme.windowActive = active

    readonly property bool maximized: visibility === Window.Maximized
    function toggleMaximized() { maximized ? showNormal() : showMaximized() }

    readonly property var pages: [
        { id: "forecast", icon: "weather-clear", title: qsTr("Forecast") },
        { id: "places", icon: "folder-open-location", title: qsTr("Places") },
        { id: "settings", icon: "configure", title: qsTr("Settings") }
    ]
    property string page: "forecast"
    Settings {
        id: settings
        property bool wideRail: true
    }
    readonly property bool railOpen: settings.wideRail && width >= 760

    Shortcut { sequence: "F5"; onActivated: Weather.refresh() }
    Shortcut { sequence: "Ctrl+R"; onActivated: Weather.refresh() }

    // --- the title bar ---
    Item {
        id: strip
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 44

        DragHandler {
            target: null
            onActiveChanged: if (active) root.startSystemMove()
        }
        TapHandler {
            onDoubleTapped: root.toggleMaximized()
        }

        Image {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            source: "image://appicon/weather-clear"
            sourceSize: Qt.size(width, height)
        }
        Text {
            x: 44
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.text
            opacity: root.active ? 1 : 0.5
            font.pixelSize: Theme.fontCaption
        }
        Row {
            anchors { right: parent.right; top: parent.top }
            WindowButton { glyph: "M 0 5.5 H 10"; onClicked: root.showMinimized() }
            WindowButton {
                glyph: root.maximized ? "M 0.5 2.5 H 7.5 V 9.5 H 0.5 Z M 2.5 2.5 V 0.5 H 9.5 V 7.5 H 7.5" : "M 0.5 0.5 H 9.5 V 9.5 H 0.5 Z"
                onClicked: root.toggleMaximized()
            }
            WindowButton { glyph: "M 0 0 L 10 10 M 10 0 L 0 10"; closes: true; onClicked: root.close() }
        }
    }

    // --- the pages, at the left ---
    Column {
        id: rail
        anchors { left: parent.left; top: strip.bottom; bottom: parent.bottom; leftMargin: 4 }
        width: root.railOpen ? 200 : 44
        spacing: 2
        ToolButton {
            width: 40
            height: 36
            icon: "application-menu"
            tooltip: qsTr("Open or close the navigation")
            onClicked: settings.wideRail = !settings.wideRail
        }
        Repeater {
            model: root.pages
            delegate: Rectangle {
                id: entry
                required property var modelData
                readonly property bool current: root.page === modelData.id
                width: rail.width
                height: 36
                radius: Theme.radius
                color: entryMouse.pressed ? Theme.subtlePressed : current || entryMouse.containsMouse ? Theme.subtleHover : "transparent"
                onCurrentChanged: navIndicator.track(entry, current)
                Component.onCompleted: navIndicator.track(entry, current)
                Image {
                    x: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16
                    height: 16
                    source: entry.modelData.id === "forecast" ? "image://appicon/weather-clear" : Theme.icons + encodeURIComponent(entry.modelData.icon)
                    sourceSize: Qt.size(width, height)
                }
                Text {
                    visible: root.railOpen
                    x: 44
                    anchors.verticalCenter: parent.verticalCenter
                    text: entry.modelData.title
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                }
                MouseArea {
                    id: entryMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.page = entry.modelData.id
                }
            }
        }
    }

    // The current entry's pill: it slides between entries.
    NavIndicator { id: navIndicator }

    // --- the page ---
    Rectangle {
        id: body
        anchors { left: rail.right; leftMargin: 4; right: parent.right; top: strip.bottom; bottom: parent.bottom }
        topLeftRadius: Theme.radiusOverlay
        color: Theme.layer
        clip: true

        // The page's name and, on the forecast, when it was fetched.
        Item {
            id: bar
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: 52
            z: 1
            Text {
                x: 20
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 40 - (refreshRow.visible ? refreshRow.width : 0)
                text: root.page === "forecast" ? (Weather.now.place || qsTr("Forecast")) : root.pages.find(p => p.id === root.page).title
                color: Theme.text
                font.pixelSize: 18
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Row {
                id: refreshRow
                visible: root.page === "forecast" && Weather.hasPlace
                anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                spacing: 8
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: (Weather.stale && Weather.hasData ? qsTr("Out of date · ") : "") + Weather.updatedText
                    color: Weather.stale ? Theme.caution : Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                }
                ToolButton {
                    icon: "view-refresh"
                    tooltip: qsTr("Refresh (F5)")
                    usable: !Weather.loading
                    onClicked: Weather.refresh()
                }
            }
        }

        Item {
            anchors { left: parent.left; right: parent.right; top: bar.bottom; bottom: parent.bottom }
            ForecastPage {
                anchors.fill: parent
                visible: root.page === "forecast"
                onAddPlace: root.page = "places"
            }
            PlacesPage {
                anchors.fill: parent
                visible: root.page === "places"
                onOpened: root.page = "forecast"
            }
            SettingsPage {
                anchors.fill: parent
                visible: root.page === "settings"
            }
        }
    }

    ResizeEdges {}
}
