// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Dialogs
import QtCore
import Vela.Controls

// Gaming, like Windows 11's: the Game Bar (Win+G) and its overlay of pinned
// numbers, the crosshair (Win+Alt+C), MangoHud's in-game overlay and the
// compositor's frame statistics. The shell reads the gamebar/* keys of
// vela-shell.conf live.
Page {
    id: page
    readonly property var bar: Prefs.gameBar
    function pick(key, fallback) { return bar[key] ?? fallback }
    function set(key, value) { Prefs.setGameBar(key, value) }

    readonly property var numbers: [
        { key: "fps", text: qsTr("FPS") }, { key: "frametime", text: qsTr("Frame time") },
        { key: "cpu", text: qsTr("CPU") }, { key: "cputemp", text: qsTr("CPU temp") },
        { key: "gpu", text: qsTr("GPU") }, { key: "gputemp", text: qsTr("GPU temp") },
        { key: "gpupower", text: qsTr("GPU power") }, { key: "vram", text: qsTr("VRAM") }, { key: "ram", text: qsTr("RAM") }
    ]
    readonly property var pinned: String(pick("pinned", "")).split(",").filter(k => k !== "")
    function togglePinned(key) {
        const list = pinned.filter(k => k !== key)
        if (list.length === pinned.length) list.push(key)
        set("pinned", list.join(","))
    }

    readonly property var corners: ["top-left", "top-right", "bottom-left", "bottom-right"]
    readonly property var styles: ["dot", "cross", "gap", "circle", "image"]
    readonly property var hudPositions: ["top-left", "top-center", "top-right", "bottom-left", "bottom-center", "bottom-right"]
    readonly property var colors: ["#00ff66", "#ff3040", "#00e5ff", "#ffe100", "#ffffff", "#ff40ff"]

    CardGroup {
        Card {
            icon: "input-gaming"
            title: qsTr("Game Bar")
            description: qsTr("Press Win+G during a game for performance, audio, capture and game mode. The shortcut can be changed in Keyboard shortcuts")
        }
        Card {
            icon: "utilities-system-monitor"
            title: qsTr("Collect frame statistics")
            description: qsTr("Vela measures the frames it shows for the Game Bar's FPS and frame time. It costs nothing while the Game Bar isn't running")
            trailing: Toggle {
                checked: Prefs.gameStats
                onToggled: on => Prefs.gameStats = on
            }
        }
    }

    CardGroup {
        title: qsTr("Overlay")
        Card {
            icon: "window-pin"
            title: qsTr("Show overlay")
            description: qsTr("The pinned numbers stay over the game in a corner. Over a fullscreen game any overlay can stop direct scanout and cost a little latency: MangoHud, drawn inside the game, doesn't")
            trailing: Toggle {
                checked: page.pick("overlay", true)
                onToggled: on => page.set("overlay", on)
            }
        }
        Card {
            title: qsTr("Pinned numbers")
            description: qsTr("Also pinned with the pin next to each number in the Game Bar")
            contentItem: Flow {
                width: parent.width
                spacing: 8
                Repeater {
                    model: page.numbers
                    delegate: Button {
                        required property var modelData
                        text: modelData.text
                        accent: page.pinned.includes(modelData.key)
                        onClicked: page.togglePinned(modelData.key)
                    }
                }
            }
        }
        Card {
            title: qsTr("Corner")
            trailing: Choice {
                model: [qsTr("Top left"), qsTr("Top right"), qsTr("Bottom left"), qsTr("Bottom right")]
                currentIndex: Math.max(0, page.corners.indexOf(page.pick("corner", "top-left")))
                onChosen: index => page.set("corner", page.corners[index])
            }
        }
        Card {
            title: qsTr("Layout")
            trailing: Choice {
                model: [qsTr("Column"), qsTr("Row")]
                currentIndex: page.pick("layout", "column") === "row" ? 1 : 0
                onChosen: index => page.set("layout", index === 1 ? "row" : "column")
            }
        }
        Card {
            title: qsTr("Frame time graph")
            trailing: Toggle {
                checked: page.pick("graph", false)
                onToggled: on => page.set("graph", on)
            }
        }
        Card {
            title: qsTr("Background opacity")
            description: Math.round(page.pick("opacity", 0.6) * 100) + " %"
            trailing: Slider {
                width: 220
                from: 0
                to: 1
                stepSize: 0.05
                live: false
                value: page.pick("opacity", 0.6)
                onReleased: value => page.set("opacity", Math.round(value * 100) / 100)
            }
        }
        Card {
            title: qsTr("Size")
            description: Math.round(page.pick("scale", 1) * 100) + " %"
            trailing: Slider {
                width: 220
                from: 0.5
                to: 2
                stepSize: 0.1
                live: false
                value: page.pick("scale", 1)
                onReleased: value => page.set("scale", Math.round(value * 10) / 10)
            }
        }
    }

    CardGroup {
        title: qsTr("Crosshair")
        Card {
            icon: "crosshairs"
            title: qsTr("Crosshair")
            description: qsTr("A mark in the center of the screen. Win+Alt+C turns it on and off")
            trailing: Toggle {
                checked: page.pick("crosshair", false)
                onToggled: on => page.set("crosshair", on)
            }
        }
        Card {
            title: qsTr("Style")
            trailing: Choice {
                model: [qsTr("Dot"), qsTr("Cross"), qsTr("Cross with gap"), qsTr("Circle"), qsTr("Image")]
                currentIndex: Math.max(0, page.styles.indexOf(page.pick("crosshair-style", "cross")))
                onChosen: index => page.set("crosshair-style", page.styles[index])
            }
        }
        Card {
            visible: page.pick("crosshair-style", "cross") === "image"
            title: qsTr("Image")
            description: page.pick("crosshair-image", "") || qsTr("A PNG with a transparent background")
            trailing: Button {
                text: qsTr("Browse")
                onClicked: {
                    imageDialog.close()
                    imageDialog.open()
                }
            }
        }
        Card {
            title: qsTr("Color")
            trailing: Choice {
                model: [qsTr("Green"), qsTr("Red"), qsTr("Cyan"), qsTr("Yellow"), qsTr("White"), qsTr("Magenta")]
                currentIndex: Math.max(0, page.colors.indexOf(page.pick("crosshair-color", "#00ff66")))
                onChosen: index => page.set("crosshair-color", page.colors[index])
            }
        }
        Card {
            title: qsTr("Outline")
            description: qsTr("A thin black edge, so it shows on light backgrounds")
            trailing: Toggle {
                checked: page.pick("crosshair-outline", true)
                onToggled: on => page.set("crosshair-outline", on)
            }
        }
        Repeater {
            // Sliders: key, title, range, default.
            model: [
                { key: "crosshair-size", title: qsTr("Size"), from: 4, to: 128, step: 2, def: 24, unit: " px" },
                { key: "crosshair-thickness", title: qsTr("Thickness"), from: 1, to: 16, step: 1, def: 2, unit: " px" },
                { key: "crosshair-gap", title: qsTr("Gap"), from: 0, to: 64, step: 1, def: 4, unit: " px" },
                { key: "crosshair-opacity", title: qsTr("Opacity"), from: 0.1, to: 1, step: 0.05, def: 1, unit: "" },
                { key: "crosshair-x", title: qsTr("Horizontal offset"), from: -400, to: 400, step: 1, def: 0, unit: " px" },
                { key: "crosshair-y", title: qsTr("Vertical offset"), from: -400, to: 400, step: 1, def: 0, unit: " px" }
            ]
            delegate: Card {
                required property var modelData
                title: modelData.title
                description: (Math.round(page.pick(modelData.key, modelData.def) * 100) / 100) + modelData.unit
                trailing: Slider {
                    width: 220
                    from: modelData.from
                    to: modelData.to
                    stepSize: modelData.step
                    origin: Math.max(from, Math.min(to, 0))
                    resetValue: modelData.def
                    live: false
                    value: page.pick(modelData.key, modelData.def)
                    onReleased: value => page.set(modelData.key, Math.round(value * 100) / 100)
                }
            }
        }
    }

    CardGroup {
        title: qsTr("MangoHud")
        Card {
            visible: !MangoHud.installed
            icon: "dialog-information"
            title: qsTr("MangoHud isn't installed")
            description: qsTr("Install the mangohud package to draw the numbers inside the game itself")
        }
        Card {
            visible: MangoHud.installed
            icon: "input-gaming"
            title: qsTr("MangoHud")
            description: qsTr("Draws inside the game, so fullscreen games keep direct scanout. Settings are saved in %1").arg(MangoHud.path)
            trailing: Button {
                text: qsTr("Copy launch option")
                onClicked: MangoHud.copyLaunchOption()
            }
        }
        Card {
            visible: MangoHud.installed
            title: qsTr("Presets")
            description: qsTr("Steam: paste \"mangohud %command%\" in a game's launch options")
            trailing: Row {
                spacing: 8
                Button { text: qsTr("Minimal"); onClicked: MangoHud.applyPreset("minimal") }
                Button { text: qsTr("Full"); onClicked: MangoHud.applyPreset("full") }
                Button { text: qsTr("Battery"); onClicked: MangoHud.applyPreset("battery") }
            }
        }
        Repeater {
            model: MangoHud.installed ? [
                { key: "fps", text: qsTr("FPS") }, { key: "frametime", text: qsTr("Frame time") },
                { key: "frame_timing", text: qsTr("Frame time graph") }, { key: "cpu_stats", text: qsTr("CPU load") },
                { key: "cpu_temp", text: qsTr("CPU temperature") }, { key: "gpu_stats", text: qsTr("GPU load") },
                { key: "gpu_temp", text: qsTr("GPU temperature") }, { key: "vram", text: qsTr("VRAM") },
                { key: "ram", text: qsTr("RAM") }, { key: "battery", text: qsTr("Battery") }
            ] : []
            delegate: Card {
                required property var modelData
                title: modelData.text
                trailing: Toggle {
                    checked: MangoHud.flags[modelData.key] === true
                    onToggled: on => MangoHud.setFlag(modelData.key, on)
                }
            }
        }
        Card {
            visible: MangoHud.installed
            title: qsTr("Position")
            trailing: Choice {
                model: [qsTr("Top left"), qsTr("Top center"), qsTr("Top right"), qsTr("Bottom left"), qsTr("Bottom center"), qsTr("Bottom right")]
                currentIndex: Math.max(0, page.hudPositions.indexOf(MangoHud.values.position || "top-left"))
                onChosen: index => MangoHud.setValue("position", page.hudPositions[index])
            }
        }
        Card {
            visible: MangoHud.installed
            title: qsTr("Font size")
            description: (MangoHud.values.font_size || "24") + " px"
            trailing: Slider {
                width: 220
                from: 12
                to: 48
                stepSize: 1
                live: false
                value: Number(MangoHud.values.font_size || 24)
                onReleased: value => MangoHud.setValue("font_size", String(Math.round(value)))
            }
        }
        Card {
            visible: MangoHud.installed
            title: qsTr("Background opacity")
            description: Math.round(Number(MangoHud.values.background_alpha || 0.5) * 100) + " %"
            trailing: Slider {
                width: 220
                from: 0
                to: 1
                stepSize: 0.05
                live: false
                value: Number(MangoHud.values.background_alpha || 0.5)
                onReleased: value => MangoHud.setValue("background_alpha", String(Math.round(value * 100) / 100))
            }
        }
        Card {
            visible: MangoHud.installed
            title: qsTr("Show or hide key")
            description: qsTr("In the game, like Shift_R+F12")
            trailing: TextBox {
                width: 160
                text: MangoHud.values.toggle_hud || "Shift_R+F12"
                onEditingFinished: if (text.trim() !== "") MangoHud.setValue("toggle_hud", text.trim())
            }
        }
    }

    FileDialog {
        id: imageDialog
        title: qsTr("Choose a crosshair image")
        currentFolder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)
        nameFilters: [qsTr("PNG images (*.png)")]
        onAccepted: page.set("crosshair-image", decodeURIComponent(selectedFile.toString().replace(/^file:\/\//, "")))
    }
}
