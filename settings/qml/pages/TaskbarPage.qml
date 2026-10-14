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
        title: qsTr("Screen sharing")
        Card {
            icon: "video-display"
            title: qsTr("Show an indicator while my screen is shared or recorded")
            trailing: Toggle {
                checked: Prefs.captureIndicator
                onToggled: on => Prefs.captureIndicator = on
            }
        }
    }

    CardGroup {
        title: qsTr("Snap windows")
        Card {
            icon: "view-split-left-right"
            title: qsTr("Show snap layouts when I drag a window to the top of my screen")
            trailing: Toggle {
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
            icon: "go-up"
            title: qsTr("Taskbar position")
            trailing: Choice {
                readonly property var positions: ["bottom", "top", "left", "right"]
                model: [qsTr("Bottom"), qsTr("Top"), qsTr("Left"), qsTr("Right")]
                currentIndex: Math.max(0, positions.indexOf(Prefs.taskbarPosition))
                onChosen: index => Prefs.taskbarPosition = positions[index]
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
            icon: "view-fullscreen"
            title: qsTr("Automatically hide the taskbar")
            description: qsTr("It slides away, and comes back when the pointer reaches the edge of the screen")
            trailing: Toggle {
                checked: Prefs.taskbarAutoHide
                onToggled: on => Prefs.taskbarAutoHide = on
            }
        }
        Card {
            icon: "view-list-icons"
            title: qsTr("Combine taskbar buttons")
            description: qsTr("Windows of the same app under one button, or each with its own")
            trailing: Choice {
                model: [qsTr("Always"), qsTr("Never")]
                currentIndex: Prefs.taskbarCombine === "never" ? 1 : 0
                onChosen: index => Prefs.taskbarCombine = index === 1 ? "never" : "always"
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
