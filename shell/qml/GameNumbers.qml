// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton

import QtQuick

// What the Game Bar's windows share (GameBar.qml, GameOverlay.qml,
// Crosshair.qml): the numbers as text ("—" when unknown) and the widgets: which
// are open (gamebar/widgets), which are pinned over the game
// (gamebar/pinned-widgets) and where they are (gamebar/pos-<id>, "screen|x|y"),
// all in vela-shell.conf.
QtObject {
    id: root
    readonly property var keys: ["fps", "frametime", "cpu", "cputemp", "gpu", "gputemp", "gpupower", "vram", "ram"]
    readonly property var widgetIds: ["capture", "audio", "performance", "media", "crosshair", "gamemode"]

    // The output the Game Bar is open on ("" closed): its widgets are drawn
    // there, and the overlay and crosshair windows of that output step aside.
    property string editScreen: ""
    // A widget asks the bar to close first (a screenshot mustn't catch it).
    signal closeRequested(var action)

    function list(key, fallback) {
        const value = Config.gameBar[key]
        if (value === undefined) return fallback
        return String(value).split(",").filter(k => widgetIds.includes(k))
    }
    readonly property var open: list("widgets", ["capture", "audio", "performance"])
    readonly property var pinnedWidgets: list("pinned-widgets", []).filter(id => id !== "crosshair" && open.includes(id))
    // "Show overlay" (on by default) and at least one widget pinned.
    readonly property bool overlayOn: Config.gameBar.overlay ?? true
    readonly property bool overlayShown: overlayOn && pinnedWidgets.length > 0
    readonly property bool crosshairOn: Config.gameBar.crosshair === true

    function isOpen(id) { return open.includes(id) }
    function isPinned(id) {
        return id === "crosshair" ? (Config.gameBar["crosshair-pinned"] ?? true) === true : pinnedWidgets.includes(id)
    }
    function setOpen(id, on) {
        const rest = open.filter(k => k !== id)
        if (on) rest.push(id)
        Config.setGameBar("widgets", rest.join(","))
        if (!on && isPinned(id) && id !== "crosshair")
            Config.setGameBar("pinned-widgets", pinnedWidgets.filter(k => k !== id).join(","))
    }
    function toggleOpen(id) { setOpen(id, !isOpen(id)) }
    // Pinning remembers where the card is and on which output.
    function togglePin(id, screen, x, y) {
        if (id === "crosshair") {
            Config.setGameBar("crosshair-pinned", !isPinned(id))
            return
        }
        const on = !isPinned(id)
        if (on) setPos(id, screen, x, y)
        const rest = pinnedWidgets.filter(k => k !== id)
        if (on) rest.push(id)
        Config.setGameBar("pinned-widgets", rest.join(","))
    }
    // {screen, x, y, stored}: stored is false until the card was placed.
    function posOf(id) {
        const parts = String(Config.gameBar["pos-" + id] ?? "").split("|")
        if (parts.length === 3 && !isNaN(Number(parts[1])) && !isNaN(Number(parts[2])))
            return { screen: parts[0], x: Number(parts[1]), y: Number(parts[2]), stored: true }
        return { screen: "", x: 0, y: 0, stored: false }
    }
    function setPos(id, screen, x, y) {
        Config.setGameBar("pos-" + id, screen + "|" + Math.round(x) + "|" + Math.round(y))
    }
    // Back to the automatic layout under the bar.
    function resetPositions() {
        for (const id of widgetIds) Config.setGameBar("pos-" + id, "")
        Config.setGameBar("crosshair-x", 0)
        Config.setGameBar("crosshair-y", 0)
    }
    // The output a pinned widget belongs to: its own, or the main one if gone.
    function screenOf(id) {
        const name = posOf(id).screen
        return Qt.application.screens.some(s => s.name === name) ? name : Shell.primaryScreen
    }

    function title(id) {
        switch (id) {
        case "capture": return qsTr("Capture")
        case "audio": return qsTr("Audio")
        case "performance": return qsTr("Performance")
        case "media": return qsTr("Media")
        case "crosshair": return qsTr("Crosshair")
        case "gamemode": return qsTr("Game mode")
        }
        return id
    }
    function icon(id) {
        switch (id) {
        case "capture": return "media-record"
        case "audio": return "audio-volume-high"
        case "performance": return "utilities-system-monitor"
        case "media": return "audio-x-generic"
        case "crosshair": return "crosshairs"
        case "gamemode": return "input-gaming"
        }
        return ""
    }

    function label(key) {
        switch (key) {
        case "fps": return qsTr("FPS")
        case "frametime": return qsTr("Frame time")
        case "cpu": return qsTr("CPU")
        case "cputemp": return qsTr("CPU temp")
        case "gpu": return qsTr("GPU")
        case "gputemp": return qsTr("GPU temp")
        case "gpupower": return qsTr("GPU power")
        case "vram": return qsTr("VRAM")
        case "ram": return qsTr("RAM")
        }
        return key
    }

    function gb(mb) { return (mb / 1024).toFixed(1) }
    function value(key) {
        const none = "—"
        switch (key) {
        case "fps": return Game.perfStale || Game.fps < 0 ? none : Game.fps.toFixed(0)
        case "frametime": return Game.perfStale || Game.frameMs < 0 ? none : Game.frameMs.toFixed(1) + " ms"
        case "cpu": return Game.cpu < 0 ? none : Game.cpu.toFixed(0) + " %"
        case "cputemp": return Game.cpuTemp < 0 ? none : Game.cpuTemp.toFixed(0) + " °C"
        case "gpu": return Game.gpu < 0 ? none : Game.gpu.toFixed(0) + " %"
        case "gputemp": return Game.gpuTemp < 0 ? none : Game.gpuTemp.toFixed(0) + " °C"
        case "gpupower": return Game.gpuPower < 0 ? none : Game.gpuPower.toFixed(0) + " W"
        case "vram": return Game.vramUsed < 0 ? none : gb(Game.vramUsed) + (Game.vramTotal > 0 ? " / " + gb(Game.vramTotal) : "") + " GB"
        case "ram": return Game.ramUsed < 0 ? none : gb(Game.ramUsed) + " / " + gb(Game.ramTotal) + " GB"
        }
        return none
    }
}
