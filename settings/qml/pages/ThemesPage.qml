// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Personalization > Themes: a theme file (theme= in vela.conf) that changes
// the colors and corner radii of the shell and the apps. Your own go in
// ~/.config/vela/themes/<name>.conf.
Page {
    id: page
    readonly property var choices: [{ name: "", text: qsTr("Vela (default)") }].concat(Prefs.visualThemes())
    readonly property int chosenIndex: {
        for (let i = 0; i < choices.length; ++i) {
            if (choices[i].name === Prefs.visualTheme) return i
        }
        return 0 // not installed (any more): Vela's own
    }

    CardGroup {
        Card {
            icon: "preferences-desktop-theme"
            title: qsTr("Theme")
            description: Prefs.visualTheme !== "" && page.chosenIndex === 0
                ? qsTr("\"%1\" isn't installed: Vela's own look is used").arg(Prefs.visualTheme)
                : qsTr("Taskbar, Start, menus, panels and apps. Your own themes: ~/.config/vela/themes/<name>.conf")
            trailing: Choice {
                model: page.choices
                currentIndex: page.chosenIndex
                onChosen: index => Prefs.visualTheme = page.choices[index].name
            }
        }
    }
}
