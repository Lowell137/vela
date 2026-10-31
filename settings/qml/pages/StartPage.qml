// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Personalization > Start: what the Start menu shows and how solid it is. The
// Start button's icon and the menu's styles are in Themes.
Page {
    CardGroup {
        Card {
            icon: "view-list-icons"
            title: qsTr("All apps")
            description: qsTr("The list below the pinned apps. Off, the others are found through the search")
            trailing: Switch {
                checked: Prefs.startAllApps
                onToggled: Prefs.startAllApps = checked
            }
        }
        Card {
            icon: "edit-opacity"
            title: qsTr("Background")
            description: qsTr("More solid than the theme: %1%. 0 keeps the theme's own look").arg(Prefs.startSolid)
            trailing: Slider {
                width: 220
                from: 0
                to: 100
                stepSize: 1
                live: false
                value: Prefs.startSolid
                onReleased: value => Prefs.startSolid = Math.round(value)
            }
        }
    }
}
