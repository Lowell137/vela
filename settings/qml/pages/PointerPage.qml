// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Accessibility > Mouse pointer: the cursor theme and size (cursor-theme=,
// cursor-size= in vela.conf). The compositor's pointer switches right away;
// apps get it when they start again.
Page {
    id: page
    readonly property var themes: Prefs.cursorThemes()
    readonly property var choices: [{ name: "", text: qsTr("Automatic") }].concat(page.themes)
    readonly property int chosenIndex: {
        for (let i = 0; i < choices.length; ++i) {
            if (choices[i].name === Prefs.cursorTheme) return i
        }
        return 0 // not installed (any more): the automatic theme is used
    }
    readonly property var sizes: [24, 32, 48, 64]
    readonly property string preview: Prefs.cursorPreview(page.choices[page.chosenIndex].name, Prefs.cursorSize)

    CardGroup {
        Card {
            icon: "cursor-arrow"
            title: qsTr("Pointer style")
            description: Prefs.cursorTheme !== "" && page.chosenIndex === 0
                ? qsTr("\"%1\" isn't installed: the automatic theme is used").arg(Prefs.cursorTheme)
                : qsTr("The cursor themes installed in your icons folders")
            trailing: Choice {
                model: page.choices
                currentIndex: page.chosenIndex
                onChosen: index => Prefs.cursorTheme = page.choices[index].name
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
            title: qsTr("Preview")
            description: page.preview === "" ? qsTr("No preview for this theme") : ""
            contentItem: Image {
                width: Prefs.cursorSize
                height: Prefs.cursorSize
                fillMode: Image.PreserveAspectFit
                source: page.preview
                visible: page.preview !== ""
            }
        }
        Card {
            icon: "dialog-information"
            title: qsTr("Open apps keep their pointer")
            description: qsTr("Apps that draw their own pointer use the new one when you open them again. The taskbar starts apps with it right away; after signing out, everything uses it.")
        }
    }
}
