// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Accessibility > Mouse pointer: the cursor theme and size (cursor-theme=,
// cursor-size= in vela.conf). The compositor's pointer switches right away;
// apps get it when they start again.
Page {
    id: page
    maxContentWidth: 100000 // the tiles fill the window
    readonly property var themes: Prefs.cursorThemes()
    readonly property var choices: [{ name: "", text: qsTr("Automatic") }].concat(page.themes)
    readonly property int chosenIndex: {
        for (let i = 0; i < choices.length; ++i) {
            if (choices[i].name === Prefs.cursorTheme) return i
        }
        return 0 // not installed (any more): the automatic theme is used
    }
    readonly property var sizes: [24, 32, 48, 64]

    CardGroup {
        Card {
            icon: "cursor-arrow"
            title: qsTr("Pointer style")
            description: Prefs.cursorTheme !== "" && page.chosenIndex === 0
                ? qsTr("\"%1\" isn't installed: the automatic theme is used").arg(Prefs.cursorTheme)
                : qsTr("The cursor themes installed in your icons folders")
            contentItem: Flow {
                id: grid
                // As many columns as fit, the tiles stretched to fill the row.
                readonly property real avail: parent ? parent.width : 0
                readonly property int columns: Math.max(1, Math.floor((avail + spacing) / (172 + spacing)))
                readonly property real tileWidth: (avail - (columns - 1) * spacing) / columns
                width: avail
                spacing: 12
                Repeater {
                    model: page.choices
                    delegate: Item {
                        id: tile
                        required property var modelData
                        required property int index
                        readonly property bool chosen: index === page.chosenIndex
                        // The theme's arrow, drawn at the chosen size.
                        readonly property string arrow: Prefs.cursorPreview(modelData.name, Prefs.cursorSize)
                        width: grid.tileWidth
                        height: 142
                        Rectangle {
                            id: plate
                            width: parent.width
                            height: 108
                            radius: Theme.radius
                            color: tileMouse.containsMouse ? Theme.controlHover : Theme.control
                            border.width: tile.chosen ? 2 : 1
                            border.color: tile.chosen ? Theme.accent : Theme.controlStroke
                            Image {
                                anchors.centerIn: parent
                                width: Prefs.cursorSize
                                height: Prefs.cursorSize
                                fillMode: Image.PreserveAspectFit
                                source: tile.arrow
                                visible: tile.arrow !== ""
                            }
                            Text {
                                anchors.centerIn: parent
                                visible: tile.arrow === ""
                                text: qsTr("No preview")
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                            }
                        }
                        Text {
                            anchors { top: plate.bottom; topMargin: 8; left: parent.left; right: parent.right }
                            horizontalAlignment: Text.AlignHCenter
                            text: tile.modelData.text
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            font.weight: tile.chosen ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }
                        MouseArea {
                            id: tileMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: Prefs.cursorTheme = tile.modelData.name
                        }
                    }
                }
            }
        }
        Card {
            icon: "zoom-in"
            title: qsTr("Size")
            trailing: Choice {
                model: page.sizes.indexOf(Prefs.cursorSize) < 0
                    ? page.sizes.concat([Prefs.cursorSize]).map(size => qsTr("%1 px").arg(size))
                    : page.sizes.map(size => qsTr("%1 px").arg(size))
                currentIndex: page.sizes.indexOf(Prefs.cursorSize) < 0 ? page.sizes.length : page.sizes.indexOf(Prefs.cursorSize)
                onChosen: index => { if (index < page.sizes.length) Prefs.cursorSize = page.sizes[index] }
            }
        }
        Card {
            icon: "dialog-information"
            title: qsTr("Open apps keep their pointer")
            description: qsTr("Apps that draw their own pointer use the new one when you open them again. The taskbar starts apps with it right away; after signing out, everything uses it.")
        }
    }
}
