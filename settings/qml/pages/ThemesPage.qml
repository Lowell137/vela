// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Personalization > Themes: a theme file (theme= in vela.conf) that changes
// the colors and corner radii of the shell and the apps. Your own go in
// ~/.config/vela/themes/<name>.conf. Below it the surfaces' own styles, like
// Windhawk's three stylers: themes/taskbar/, startmenu/, notifications/.
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
                : qsTr("Colors and corners of the shell and the apps. Your own themes: ~/.config/vela/themes/<name>.conf")
            trailing: Choice {
                model: page.choices
                currentIndex: page.chosenIndex
                onChosen: index => Prefs.visualTheme = page.choices[index].name
            }
        }
    }

    CardGroup {
        title: qsTr("Styles")
        Repeater {
            model: [
                { surface: "taskbar", icon: "preferences-desktop-panel", title: qsTr("Taskbar") },
                { surface: "startmenu", icon: "start-here", title: qsTr("Start menu") },
                { surface: "notifications", icon: "preferences-desktop-notification", title: qsTr("Notification center and quick settings") },
            ]
            Card {
                id: styleCard
                required property var modelData
                readonly property var choices: [{ name: "", text: qsTr("From the theme") }].concat(Prefs.visualThemes(modelData.surface))
                readonly property string chosen: Prefs.surfaceStyles[modelData.surface] || ""
                icon: modelData.icon
                title: modelData.title
                description: qsTr("Your own styles: ~/.config/vela/themes/%1/<name>.conf").arg(modelData.surface)
                trailing: Choice {
                    model: styleCard.choices
                    currentIndex: Math.max(0, styleCard.choices.findIndex(c => c.name === styleCard.chosen))
                    onChosen: index => Prefs.setSurfaceStyle(styleCard.modelData.surface, styleCard.choices[index].name)
                }
            }
        }
        Card {
            icon: "preferences-desktop-panel"
            title: qsTr("Blur behind the taskbar")
            description: qsTr("A floating taskbar: the strip around it blurred while a window is maximized")
            trailing: Toggle {
                checked: Prefs.taskbarBackdrop
                onToggled: on => Prefs.taskbarBackdrop = on
            }
        }
        Card {
            icon: "preferences-desktop-theme"
            title: qsTr("Zoom on press")
            description: qsTr("Quick settings' sliders: the knob zooms a little, with a bounce, while it's held")
            trailing: Toggle {
                checked: Prefs.sliderPressZoom
                onToggled: on => Prefs.sliderPressZoom = on
            }
        }
    }

    CardGroup {
        Card {
            icon: "edit-undo"
            title: qsTr("Reset")
            description: qsTr("Vela's own theme and styles, the default accent and the switches above. Light or dark stays")
            trailing: Button {
                text: qsTr("Reset")
                onClicked: Prefs.resetAppearance()
            }
        }
    }
}
