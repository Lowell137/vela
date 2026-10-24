// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Personalization > Icons: the icon theme (icon-theme= in vela.conf). The
// shell, Settings and the title bars switch right away; a theme with a dark
// variant ("X-dark") follows the mode.
Page {
    id: page
    maxContentWidth: 100000 // the tiles fill the window
    readonly property var themes: Prefs.iconThemes()
    // vela and Fluent first (if installed), then the rest.
    readonly property var choices: {
        const first = ["vela", "Fluent"]
        const head = [], tail = []
        for (const t of page.themes) (first.indexOf(t.name) >= 0 ? head : tail).push(t)
        head.sort((a, b) => first.indexOf(a.name) - first.indexOf(b.name))
        return [{ name: "", text: qsTr("Automatic (Vela)") }].concat(head, tail)
    }
    readonly property bool fluentInstalled: page.themes.some(t => t.name === "Fluent")
    readonly property int chosenIndex: {
        for (let i = 0; i < choices.length; ++i) {
            if (choices[i].name === Prefs.iconTheme) return i
        }
        return 0 // not installed (any more): the automatic theme is used
    }

    // The icons each theme is shown with (like KDE's): the same ones everywhere.
    readonly property var previewIcons: ["utilities-terminal", "folder", "text-x-generic", "user-trash", "help-browser", "preferences-system"]
    // "Automatic" is shown with the theme it uses: vela if installed.
    readonly property string automaticTheme: page.themes.some(t => t.name === "vela") ? "vela" : "breeze"

    CardGroup {
        Card {
            icon: "preferences-desktop-icons"
            title: qsTr("Icon theme")
            description: Prefs.iconTheme !== "" && page.chosenIndex === 0
                ? qsTr("\"%1\" isn't installed: the automatic theme is used").arg(Prefs.iconTheme)
                : qsTr("Taskbar, Start, Settings and title bars. A theme's dark version follows the mode")
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
                        readonly property string previewTheme: modelData.name === "" ? page.automaticTheme : modelData.name
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
                            Grid {
                                anchors.centerIn: parent
                                columns: 3
                                spacing: 10
                                Repeater {
                                    model: page.previewIcons
                                    Image {
                                        required property string modelData
                                        width: 36
                                        height: 36
                                        source: "image://themeicon/" + (Theme.light ? "l" : "d") + "/"
                                            + encodeURIComponent(tile.previewTheme) + "/" + encodeURIComponent(modelData)
                                        sourceSize: Qt.size(width, height)
                                        smooth: true
                                    }
                                }
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
                            onClicked: Prefs.iconTheme = tile.modelData.name
                        }
                    }
                }
            }
        }
        Card {
            visible: !page.fluentInstalled
            title: qsTr("Fluent app and file icons")
            description: qsTr("Not installed. The AUR package fluent-icon-theme-git (search \"fluent-icon-theme\") adds it; the Vela theme uses it when present")
        }
    }
}
