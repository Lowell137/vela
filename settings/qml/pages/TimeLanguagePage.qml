// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

Page {
    CardGroup {
        LinkCard {
            icon: "preferences-system-time"
            title: qsTr("Date & time")
            description: qsTr("Time zone, automatic clock settings")
            onClicked: root.navigate("datetime")
        }
        Card {
            icon: "preferences-desktop-locale"
            title: qsTr("Vela language")
            description: qsTr("The language of Vela's menus, windows and settings")
            // Language names stay in their own language, like on Windows.
            trailing: Choice {
                model: [qsTr("Same as the system"), "Italiano", "English", "Türkçe"]
                currentIndex: Math.max(0, ["", "it", "en", "tr"].indexOf(Prefs.language))
                onChosen: index => Prefs.language = ["", "it", "en", "tr"][index]
            }
        }
        LinkCard {
            icon: "input-keyboard"
            title: qsTr("Keyboard")
            description: qsTr("Keyboard layouts, key repeat")
            onClicked: root.navigate("keyboard")
        }
    }
}
