// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// System > Multitasking: how windows are arranged (snap layouts, the Task
// View hot corner, rules for where an app's windows open).
Page {
    CardGroup {
        title: qsTr("Snap windows")
        Card {
            icon: "view-split-left-right"
            title: qsTr("Show snap layouts when I drag a window to the top of my screen")
            trailing: Switch {
                checked: Prefs.snapDragLayouts
                onToggled: on => Prefs.snapDragLayouts = on
            }
        }
        Card {
            enabled: Prefs.snapDragLayouts
            opacity: enabled ? 1 : 0.45
            title: qsTr("How close to the top")
            description: qsTr("%1 px from the top of the screen").arg(Prefs.snapDragDistance)
            trailing: Slider {
                width: 220
                from: 4
                to: 200
                stepSize: 4
                live: false
                value: Prefs.snapDragDistance
                onReleased: value => Prefs.snapDragDistance = Math.round(value)
            }
        }
        Card {
            enabled: Prefs.snapDragLayouts
            opacity: enabled ? 1 : 0.45
            title: qsTr("How far to the sides")
            description: qsTr("%1 px beside the layouts").arg(Prefs.snapDragWidth)
            trailing: Slider {
                width: 220
                from: 0
                to: 960
                stepSize: 20
                live: false
                value: Prefs.snapDragWidth
                onReleased: value => Prefs.snapDragWidth = Math.round(value)
            }
        }
        Card {
            enabled: Prefs.snapDragLayouts
            opacity: enabled ? 1 : 0.45
            title: qsTr("Size of the layouts")
            description: qsTr("%1%").arg(Prefs.snapDragScale)
            trailing: Slider {
                width: 220
                from: 60
                to: 200
                stepSize: 10
                live: false
                value: Prefs.snapDragScale
                onReleased: value => Prefs.snapDragScale = Math.round(value)
            }
        }
    }

    CardGroup {
        title: qsTr("Task View")
        Card {
            icon: "view-grid"
            title: qsTr("Hot corner")
            description: qsTr("Moving the pointer into the top-left corner opens Task View")
            trailing: Switch {
                checked: Prefs.hotCorner
                onToggled: on => Prefs.hotCorner = on
            }
        }
        // Only how it looks: the corner opens Task View without them too.
        Card {
            enabled: Prefs.hotCorner
            opacity: enabled ? 1 : 0.45
            title: qsTr("Glow when the pointer is near")
            trailing: Switch {
                checked: Prefs.hotCornerGlow
                onToggled: on => Prefs.hotCornerGlow = on
            }
        }
        Card {
            enabled: Prefs.hotCorner
            opacity: enabled ? 1 : 0.45
            title: qsTr("Animation when it triggers")
            trailing: Switch {
                checked: Prefs.hotCornerAnimation
                onToggled: on => Prefs.hotCornerAnimation = on
            }
        }
    }

    CardGroup {
        title: qsTr("Related settings")
        LinkCard {
            icon: "window"
            title: qsTr("Window rules")
            description: qsTr("Where and how an app's windows open")
            onClicked: root.navigate("window-rules")
        }
    }
}
