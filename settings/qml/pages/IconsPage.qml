// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Personalization > Icons: the icon theme (icon-theme= in vela.conf). The
// shell, Settings and the title bars switch right away; a theme with a dark
// variant ("X-dark") follows the mode.
Page {
    id: page
    readonly property var themes: Prefs.iconThemes()
    readonly property var choices: [{ name: "", text: qsTr("Automatic (Breeze)") }].concat(page.themes)
    readonly property int chosenIndex: {
        for (let i = 0; i < choices.length; ++i) {
            if (choices[i].name === Prefs.iconTheme) return i
        }
        return 0 // not installed (any more): the automatic theme is used
    }

    CardGroup {
        Card {
            icon: "preferences-desktop-icons"
            title: qsTr("Icon theme")
            description: Prefs.iconTheme !== "" && page.chosenIndex === 0
                ? qsTr("\"%1\" isn't installed: the automatic theme is used").arg(Prefs.iconTheme)
                : qsTr("Taskbar, Start, Settings and title bars. A theme's dark version follows the mode")
            trailing: Choice {
                model: page.choices
                currentIndex: page.chosenIndex
                onChosen: index => Prefs.iconTheme = page.choices[index].name
            }
        }
        Card {
            title: qsTr("Preview")
            contentItem: Row {
                spacing: 16
                Repeater {
                    model: ["folder", "text-x-generic", "internet-web-browser", "utilities-terminal", "user-trash", "preferences-system"]
                    Image {
                        required property string modelData
                        width: 32
                        height: 32
                        source: Theme.icons + encodeURIComponent(modelData)
                        sourceSize: Qt.size(width, height)
                    }
                }
            }
        }
    }
}
