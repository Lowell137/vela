// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Personalization > Fonts: the text font of the shell and the apps
// (font-family= in vela.conf). Apps switch right away (Appearance).
Page {
    id: page
    readonly property var choices: [{ name: "", text: qsTr("Automatic (Vela)") }].concat(
        Qt.fontFamilies().map(f => ({ name: f, text: f })))
    readonly property int chosenIndex: {
        for (let i = 0; i < choices.length; ++i) {
            if (choices[i].name === Prefs.fontFamily) return i
        }
        return 0 // not installed (any more): the automatic font is used
    }

    CardGroup {
        Card {
            icon: "preferences-desktop-font"
            title: qsTr("Font")
            description: Prefs.fontFamily !== "" && page.chosenIndex === 0
                ? qsTr("\"%1\" isn't installed: the automatic font is used").arg(Prefs.fontFamily)
                : qsTr("The text of the shell, Settings and every app")
            trailing: Choice {
                model: page.choices
                currentIndex: page.chosenIndex
                onChosen: index => Prefs.fontFamily = page.choices[index].name
            }
        }
    }

    CardGroup {
        title: qsTr("Preview")
        Card {
            contentItem: Text {
                width: parent.width
                wrapMode: Text.WordWrap
                color: Theme.text
                font.family: Prefs.fontFamily !== "" ? Prefs.fontFamily : Qt.application.font.family
                font.pixelSize: Theme.fontSubtitle
                text: qsTr("The quick brown fox jumps over the lazy dog. 0123456789")
            }
        }
    }
}
