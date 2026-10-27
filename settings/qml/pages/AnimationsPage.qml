// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Personalization > Animations: all the motion of the desktop in one place:
// an off switch for every animation, their speed, and each of them.
Page {
    CardGroup {
        title: qsTr("All animations")
        Card {
            icon: "preferences-desktop-effects"
            title: qsTr("Animations")
            description: qsTr("Off: windows, menus and everything else change at once")
            trailing: Toggle {
                checked: Prefs.animations
                onToggled: on => Prefs.animations = on
            }
        }
        Card {
            enabled: Prefs.animations
            opacity: enabled ? 1 : 0.45
            title: qsTr("Speed")
            description: qsTr("x%1: %2").arg((Prefs.animationSpeed / 100).toFixed(1))
                .arg(Prefs.animationSpeed === 100 ? qsTr("as designed") : Prefs.animationSpeed > 100 ? qsTr("faster") : qsTr("slower"))
            trailing: Slider {
                width: 220
                from: 50
                to: 200
                stepSize: 10
                live: false
                value: Prefs.animationSpeed
                onReleased: value => Prefs.animationSpeed = Math.round(value)
            }
        }
    }

    CardGroup {
        title: qsTr("Taskbar and desktop")
        enabled: Prefs.animations
        opacity: enabled ? 1 : 0.45
        Card {
            icon: "preferences-desktop-panel"
            title: qsTr("Taskbar button hop")
            description: qsTr("The button of an open app hops up when its window comes back and dips when it is minimized")
            trailing: Toggle {
                checked: Prefs.activateHop
                onToggled: on => Prefs.activateHop = on
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
        Card {
            icon: "preferences-system-windows"
            title: qsTr("Hot corner")
            description: qsTr("The animation when the corner triggers")
            trailing: Toggle {
                checked: Prefs.hotCornerAnimation
                onToggled: on => Prefs.hotCornerAnimation = on
            }
        }
    }

    CardGroup {
        title: qsTr("Windows")
        enabled: Prefs.animations
        opacity: enabled ? 1 : 0.45
        Card {
            icon: "window"
            title: qsTr("Snap layouts at the top")
            description: qsTr("While a window is dragged, the edge of the layouts shows at the top and opens near it")
            trailing: Toggle {
                checked: Prefs.snapDragPeek
                onToggled: on => Prefs.snapDragPeek = on
            }
        }
    }

    CardGroup {
        title: qsTr("Menus")
        enabled: Prefs.animations
        opacity: enabled ? 1 : 0.45
        Card {
            icon: "view-list-details"
            title: qsTr("Selection pill")
            description: qsTr("In the side menus of Settings and the apps, the pill of the chosen item hops to the new one")
            trailing: Toggle {
                checked: Prefs.navPillAnimation
                onToggled: on => Prefs.navPillAnimation = on
            }
        }
        Card {
            enabled: Prefs.navPillAnimation
            opacity: enabled ? 1 : 0.45
            icon: "system-file-manager"
            title: qsTr("Selection pill in Files")
            description: qsTr("Off: the pill of the side pane of Files jumps to the folder you open")
            trailing: Toggle {
                checked: Prefs.filesPillAnimation
                onToggled: on => Prefs.filesPillAnimation = on
            }
        }
    }

    CardGroup {
        Card {
            icon: "edit-undo"
            title: qsTr("Reset")
            description: qsTr("Every animation on, normal speed, the pill of Files off")
            trailing: Button {
                text: qsTr("Reset")
                onClicked: Prefs.resetAnimations()
            }
        }
    }
}
