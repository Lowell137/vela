// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton

import QtQuick

// The Game Bar's numbers as the panel (GameBar.qml) and the pinned overlay
// (GameOverlay.qml) show them, and which ones are pinned (gamebar/pinned in
// vela-shell.conf, a comma list). Unknown numbers read "—".
QtObject {
    readonly property var keys: ["fps", "frametime", "cpu", "cputemp", "gpu", "gputemp", "gpupower", "vram", "ram"]
    readonly property var pinned: String(Config.gameBar.pinned ?? "").split(",").filter(k => keys.includes(k))

    // "Show overlay" (on by default) and at least one number pinned.
    readonly property bool overlayOn: Config.gameBar.overlay ?? true
    readonly property bool overlayShown: overlayOn && pinned.length > 0

    function isPinned(key) { return pinned.includes(key) }
    function togglePin(key) {
        const list = pinned.filter(k => k !== key)
        if (list.length === pinned.length) list.push(key)
        Config.setGameBar("pinned", list.join(","))
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
