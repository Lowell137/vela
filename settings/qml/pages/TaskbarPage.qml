// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Personalization > Taskbar.
Page {
    PersonalizationPage.DesktopPreview {}

    CardGroup {
        title: qsTr("Taskbar items")
        Card {
            icon: "view-grid"
            title: qsTr("Task view")
            description: qsTr("Open windows and virtual desktops (also with Win+Tab)")
            trailing: Toggle {
                checked: Prefs.taskView
                onToggled: on => Prefs.taskView = on
            }
        }
    }

    CardGroup {
        title: qsTr("Other system tray icons")
        Card {
            icon: "go-up"
            title: qsTr("Hidden icon menu")
            description: qsTr("Apps' icons go behind the arrow next to the system icons")
            trailing: Toggle {
                checked: Prefs.trayOverflow
                onToggled: on => Prefs.trayOverflow = on
            }
        }
    }

    CardGroup {
        title: qsTr("Task View")
        Card {
            icon: "view-grid"
            title: qsTr("Hot corner")
            description: qsTr("Moving the pointer into the top-left corner opens Task View")
            trailing: Toggle {
                checked: Prefs.hotCorner
                onToggled: on => Prefs.hotCorner = on
            }
        }
        // Only how it looks: the corner opens Task View without them too.
        Card {
            enabled: Prefs.hotCorner
            opacity: enabled ? 1 : 0.45
            title: qsTr("Glow when the pointer is near")
            trailing: Toggle {
                checked: Prefs.hotCornerGlow
                onToggled: on => Prefs.hotCornerGlow = on
            }
        }
        Card {
            enabled: Prefs.hotCorner
            opacity: enabled ? 1 : 0.45
            title: qsTr("Animation when it triggers")
            trailing: Toggle {
                checked: Prefs.hotCornerAnimation
                onToggled: on => Prefs.hotCornerAnimation = on
            }
        }
    }

    CardGroup {
        title: qsTr("Taskbar behaviors")
        Card {
            icon: "format-justify-center"
            title: qsTr("Taskbar alignment")
            trailing: Choice {
                model: [qsTr("Center"), qsTr("Left")]
                currentIndex: Prefs.taskbarAlignment === "left" ? 1 : 0
                onChosen: index => Prefs.taskbarAlignment = index === 1 ? "left" : "center"
            }
        }
        Card {
            icon: "video-display"
            title: qsTr("Show the taskbar on all displays")
            description: qsTr("With multiple monitors: Start, apps and the clock on each one; notification area on the main one")
            trailing: Toggle {
                checked: Prefs.taskbarAllScreens
                onToggled: on => Prefs.taskbarAllScreens = on
            }
        }
        Card {
            icon: "process-stop"
            title: qsTr("End task")
            description: qsTr("In the taskbar buttons' menu, to close an app that isn't responding right away")
            trailing: Toggle {
                checked: Prefs.endTask
                onToggled: on => Prefs.endTask = on
            }
        }
    }
}
