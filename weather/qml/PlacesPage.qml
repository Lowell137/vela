// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// The saved places and the city search. Picking a result adds the place and
// shows it; a saved place opens on a click.
Item {
    id: page
    signal opened() // a place was chosen: show its forecast

    Timer {
        id: debounce
        interval: 400
        onTriggered: Weather.search(field.text)
    }

    TextBox {
        id: field
        x: 20
        y: 4
        width: Math.min(480, parent.width - 40)
        placeholderText: qsTr("Search for a city")
        onTextChanged: {
            if (text.trim().length < 2) {
                debounce.stop()
                Weather.clearSearch()
            } else {
                debounce.restart()
            }
        }
        Keys.onEscapePressed: text = ""
        onAccepted: {
            debounce.stop()
            Weather.search(text)
        }
    }

    Flickable {
        id: scroller
        anchors { left: parent.left; right: parent.right; top: field.bottom; topMargin: 12; bottom: parent.bottom }
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: width
        contentHeight: column.height + 20

        Column {
            id: column
            x: 20
            width: Math.min(scroller.width - 40, 720)
            spacing: 4

            // --- search results ---
            Text {
                visible: Weather.searching
                text: qsTr("Searching…")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                height: 32
            }
            Text {
                visible: Weather.searchFailed
                width: parent.width
                text: qsTr("The search needs a connection. Check it and try again.")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                wrapMode: Text.Wrap
            }
            Text {
                visible: Weather.searchEmpty
                text: qsTr("No places found.")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                height: 32
            }
            Repeater {
                model: Weather.searchResults
                delegate: Rectangle {
                    id: result
                    required property int index
                    required property var modelData
                    width: column.width
                    height: 52
                    radius: Theme.radius
                    color: resultMouse.pressed ? Theme.subtlePressed : resultMouse.containsMouse ? Theme.subtleHover : "transparent"
                    Column {
                        anchors { left: parent.left; leftMargin: 12; right: tag.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
                        Text {
                            width: parent.width
                            text: result.modelData.name
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: result.modelData.detail
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideRight
                        }
                    }
                    Text {
                        id: tag
                        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        text: result.modelData.saved ? qsTr("Saved") : qsTr("Add")
                        color: result.modelData.saved ? Theme.textTertiary : Theme.link
                        font.pixelSize: Theme.fontCaption
                    }
                    MouseArea {
                        id: resultMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (Weather.addResult(result.index)) {
                                field.text = ""
                                page.opened()
                            }
                        }
                    }
                }
            }

            // --- saved places ---
            Text {
                topPadding: 10
                bottomPadding: 6
                text: qsTr("Saved places")
                color: Theme.text
                font.pixelSize: Theme.fontBodyLarge
                font.weight: Font.DemiBold
            }
            Text {
                visible: Weather.places.length === 0
                width: parent.width
                text: qsTr("No saved places yet. Search for a city above.")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                wrapMode: Text.Wrap
            }
            Text {
                visible: Weather.corruptPlaces
                width: parent.width
                text: qsTr("The saved places file could not be read; it was kept as places.json.corrupt.")
                color: Theme.caution
                font.pixelSize: Theme.fontCaption
                wrapMode: Text.Wrap
            }
            Repeater {
                model: Weather.places
                delegate: Rectangle {
                    id: saved
                    required property int index
                    required property var modelData
                    width: column.width
                    height: 56
                    radius: Theme.radiusCard
                    color: savedMouse.pressed ? Theme.cardPressed : savedMouse.containsMouse ? Theme.cardHover : Theme.card
                    border.width: 1
                    border.color: modelData.current ? Theme.accentFill : Theme.cardStroke
                    Column {
                        anchors { left: parent.left; leftMargin: 16; right: actions.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
                        Text {
                            width: parent.width
                            text: saved.modelData.name + (saved.modelData.isDefault ? "  ·  " + qsTr("Default") : "")
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: saved.modelData.detail
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideRight
                        }
                    }
                    MouseArea {
                        id: savedMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            Weather.choose(saved.index)
                            page.opened()
                        }
                    }
                    Row {
                        id: actions
                        anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                        ToolButton {
                            icon: "shape-star"
                            usable: !saved.modelData.isDefault
                            tooltip: qsTr("Set as default place")
                            onClicked: Weather.setDefault(saved.index)
                        }
                        ToolButton {
                            icon: "edit-delete"
                            tooltip: qsTr("Remove")
                            onClicked: Weather.remove(saved.index)
                        }
                    }
                }
            }
        }
    }

    ScrollBar {
        flickable: scroller
        anchors { right: parent.right; top: scroller.top; bottom: parent.bottom }
    }
}
