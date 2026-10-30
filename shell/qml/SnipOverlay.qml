// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import Vela.Controls

// The Snipping Tool on one output (one per output, created by snip.cpp): the
// darkened photo of the output, and at the top the mode bar like Windows 11:
// Rectangle, Window, Full screen. Drag a rectangle, or click a window or the
// output; Esc cancels.
Window {
    id: root
    property string screenName
    visible: false
    color: "black"

    // The chosen rectangle (or the window under the mouse), in this output's
    // coordinates.
    property rect selection: Qt.rect(0, 0, 0, 0)
    property point origin
    property bool dragging: false
    // Text, like Rectangle, drags a rectangle; it copies the text in it.
    // Color mode: the color under the mouse, shown beside it.
    property string hoverColor: ""
    property point hoverAt
    // A recording picks like a screenshot: a rectangle or a window (the shape menu works in both).
    property string recordShape: "rect"
    // The shape the menu shows as chosen: the recording's in record mode, else the screenshot's.
    property string lensShape: "rect"
    readonly property string shownShape: Snip.mode === "record" ? recordShape : Snip.mode === "lens" ? lensShape : shape
    readonly property string pickMode: Snip.mode === "record" ? recordShape : Snip.mode === "lens" ? lensShape : Snip.mode
    readonly property bool dragMode: pickMode === "rect" || Snip.mode === "text"
    // Quick markup (Ctrl+E): choose a rectangle, draw on it, then Capture keeps the drawn picture.
    property bool markup: false
    property bool markupReady: false
    property string pen: "pen"
    property string flyout: ""
    // What each tool draws with; the pen and the highlighter keep their own color, size and opacity.
    property var cfg: ({
        pen: { color: "#e81123", size: 3, alpha: 1 },
        marker: { color: "#ffe100", size: 16, alpha: 0.5 },
        eraser: { size: 18 },
        shape: { kind: "rect", color: "#e81123", size: 3, alpha: 1 },
        redact: { kind: "box", color: "#000000" },
        text: { color: "#e81123", size: 28 }
    })
    // What a tool offers in its settings: its kinds, and which of color, size and opacity it has.
    readonly property var kinds: ({
        shape: [{ k: "rect", t: qsTr("Rectangle") }, { k: "ellipse", t: qsTr("Ellipse") }, { k: "line", t: qsTr("Line") }, { k: "arrow", t: qsTr("Arrow") }],
        redact: [{ k: "box", t: qsTr("Box") }, { k: "blur", t: qsTr("Blur") }, { k: "pixel", t: qsTr("Pixelate") }]
    })
    readonly property var sizeRange: ({ pen: [1, 24], marker: [6, 48], eraser: [6, 48], shape: [1, 16], text: [12, 96] })
    readonly property bool flyoutColor: flyout !== "eraser" && flyout !== "" && !(flyout === "redact" && cfg.redact.kind !== "box")
    readonly property bool flyoutSize: flyout !== "" && flyout !== "redact"
    readonly property bool flyoutAlpha: flyout === "pen" || flyout === "marker" || flyout === "shape"
    property var textPos: null
    readonly property var swatches: ["#000000", "#ffffff", "#e81123", "#f7630c", "#ffe100", "#16c60c", "#00b7c3", "#0078d4", "#886ce4", "#e3008c"]
    property var strokes: []
    property var undoStack: []
    property var redoStack: []
    property var currentStroke: null
    property bool erasePushed: false
    function setCfg(tool, key, value) {
        const next = JSON.parse(JSON.stringify(cfg))
        next[tool][key] = value
        cfg = next
    }
    // Quick markup, once on, comes back with every snip until it is turned off.
    function toggleMarkup() {
        if (!shotMode && !markup) return
        Snip.markup = !markup
        markup = !markup
        clearMarkup()
        selection = Qt.rect(0, 0, 0, 0)
    }
    function applyMarkupSetting() {
        if (Snip.markup && !markup) {
            markup = true
            clearMarkup()
            selection = Qt.rect(0, 0, 0, 0)
        }
    }
    Component.onCompleted: if (shotMode) applyMarkupSetting()
    function clearMarkup() {
        strokes = []
        undoStack = []
        redoStack = []
        currentStroke = null
        markupReady = false
        flyout = ""
        textPos = null
    }
    // Every change to the drawing goes through here, so undo can step back over erasing too.
    function commit(next) {
        undoStack = undoStack.concat([strokes])
        redoStack = []
        strokes = next
    }
    function undo() {
        if (undoStack.length === 0) return
        redoStack = redoStack.concat([strokes])
        strokes = undoStack[undoStack.length - 1]
        undoStack = undoStack.slice(0, -1)
    }
    function redo() {
        if (redoStack.length === 0) return
        undoStack = undoStack.concat([strokes])
        strokes = redoStack[redoStack.length - 1]
        redoStack = redoStack.slice(0, -1)
    }
    // Rubs out only what the eraser touches: a stroke is cut into the pieces that stay.
    function boxOf(s) {
        const a = s.points[0]
        const b = s.points[s.points.length - 1]
        if (s.kind === "text") return Qt.rect(a.x, a.y, s.text.length * s.width * 0.6, s.width * 1.3)
        return Qt.rect(Math.min(a.x, b.x), Math.min(a.y, b.y), Math.abs(b.x - a.x), Math.abs(b.y - a.y))
    }
    function eraseAt(x, y) {
        const reach = cfg.eraser.size / 2
        const out = []
        let changed = false
        for (const s of strokes) {
            // Shapes, boxes and text go whole; only the pen's lines are rubbed out in parts.
            if (s.kind !== "free") {
                const b = boxOf(s)
                if (x >= b.x - reach && x <= b.x + b.width + reach && y >= b.y - reach && y <= b.y + b.height + reach) changed = true
                else out.push(s)
                continue
            }
            let piece = []
            for (const p of s.points) {
                if (Math.hypot(p.x - x, p.y - y) < reach + s.width / 2) {
                    changed = true
                    if (piece.length > 1) out.push({ kind: "free", color: s.color, alpha: s.alpha, width: s.width, points: piece })
                    piece = []
                } else {
                    piece.push(p)
                }
            }
            if (piece.length > 1) out.push({ kind: "free", color: s.color, alpha: s.alpha, width: s.width, points: piece })
        }
        if (!changed) return
        if (!erasePushed) {
            undoStack = undoStack.concat([strokes])
            redoStack = []
            erasePushed = true
        }
        strokes = out
    }
    // "capture" keeps it, "share" keeps it and uploads it for a link, "lens" searches it with Google Lens.
    function capture(action) {
        const r = selection
        if (!markupReady || r.width < 2 || r.height < 2) return
        commitText()
        Snip.finishMarkup(screenName, r.x, r.y, r.width, r.height, strokes.map(s => ({
            kind: s.kind, text: s.text ? s.text : "", color: s.color, alpha: s.alpha, width: s.width,
            points: s.points.reduce((flat, p) => flat.concat([p.x, p.y]), [])
        })), action)
    }
    function rectPath(s) {
        const a = s.points[0]
        const b = s.points[1]
        const x = Math.min(a.x, b.x), y = Math.min(a.y, b.y), w = Math.abs(b.x - a.x), h = Math.abs(b.y - a.y)
        if (s.kind === "rect") return "M" + x + " " + y + "h" + w + "v" + h + "h" + (-w) + "z"
        if (s.kind === "ellipse") return "M" + x + " " + (y + h / 2) + "a" + (w / 2) + " " + (h / 2) + " 0 1 0 " + w + " 0a" + (w / 2) + " " + (h / 2) + " 0 1 0 " + (-w) + " 0z"
        let path = "M" + a.x + " " + a.y + "L" + b.x + " " + b.y
        if (s.kind === "arrow") {
            const angle = Math.atan2(b.y - a.y, b.x - a.x)
            const head = Math.max(10, s.width * 4)
            for (const turn of [0.5, -0.5])
                path += "M" + (b.x - head * Math.cos(angle - turn)) + " " + (b.y - head * Math.sin(angle - turn)) + "L" + b.x + " " + b.y
        }
        return path
    }
    // A text being typed: Enter (or a click elsewhere) puts it on the picture.
    function commitText() {
        if (textPos !== null && textEdit.text !== "") {
            commit(strokes.concat([{ kind: "text", text: textEdit.text, color: cfg.text.color, alpha: 1, width: cfg.text.size, points: [textPos, textPos] }]))
        }
        textEdit.text = ""
        textPos = null
        keyItem.forceActiveFocus()
    }
    // Text actions on the chosen rectangle: leaves the markup for the Text mode.
    function markupText() {
        const r = selection
        markup = false
        clearMarkup()
        Snip.mode = "text"
        selection = r
        beginText(r)
    }
    readonly property bool hasSelection: selection.width > 0 && selection.height > 0

    // The Text mode: the chosen rectangle stays, with the words found in it
    // (like Windows' Text Actions) to select and copy. textState: "" (choosing),
    // "busy", "words", "empty" or "failed".
    property string textState: ""
    property string textError: ""
    property var words: []
    property var lineBands: ({}) // line -> its top and bottom, from all its words
    property int hoverWord: -1
    property rect textRect: Qt.rect(0, 0, 0, 0)
    property int selFrom: -1
    property int selTo: -1
    property point menuAt
    property bool menuOpen: false

    // The shape of a screenshot (Rectangle, Window, Full screen), chosen in the bar's menu;
    // the camera button returns to it, and recording leaves it as it was.
    property string shape: "rect"
    property bool shapeMenu: false
    readonly property bool shotMode: Snip.mode === "rect" || Snip.mode === "window" || Snip.mode === "screen"
    // A small label under a button, shown after the pointer has rested on it a moment.
    component Tip: Rectangle {
        id: tip
        property string text
        property bool hovered: false
        property real gap: 8
        property bool shown: false
        onHoveredChanged: if (!hovered) shown = false
        Timer { interval: 450; running: tip.hovered; onTriggered: tip.shown = true }
        visible: shown
        z: 100
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height + gap
        width: tipText.width + 20
        height: 30
        radius: Theme.radius
        color: Theme.dialog
        border.width: 1
        border.color: Theme.stroke
        Text { id: tipText; anchors.centerIn: parent; text: tip.text; color: Theme.text; font.pixelSize: Theme.fontBody }
    }
    // A line icon drawn from an SVG path on a 24 grid.
    component Glyph: Shape {
        id: glyph
        property string d
        property color tint: Theme.text
        property color fill: "transparent"
        property real thickness: 1.6
        width: 24
        height: 24
        scale: 20 / 24
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: glyph.tint
            strokeWidth: glyph.thickness
            fillColor: glyph.fill
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: glyph.d }
        }
    }
    component Sld: Item {
        id: sld
        property real from: 0
        property real to: 1
        property real value: 0
        signal moved(real v)
        width: 170
        height: 24
        readonly property real fraction: Math.max(0, Math.min(1, (value - from) / (to - from)))
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width; height: 4; radius: 2
            color: Theme.stroke
            Rectangle { width: parent.width * sld.fraction; height: 4; radius: 2; color: Theme.accent }
        }
        Rectangle {
            width: 16; height: 16; radius: 8
            anchors.verticalCenter: parent.verticalCenter
            x: (sld.width - width) * sld.fraction
            color: Theme.accent
        }
        MouseArea {
            anchors.fill: parent
            function set(mx) { sld.moved(sld.from + (sld.to - sld.from) * Math.max(0, Math.min(1, (mx - 8) / (sld.width - 16)))) }
            onPressed: mouse => set(mouse.x)
            onPositionChanged: mouse => { if (pressed) set(mouse.x) }
        }
    }
    function shapeIcon(name) {
        return name === "window" ? "window" : name === "screen" ? "view-fullscreen" : "select-rectangular"
    }
    Connections {
        target: Snip
        function onModeChanged() {
            if (root.shotMode) {
                root.shape = Snip.mode
                root.applyMarkupSetting()
            } else if (root.markup) {
                root.markup = false
                root.clearMarkup()
            }
        }
    }
    readonly property int selMin: Math.min(selFrom, selTo)
    readonly property int selMax: Math.max(selFrom, selTo)

    function resetText() {
        textState = ""
        words = []
        selFrom = -1
        selTo = -1
        menuOpen = false
        selection = Qt.rect(0, 0, 0, 0)
    }
    function beginText(r) {
        if (r.width < 8 || r.height < 8) {
            return
        }
        textRect = r
        textState = "busy"
        Snip.recognize(root.screenName, r.x, r.y, r.width, r.height)
    }
    // The word whose box (a little larger) holds the point, else -1.
    function hitWord(x, y) {
        for (let i = 0; i < words.length; ++i) {
            const w = words[i]
            const b = lineBands[w.line] ?? { y0: w.y, y1: w.y + w.h }
            // Wide enough to cover the space between two words, as tall as the line.
            if (x >= w.x - 8 && x <= w.x + w.w + 8 && y >= b.y0 - 3 && y <= b.y1 + 3) {
                return i
            }
        }
        return -1
    }
    // The word under the point, else the nearest one.
    function wordAt(x, y) {
        // The line under the point first (the nearest one vertically), then the word nearest along it:
        // left of a line is its first word, right of it its last, like selecting text.
        let line = null
        let lineDistance = Infinity
        for (const key of Object.keys(lineBands)) {
            const b = lineBands[key]
            const d = Math.max(b.y0 - y, 0, y - b.y1)
            if (d < lineDistance) {
                lineDistance = d
                line = key
            }
        }
        if (line === null) {
            return -1
        }
        let best = -1
        let bestDistance = Infinity
        for (let i = 0; i < words.length; ++i) {
            const w = words[i]
            if (String(w.line) !== line) {
                continue
            }
            const dx = Math.max(w.x - x, 0, x - (w.x + w.w))
            if (dx < bestDistance) {
                best = i
                bestDistance = dx
            }
        }
        return best
    }
    function textOf(from, to) {
        let text = ""
        for (let i = from; i <= to; ++i) {
            if (i > from) {
                text += words[i].line !== words[i - 1].line ? "\n" : " "
            }
            text += words[i].text
        }
        return text
    }
    function copySelected() {
        if (selFrom >= 0) {
            Snip.copyText(textOf(selMin, selMax))
        }
    }
    function copyAll() {
        if (words.length > 0) {
            Snip.copyText(textOf(0, words.length - 1))
        }
    }
    function selectAll() {
        if (words.length > 0) {
            selFrom = 0
            selTo = words.length - 1
        }
    }

    Connections {
        target: Snip
        function onWordsReady(screen, found) {
            if (screen !== root.screenName || root.textState !== "busy") return
            const bands = {}
            for (const w of found) {
                const b = bands[w.line]
                if (b) {
                    b.y0 = Math.min(b.y0, w.y)
                    b.y1 = Math.max(b.y1, w.y + w.h)
                } else {
                    bands[w.line] = { y0: w.y, y1: w.y + w.h }
                }
            }
            root.lineBands = bands
            root.words = found
            root.selFrom = -1
            root.selTo = -1
            root.textState = found.length > 0 ? "words" : "empty"
        }
        function onRecognizeFailed(screen, reason) {
            if (screen !== root.screenName || root.textState !== "busy") return
            root.textError = reason
            root.textState = "failed"
        }
    }

    // The topmost window under the point (this output's coordinates).
    function windowAt(x, y) {
        const gx = x + Screen.virtualX
        const gy = y + Screen.virtualY
        for (const w of Snip.windows) {
            if (gx >= w.x && gx < w.x + w.w && gy >= w.y && gy < w.y + w.h) {
                // Only the part on this output.
                const x0 = Math.max(w.x - Screen.virtualX, 0)
                const y0 = Math.max(w.y - Screen.virtualY, 0)
                const x1 = Math.min(w.x + w.w - Screen.virtualX, root.width)
                const y1 = Math.min(w.y + w.h - Screen.virtualY, root.height)
                return Qt.rect(x0, y0, x1 - x0, y1 - y0)
            }
        }
        return Qt.rect(0, 0, 0, 0)
    }
    function finish(r) {
        if (r.width >= 2 && r.height >= 2) {
            Snip.finish(root.screenName, r.x, r.y, r.width, r.height)
        }
    }

    Image {
        id: photo
        anchors.fill: parent
        source: root.screenName !== "" ? "image://snip/" + root.screenName : ""
        cache: false
        smooth: true
    }

    // The veil, except where the choice is being made.
    // Windows 11's, measured: the screen is left at 0.37 of its brightness.
    readonly property color veil: Qt.rgba(0, 0, 0, 0.63)
    Rectangle { color: root.veil; x: 0; y: 0; width: parent.width; height: root.hasSelection ? root.selection.y : parent.height }
    Rectangle {
        visible: root.hasSelection
        color: root.veil
        x: 0; y: root.selection.y + root.selection.height
        width: parent.width; height: parent.height - y
    }
    Rectangle {
        visible: root.hasSelection
        color: root.veil
        x: 0; y: root.selection.y; width: root.selection.x; height: root.selection.height
    }
    Rectangle {
        visible: root.hasSelection
        color: root.veil
        x: root.selection.x + root.selection.width; y: root.selection.y
        width: parent.width - x; height: root.selection.height
    }
    // After the words are read the inside is darker again (Windows 11's, measured: by about 37%).
    Rectangle {
        x: root.selection.x
        y: root.selection.y
        width: root.selection.width
        height: root.selection.height
        color: Qt.rgba(0, 0, 0, 0.37)
        opacity: selectionFrame.handles ? 1 : 0
        visible: root.hasSelection && opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.animated(200) } }
    }
    // The selection's edge, like Windows 11's: a thin dashed line while it is chosen; a violet to
    // pink frame, 3 px, while the words in it are being read; then the dashed line again with
    // thick corners and a tick in the middle of each side.
    Item {
        id: selectionFrame
        visible: root.hasSelection
        x: root.selection.x - 2
        y: root.selection.y - 2
        width: root.selection.width + 4
        height: root.selection.height + 4
        readonly property bool scanning: root.textState === "busy"
        readonly property bool handles: root.textState === "words" || root.textState === "empty" || root.textState === "failed"
        function rrect(x, y, w, h, r) {
            return "M" + (x + r) + " " + y + "h" + (w - 2 * r) + "a" + r + " " + r + " 0 0 1 " + r + " " + r
                + "v" + (h - 2 * r) + "a" + r + " " + r + " 0 0 1 " + (-r) + " " + r
                + "h" + (-(w - 2 * r)) + "a" + r + " " + r + " 0 0 1 " + (-r) + " " + (-r)
                + "v" + (-(h - 2 * r)) + "a" + r + " " + r + " 0 0 1 " + r + " " + (-r) + "z"
        }
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            opacity: selectionFrame.scanning ? 0 : 1
            Behavior on opacity { NumberAnimation { duration: Theme.animated(100) } }
            ShapePath {
                strokeColor: "white"
                strokeWidth: 1
                fillColor: "transparent"
                strokeStyle: ShapePath.DashLine
                dashPattern: [3, 3]
                PathSvg { path: "M1.5 1.5h" + (selectionFrame.width - 3) + "v" + (selectionFrame.height - 3) + "h" + (-(selectionFrame.width - 3)) + "z" }
            }
        }
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            opacity: selectionFrame.scanning ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.animated(100) } }
            ShapePath {
                strokeColor: "transparent"
                fillRule: ShapePath.OddEvenFill
                fillGradient: LinearGradient {
                    x1: 0; y1: 0
                    x2: selectionFrame.width; y2: selectionFrame.height
                    GradientStop { position: 0; color: "#6B5BFF" }
                    GradientStop { position: 1; color: "#FF5FA8" }
                }
                PathSvg {
                    path: selectionFrame.rrect(0, 0, selectionFrame.width, selectionFrame.height, 5)
                        + selectionFrame.rrect(3, 3, selectionFrame.width - 6, selectionFrame.height - 6, 3)
                }
            }
        }
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            opacity: selectionFrame.handles ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.animated(100) } }
            ShapePath {
                strokeColor: "white"
                strokeWidth: 4
                fillColor: "transparent"
                capStyle: ShapePath.FlatCap
                PathSvg {
                    readonly property real w: selectionFrame.width
                    readonly property real h: selectionFrame.height
                    path: "M2 22V2H22 M" + (w - 22) + " 2H" + (w - 2) + "V22 M2 " + (h - 22) + "V" + (h - 2) + "H22 M"
                        + (w - 22) + " " + (h - 2) + "H" + (w - 2) + "V" + (h - 22)
                        + " M" + (w / 2 - 8) + " 2h16 M" + (w / 2 - 8) + " " + (h - 2) + "h16"
                        + " M2 " + (h / 2 - 8) + "v16 M" + (w - 2) + " " + (h / 2 - 8) + "v16"
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.CrossCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: mouse => {
            if (mouse.button === Qt.RightButton) {
                Snip.cancel()
                return
            }
            if (Snip.mode === "color") {
                Snip.pickColor(root.screenName, mouse.x, mouse.y)
                return
            }
            if (root.dragMode) {
                if (root.markup) root.clearMarkup()
                root.origin = Qt.point(mouse.x, mouse.y)
                root.dragging = true
                root.selection = Qt.rect(mouse.x, mouse.y, 0, 0)
            }
        }
        onPositionChanged: mouse => {
            if (Snip.mode === "color") {
                root.hoverAt = Qt.point(mouse.x, mouse.y)
                root.hoverColor = Snip.colorAt(root.screenName, mouse.x, mouse.y)
            } else if (root.dragMode && root.dragging) {
                root.selection = Qt.rect(Math.min(root.origin.x, mouse.x), Math.min(root.origin.y, mouse.y),
                    Math.abs(mouse.x - root.origin.x), Math.abs(mouse.y - root.origin.y))
            } else if (root.pickMode === "window") {
                if (!root.markupReady) root.selection = root.windowAt(mouse.x, mouse.y)
            } else if (root.pickMode === "screen") {
                if (!root.markupReady) root.selection = Qt.rect(0, 0, root.width, root.height)
            }
        }
        onExited: if (!root.dragMode && !root.markupReady) root.selection = Qt.rect(0, 0, 0, 0)
        onReleased: mouse => {
            if (mouse.button !== Qt.LeftButton || Snip.mode === "color") return
            if (root.dragMode) {
                root.dragging = false
                if (root.markup) {
                    root.markupReady = root.selection.width >= 8 && root.selection.height >= 8
                    if (!root.markupReady) root.selection = Qt.rect(0, 0, 0, 0)
                    return
                }
                if (Snip.mode === "text") {
                    root.beginText(root.selection)
                } else if (Snip.mode === "record" && root.selection.width < 6 && root.selection.height < 6) {
                    root.finish(Qt.rect(0, 0, root.width, root.height)) // a click: the whole output
                } else {
                    root.finish(root.selection)
                }
            } else if (root.markup) {
                // A window or the whole output is the picture to mark up.
                root.clearMarkup()
                root.selection = root.pickMode === "window" ? root.windowAt(mouse.x, mouse.y) : Qt.rect(0, 0, root.width, root.height)
                root.markupReady = root.selection.width >= 8 && root.selection.height >= 8
            } else if (root.pickMode === "window") {
                root.finish(root.windowAt(mouse.x, mouse.y))
            } else {
                root.finish(Qt.rect(0, 0, root.width, root.height))
            }
        }
    }

    // Quick markup: the strokes, the drawing area over the chosen rectangle and the bar under it.
    Item {
        visible: root.markup && root.hasSelection
        x: root.selection.x; y: root.selection.y
        width: root.selection.width; height: root.selection.height
        clip: true
        Repeater {
            model: root.currentStroke ? root.strokes.concat([root.currentStroke]) : root.strokes
            delegate: Item {
                id: mark
                required property var modelData
                readonly property string kind: modelData.kind
                readonly property rect box: root.boxOf(modelData)
                readonly property color ink: Qt.rgba(Qt.color(modelData.color).r, Qt.color(modelData.color).g, Qt.color(modelData.color).b, modelData.alpha)
                x: -root.selection.x; y: -root.selection.y
                width: root.width; height: root.height
                Shape {
                    anchors.fill: parent
                    visible: mark.kind !== "box" && mark.kind !== "blur" && mark.kind !== "pixel" && mark.kind !== "text"
                    preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        strokeColor: mark.ink
                        strokeWidth: mark.modelData.width
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        joinStyle: ShapePath.RoundJoin
                        PathPolyline { path: mark.kind === "free" ? mark.modelData.points : [] }
                        PathSvg { path: mark.kind === "free" || mark.kind === "text" ? "" : root.rectPath(mark.modelData) }
                    }
                }
                Rectangle {
                    visible: mark.kind === "box"
                    x: mark.box.x; y: mark.box.y; width: mark.box.width; height: mark.box.height
                    color: mark.modelData.color
                }
                // Blur and pixelate redraw the photo under the box from a smaller picture.
                ShaderEffectSource {
                    id: under
                    visible: false
                    sourceItem: photo
                    sourceRect: mark.box
                    width: mark.box.width; height: mark.box.height
                    smooth: false
                }
                ShaderEffectSource {
                    visible: mark.kind === "pixel"
                    sourceItem: photo
                    sourceRect: mark.box
                    x: mark.box.x; y: mark.box.y; width: mark.box.width; height: mark.box.height
                    textureSize: Qt.size(Math.max(1, mark.box.width / 12), Math.max(1, mark.box.height / 12))
                    smooth: false
                    live: true
                }
                MultiEffect {
                    visible: mark.kind === "blur"
                    source: under
                    x: mark.box.x; y: mark.box.y; width: mark.box.width; height: mark.box.height
                    autoPaddingEnabled: false
                    blurEnabled: true
                    blurMax: 48
                    blur: 1
                }
                Text {
                    visible: mark.kind === "text"
                    x: mark.box.x; y: mark.box.y
                    text: mark.modelData.text ? mark.modelData.text : ""
                    color: mark.modelData.color
                    font.pixelSize: mark.modelData.width
                }
            }
        }
    }
    MouseArea {
        visible: root.markup && root.markupReady
        x: root.selection.x; y: root.selection.y
        width: root.selection.width; height: root.selection.height
        acceptedButtons: Qt.LeftButton
        cursorShape: Qt.CrossCursor
        function point(mouse) { return Qt.point(mouse.x + x, mouse.y + y) }
        onPressed: mouse => {
            root.flyout = ""
            const p = point(mouse)
            if (root.pen === "text") {
                root.commitText()
                root.textPos = p
                textEdit.forceActiveFocus()
                return
            }
            root.commitText()
            if (root.pen === "eraser") {
                root.erasePushed = false
                root.eraseAt(p.x, p.y)
                return
            }
            const c = root.cfg[root.pen]
            if (root.pen === "shape" || root.pen === "redact") {
                root.currentStroke = { kind: c.kind, color: c.color, alpha: c.alpha === undefined ? 1 : c.alpha, width: c.size === undefined ? 0 : c.size, points: [p, p] }
                return
            }
            root.currentStroke = { kind: "free", color: c.color, alpha: c.alpha, width: c.size, points: [p, Qt.point(p.x + 0.01, p.y)] }
        }
        onPositionChanged: mouse => {
            if (!pressed) return
            const p = point(mouse)
            if (root.pen === "eraser") { root.eraseAt(p.x, p.y); return }
            const c = root.currentStroke
            if (!c) return
            if (c.kind !== "free") {
                root.currentStroke = { kind: c.kind, color: c.color, alpha: c.alpha, width: c.width, points: [c.points[0], p] }
                return
            }
            // Dense points, so the eraser can cut a stroke anywhere along it.
            const last = c.points[c.points.length - 1]
            const steps = Math.max(1, Math.ceil(Math.hypot(p.x - last.x, p.y - last.y) / 2))
            const more = []
            for (let i = 1; i <= steps; i++) more.push(Qt.point(last.x + (p.x - last.x) * i / steps, last.y + (p.y - last.y) * i / steps))
            root.currentStroke = { kind: "free", color: c.color, alpha: c.alpha, width: c.width, points: c.points.concat(more) }
        }
        onReleased: {
            const c = root.currentStroke
            root.currentStroke = null
            if (!c) return
            if (c.kind !== "free" && Math.hypot(c.points[1].x - c.points[0].x, c.points[1].y - c.points[0].y) < 4) return
            root.commit(root.strokes.concat([c]))
        }
    }
    TextInput {
        id: textEdit
        visible: root.textPos !== null
        x: root.textPos ? root.textPos.x : 0
        y: root.textPos ? root.textPos.y : 0
        color: root.cfg.text.color
        font.pixelSize: root.cfg.text.size
        selectByMouse: true
        Keys.onReturnPressed: root.commitText()
        Keys.onEnterPressed: root.commitText()
        Keys.onEscapePressed: { text = ""; root.commitText() }
        Rectangle { z: -1; anchors.fill: parent; anchors.margins: -3; color: "transparent"; border.width: 1; border.color: Theme.accent }
    }
    Rectangle {
        id: markupBar
        visible: root.markup && root.markupReady && root.hasSelection
        x: Math.max(8, Math.min(root.selection.x + root.selection.width / 2 - width / 2, root.width - width - 8))
        y: Math.min(root.selection.y + root.selection.height + 12, root.height - height - 12)
        width: markupRow.width + 16
        height: 48
        radius: Theme.radiusOverlay
        color: Theme.dialog
        border.width: 1
        border.color: Theme.stroke
        Row {
            id: markupRow
            anchors.centerIn: parent
            spacing: 2
            Rectangle {
                width: 84; height: 34; radius: Theme.radius
                anchors.verticalCenter: parent.verticalCenter
                color: captureMouse.containsMouse ? Theme.accentFillHover : Theme.accentFill
                Text { anchors.centerIn: parent; text: qsTr("Capture"); color: Theme.accentText; font.pixelSize: Theme.fontBody }
                MouseArea { id: captureMouse; anchors.fill: parent; hoverEnabled: true; onClicked: root.capture("capture") }
            }
            Repeater {
                model: [
                    { act: "share", tip: qsTr("Upload and copy the link"),
                      d: "M12 15V4 M8 8l4-4 4 4 M5 14v4a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2v-4" },
                    { act: "lens", tip: qsTr("Search with Google Lens"),
                      d: "M4 8V7a3 3 0 0 1 3-3h1 M16 4h1a3 3 0 0 1 3 3v1 M20 16v1a3 3 0 0 1-3 3h-1 M8 20H7a3 3 0 0 1-3-3v-1 M12 8.8a3.2 3.2 0 1 0 0 6.4a3.2 3.2 0 1 0 0-6.4z" },
                    { act: "text", tip: qsTr("Text actions"),
                      d: "M3.5 8V6.5A2.5 2.5 0 0 1 6 4H8 M16 4h2a2.5 2.5 0 0 1 2.5 2.5V8 M3.5 16v1.5A2.5 2.5 0 0 0 6 20h2 M16 20h2a2.5 2.5 0 0 0 2.5-2.5V16 M7.5 9h9 M7.5 12h9 M7.5 15h5" }
                ]
                delegate: Rectangle {
                    id: actionButton
                    required property var modelData
                    width: 36; height: 34; radius: Theme.radius
                    anchors.verticalCenter: parent.verticalCenter
                    color: actionMouse.containsMouse ? Theme.subtleHover : "transparent"
                    Glyph { anchors.centerIn: parent; d: actionButton.modelData.d }
                    Tip { text: actionButton.modelData.tip; hovered: actionMouse.containsMouse }
                    MouseArea {
                        id: actionMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (actionButton.modelData.act === "text") root.markupText()
                            else root.capture(actionButton.modelData.act)
                        }
                    }
                }
            }
            Rectangle { width: 1; height: 24; anchors.verticalCenter: parent.verticalCenter; color: Theme.stroke }
            Repeater {
                model: [
                    { pen: "pen", tip: qsTr("Pen"),
                      body: "M15.5 4.5l4 4L8.5 19.5l-4.5 1 1-4.5z M13 7l4 4", mark: "M5 16l3 3-4 1z" },
                    { pen: "marker", tip: qsTr("Highlighter"),
                      body: "M14.5 4.5l5 5L11 18H7v-4z", mark: "M4.5 21h9" },
                    { pen: "eraser", tip: qsTr("Eraser"),
                      body: "M4 14.5L13.5 5a2 2 0 0 1 2.8 0l3.7 3.7a2 2 0 0 1 0 2.8L12.5 19H8z M8 11l6 6", mark: "" },
                    { pen: "shape", tip: qsTr("Shapes"),
                      body: "M3.5 5.5h9v9h-9z M15 9.5a4.5 4.5 0 1 0 .01 0z", mark: "" },
                    { pen: "redact", tip: qsTr("Hide: box, blur, pixelate"),
                      body: "M4 5h16v14H4z M4 12h16 M12 5v14", mark: "" },
                    { pen: "text", tip: qsTr("Text"),
                      body: "M5 6V4h14v2 M12 4v16 M9 20h6", mark: "" }
                ]
                delegate: Rectangle {
                    id: penButton
                    required property var modelData
                    readonly property bool current: root.pen === modelData.pen
                    width: 36; height: 38; radius: Theme.radius
                    anchors.verticalCenter: parent.verticalCenter
                    color: current || penMouse.containsMouse ? Theme.subtleHover : "transparent"
                    border.width: current ? 1 : 0
                    border.color: Theme.stroke
                    Glyph { anchors.centerIn: parent; anchors.verticalCenterOffset: -2; d: penButton.modelData.body }
                    Glyph {
                        visible: penButton.modelData.mark !== ""
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: -2
                        d: penButton.modelData.mark
                        tint: root.cfg[penButton.modelData.pen] ? root.cfg[penButton.modelData.pen].color : Theme.text
                        fill: penButton.modelData.pen === "pen" ? tint : "transparent"
                        thickness: penButton.modelData.pen === "marker" ? 3 : 1.6
                    }
                    Image {
                        visible: penButton.current
                        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 1 }
                        width: 10; height: 10
                        source: Theme.icons + "pan-down-symbolic"
                        sourceSize: Qt.size(10, 10)
                    }
                    Tip { text: penButton.modelData.tip; hovered: penMouse.containsMouse && root.flyout === "" }
                    MouseArea {
                        id: penMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        // The chosen tool opens its settings; another one just becomes the chosen one.
                        onClicked: {
                            if (penButton.current) root.flyout = root.flyout === "" ? penButton.modelData.pen : ""
                            else { root.pen = penButton.modelData.pen; root.flyout = "" }
                        }
                    }
                }
            }
            Rectangle { width: 1; height: 24; anchors.verticalCenter: parent.verticalCenter; color: Theme.stroke }
            Repeater {
                model: [
                    { act: "undo", d: "M9 14L4 9l5-5 M4 9h10a6 6 0 0 1 0 12h-3", tip: qsTr("Undo"), on: root.undoStack.length > 0 },
                    { act: "redo", d: "M15 14l5-5-5-5 M20 9H10a6 6 0 0 0 0 12h3", tip: qsTr("Redo"), on: root.redoStack.length > 0 },
                    { act: "clear", d: "M4 7h16 M9 7V4h6v3 M6 7l1 13h10l1-13 M10 11v5 M14 11v5", tip: qsTr("Clear all"), on: root.strokes.length > 0 }
                ]
                delegate: Rectangle {
                    id: editButton
                    required property var modelData
                    width: 36; height: 34; radius: Theme.radius
                    anchors.verticalCenter: parent.verticalCenter
                    opacity: modelData.on ? 1 : 0.4
                    color: editMouse.containsMouse && modelData.on ? Theme.subtleHover : "transparent"
                    Glyph { anchors.centerIn: parent; d: editButton.modelData.d }
                    Tip { text: editButton.modelData.tip; hovered: editMouse.containsMouse }
                    MouseArea {
                        id: editMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (!editButton.modelData.on) return
                            if (editButton.modelData.act === "undo") root.undo()
                            else if (editButton.modelData.act === "redo") root.redo()
                            else root.commit([])
                        }
                    }
                }
            }
        }
    }
    // The tool's settings, over the bar: its kind, color, size and opacity, as far as it has them.
    Rectangle {
        visible: markupBar.visible && root.flyout !== ""
        x: Math.max(8, Math.min(markupBar.x + markupBar.width / 2 - width / 2, root.width - width - 8))
        y: markupBar.y - height - 8
        width: 224
        height: flyoutColumn.height + 24
        radius: Theme.radiusOverlay
        color: Theme.dialog
        border.width: 1
        border.color: Theme.stroke
        MouseArea { anchors.fill: parent } // clicks stay on the flyout
        Column {
            id: flyoutColumn
            x: 12; y: 12
            width: parent.width - 24
            spacing: 8
            Flow {
                width: parent.width
                spacing: 6
                visible: root.kinds[root.flyout] !== undefined
                Repeater {
                    model: root.kinds[root.flyout] ? root.kinds[root.flyout] : []
                    delegate: Rectangle {
                        required property var modelData
                        readonly property bool chosen: root.flyout !== "" && root.cfg[root.flyout].kind === modelData.k
                        width: kindText.width + 20; height: 28; radius: Theme.radius
                        color: chosen ? Qt.alpha(Theme.accent, 0.35) : kindMouse.containsMouse ? Theme.subtleHover : "transparent"
                        border.width: 1
                        border.color: chosen ? Theme.accent : Theme.stroke
                        Text { id: kindText; anchors.centerIn: parent; text: parent.modelData.t; color: Theme.text; font.pixelSize: Theme.fontBody }
                        MouseArea { id: kindMouse; anchors.fill: parent; hoverEnabled: true; onClicked: root.setCfg(root.flyout, "kind", parent.modelData.k) }
                    }
                }
            }
            Grid {
                visible: root.flyoutColor
                columns: 5
                spacing: 6
                Repeater {
                    model: root.swatches
                    delegate: Rectangle {
                        required property string modelData
                        width: 32; height: 28; radius: 4
                        color: modelData
                        border.width: root.flyout !== "" && root.cfg[root.flyout].color === modelData ? 2 : 1
                        border.color: root.flyout !== "" && root.cfg[root.flyout].color === modelData ? Theme.accent : Theme.stroke
                        MouseArea { anchors.fill: parent; onClicked: root.setCfg(root.flyout, "color", parent.modelData) }
                    }
                }
            }
            Text { visible: root.flyoutSize; text: qsTr("Size"); color: Theme.text; font.pixelSize: Theme.fontBody }
            Sld {
                visible: root.flyoutSize
                from: root.sizeRange[root.flyout] ? root.sizeRange[root.flyout][0] : 0
                to: root.sizeRange[root.flyout] ? root.sizeRange[root.flyout][1] : 1
                value: root.flyoutSize ? root.cfg[root.flyout].size : 0
                onMoved: v => root.setCfg(root.flyout, "size", Math.round(v))
            }
            Text { visible: root.flyoutAlpha; text: qsTr("Opacity"); color: Theme.text; font.pixelSize: Theme.fontBody }
            Sld {
                visible: root.flyoutAlpha
                from: 0.1
                to: 1
                value: root.flyoutAlpha ? root.cfg[root.flyout].alpha : 1
                onMoved: v => root.setCfg(root.flyout, "alpha", Math.round(v * 20) / 20)
            }
        }
    }

    // The words, and the mouse once the rectangle is chosen.
    Item {
        visible: root.textState !== ""
        anchors.fill: parent
        // Like Windows': no boxes at rest; the word under the pointer and the chosen words are
        // marked in blue, all the words of a line as tall as the line.
        Repeater {
            model: root.words
            delegate: Rectangle {
                required property var modelData
                required property int index
                readonly property bool picked: root.selFrom >= 0 && index >= root.selMin && index <= root.selMax
                readonly property var band: root.lineBands[modelData.line] ?? { y0: modelData.y, y1: modelData.y + modelData.h }
                // Chosen words of a line form one strip: a word reaches the left edge of the next one.
                readonly property var next: index + 1 < root.words.length ? root.words[index + 1] : null
                readonly property bool joined: picked && next !== null && next.line === modelData.line && index < root.selMax
                x: modelData.x - 2
                y: band.y0 - 1
                width: joined ? next.x - modelData.x : modelData.w + 4
                height: band.y1 - band.y0 + 2
                radius: 3
                color: Qt.rgba(0.25, 0.5, 0.95, 0.55)
                opacity: picked ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.animated(80) } }
            }
        }
        Text {
            visible: root.textState !== "words"
            x: root.textRect.x + (root.textRect.width - width) / 2
            y: root.textRect.y + (root.textRect.height - height) / 2
            text: root.textState === "busy" ? qsTr("Reading text…")
                : root.textState === "empty" ? qsTr("No text found") : root.textError
            color: "white"
            font.pixelSize: Theme.fontBody
            style: Text.Outline
            styleColor: "black"
        }
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true
            cursorShape: Qt.IBeamCursor
            property bool selecting: false
            onPressed: mouse => {
                root.menuOpen = false
                const r = root.textRect
                const inside = mouse.x >= r.x && mouse.x < r.x + r.width && mouse.y >= r.y && mouse.y < r.y + r.height
                if (!inside) {
                    root.resetText() // choose another rectangle
                    return
                }
                if (mouse.button === Qt.RightButton) {
                    root.menuAt = Qt.point(mouse.x, mouse.y)
                    root.menuOpen = root.textState === "words"
                    return
                }
                if (root.textState !== "words") return
                // A click on no word chooses nothing (and the menu stays closed).
                const hit = root.hitWord(mouse.x, mouse.y)
                if (hit < 0) {
                    root.selFrom = -1
                    root.selTo = -1
                    return
                }
                selecting = true
                root.selFrom = hit
                root.selTo = hit
            }
            onPositionChanged: mouse => {
                root.hoverWord = root.textState === "words" ? root.hitWord(mouse.x, mouse.y) : -1
                if (selecting) root.selTo = root.wordAt(mouse.x, mouse.y)
            }
            onExited: root.hoverWord = -1
            onReleased: mouse => {
                if (selecting && root.selFrom >= 0) {
                    root.menuAt = Qt.point(mouse.x, mouse.y) // like Windows: the menu comes with the selection
                    root.menuOpen = true
                }
                selecting = false
            }
        }
        Rectangle {
            visible: root.menuOpen
            // Beside the pointer (right and a little below), where the click or the selection ended.
            x: Math.max(8, Math.min(root.menuAt.x + 16, root.width - width - 8))
            y: Math.max(8, Math.min(root.menuAt.y + 8, root.height - height - 8))
            // Only icons, like Windows': copy the chosen words, choose all of them.
            width: 76
            height: 38
            radius: Theme.radiusOverlay
            color: Theme.dialog
            border.width: 1
            border.color: Theme.stroke
            Row {
                id: menuItems
                anchors.centerIn: parent
                spacing: 4
                Repeater {
                    model: [
                        { icon: "edit-copy-symbolic", copy: true },
                        { icon: "edit-select-all-symbolic", copy: false }
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        width: 32
                        height: 30
                        radius: Theme.radius
                        color: itemMouse.containsMouse ? Theme.subtleHover : "transparent"
                        // Copy: one rounded square behind another.
                        Shape {
                            visible: modelData.copy
                            preferredRendererType: Shape.CurveRenderer
                            anchors.centerIn: parent
                            width: 24
                            height: 24
                            scale: 20 / 24
                            ShapePath {
                                strokeColor: Theme.text
                                strokeWidth: 1.6
                                fillColor: "transparent"
                                capStyle: ShapePath.RoundCap
                                joinStyle: ShapePath.RoundJoin
                                PathSvg { path: "M11.5 3.5h6A2.5 2.5 0 0 1 20 6v8.5a2.5 2.5 0 0 1-2.5 2.5h-6A2.5 2.5 0 0 1 9 14.5V6a2.5 2.5 0 0 1 2.5-2.5z M6.5 7.5H6A2.5 2.5 0 0 0 3.5 10v7.5A2.5 2.5 0 0 0 6 20h6.5a2.5 2.5 0 0 0 2.5-2.2" }
                            }
                        }
                        // The second button: a dashed square round four small cells.
                        Shape {
                            visible: !modelData.copy
                            preferredRendererType: Shape.CurveRenderer
                            anchors.centerIn: parent
                            width: 24
                            height: 24
                            scale: 20 / 24
                            ShapePath {
                                strokeColor: Theme.text
                                strokeWidth: 1.4
                                fillColor: "transparent"
                                capStyle: ShapePath.RoundCap
                                joinStyle: ShapePath.RoundJoin
                                strokeStyle: ShapePath.DashLine
                                dashPattern: [1.6, 1.6]
                                PathSvg { path: "M5.5 3h13A2.5 2.5 0 0 1 21 5.5v13a2.5 2.5 0 0 1-2.5 2.5h-13A2.5 2.5 0 0 1 3 18.5v-13A2.5 2.5 0 0 1 5.5 3z" }
                            }
                            ShapePath {
                                strokeColor: Theme.text
                                strokeWidth: 1.5
                                fillColor: "transparent"
                                capStyle: ShapePath.RoundCap
                                joinStyle: ShapePath.RoundJoin
                                PathSvg {
                                    path: "M8.5 7h1A1.5 1.5 0 0 1 11 8.5v1A1.5 1.5 0 0 1 9.5 11h-1A1.5 1.5 0 0 1 7 9.5v-1A1.5 1.5 0 0 1 8.5 7z "
                                        + "M14.5 7h1A1.5 1.5 0 0 1 17 8.5v1a1.5 1.5 0 0 1-1.5 1.5h-1a1.5 1.5 0 0 1-1.5-1.5v-1A1.5 1.5 0 0 1 14.5 7z "
                                        + "M8.5 13h1a1.5 1.5 0 0 1 1.5 1.5v1A1.5 1.5 0 0 1 9.5 17h-1A1.5 1.5 0 0 1 7 15.5v-1A1.5 1.5 0 0 1 8.5 13z "
                                        + "M14.5 13h1a1.5 1.5 0 0 1 1.5 1.5v1a1.5 1.5 0 0 1-1.5 1.5h-1a1.5 1.5 0 0 1-1.5-1.5v-1a1.5 1.5 0 0 1 1.5-1.5z"
                                }
                            }
                        }
                        Tip { text: modelData.copy ? qsTr("Copy text") : qsTr("Select all"); hovered: itemMouse.containsMouse; gap: 12 }
                        MouseArea {
                            id: itemMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                root.menuOpen = false
                                if (modelData.copy) root.copySelected(); else root.selectAll()
                            }
                        }
                    }
                }
            }
        }
    }

    // Color mode: the hex of the pixel, with a swatch, near the mouse.
    Rectangle {
        visible: Snip.mode === "color" && root.hoverColor !== ""
        x: Math.min(root.hoverAt.x + 18, root.width - width - 8)
        y: Math.min(root.hoverAt.y + 18, root.height - height - 8)
        width: swatchText.width + 44
        height: 30
        radius: Theme.radius
        color: Theme.dialog
        border.width: 1
        border.color: Theme.stroke
        Rectangle { x: 6; anchors.verticalCenter: parent.verticalCenter; width: 18; height: 18; radius: 3; color: root.hoverColor; border.width: 1; border.color: Theme.stroke }
        Text { id: swatchText; x: 32; anchors.verticalCenter: parent.verticalCenter; text: root.hoverColor; color: Theme.text; font.pixelSize: Theme.fontBody }
    }

    // The mode bar, at the top center.
    Rectangle {
        id: toolbar
        anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 24 }
        width: tools.width + 16
        height: 48
        radius: Theme.radiusOverlay
        color: Theme.dialog
        border.width: 1
        border.color: Theme.stroke

        Row {
            id: tools
            anchors.centerIn: parent
            spacing: 2
            // Photo or video, like Windows 11's bar: the chosen one has a short light line under it.
            Repeater {
                model: [
                    { mode: "shot", icon: "camera-photo-symbolic", tip: qsTr("Screenshot") },
                    { mode: "record", icon: "camera-video-symbolic", tip: qsTr("Record") }
                ]
                delegate: Rectangle {
                    id: modeButton
                    required property var modelData
                    readonly property bool current: modelData.mode === "record" ? Snip.mode === "record" : root.shotMode
                    width: 40
                    height: 38
                    radius: Theme.radius
                    color: current || modeMouse.containsMouse ? Theme.subtleHover : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.animated(100) } }
                    border.width: current ? 1 : 0
                    border.color: Theme.stroke
                    Image {
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: -2
                        width: 20
                        height: 20
                        source: Theme.icons + modeButton.modelData.icon
                        sourceSize: Qt.size(width, height)
                    }
                    // The light line: the old one shrinks away, then the new one grows (Windows 11's, measured).
                    Rectangle {
                        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 4 }
                        width: modeButton.current ? 16 : 0
                        height: 3
                        radius: 1.5
                        color: Theme.accent
                        Behavior on width {
                            SequentialAnimation {
                                PauseAnimation { duration: modeButton.current ? Theme.animated(100) : 0 }
                                NumberAnimation { duration: Theme.animated(100); easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
                            }
                        }
                    }
                    Tip { text: modeButton.modelData.tip; hovered: modeMouse.containsMouse }
                    MouseArea {
                        id: modeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            root.shapeMenu = false
                            Snip.mode = modeButton.modelData.mode === "record" ? "record" : root.shape
                            root.resetText()
                        }
                    }
                }
            }
            // The shape: Rectangle, Window or Full screen, in a menu under this button.
            Rectangle {
                id: shapeButton
                width: 58
                height: 38
                radius: Theme.radius
                color: shapeMouse.containsMouse || root.shapeMenu ? Theme.subtleHover : "transparent"
                Row {
                    anchors.centerIn: parent
                    spacing: 6
                    Image {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 20
                        height: 20
                        source: Theme.icons + root.shapeIcon(root.shownShape)
                        sourceSize: Qt.size(width, height)
                    }
                    Image {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 12
                        height: 12
                        source: Theme.icons + "pan-down-symbolic"
                        sourceSize: Qt.size(width, height)
                    }
                }
                Tip { text: qsTr("Snipping mode"); hovered: shapeMouse.containsMouse && !root.shapeMenu }
                MouseArea {
                    id: shapeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.shapeMenu = !root.shapeMenu
                }
            }
            // Quick markup (Ctrl + E): draw on the chosen rectangle before keeping it.
            Rectangle {
                id: markupButton
                visible: root.shotMode || root.markup
                width: 40
                height: 38
                radius: Theme.radius
                color: root.markup ? Qt.alpha(Theme.accent, 0.35) : markupMouse.containsMouse ? Theme.subtleHover : "transparent"
                Image { anchors.centerIn: parent; width: 20; height: 20; source: Theme.icons + "insert-image-symbolic"; sourceSize: Qt.size(20, 20) }
                Tip { text: qsTr("Quick markup (Ctrl + E)"); hovered: markupMouse.containsMouse }
                MouseArea { id: markupMouse; anchors.fill: parent; hoverEnabled: true; onClicked: { root.shapeMenu = false; root.toggleMarkup() } }
            }
            Rectangle { width: 1; height: 24; anchors.verticalCenter: parent.verticalCenter; color: Theme.stroke }
            Repeater {
                model: [
                    { mode: "lens", icon: "", tip: qsTr("Search with Google Lens (Win + Shift + G)") },
                    { mode: "color", icon: "color-picker", tip: qsTr("Color picker") },
                    { mode: "text", icon: "edit-select-text", tip: qsTr("Text") }
                ]
                delegate: Rectangle {
                    id: tool
                    required property var modelData
                    readonly property bool current: Snip.mode === modelData.mode
                    width: 40
                    height: 38
                    radius: Theme.radius
                    color: current || toolMouse.containsMouse ? Theme.subtleHover : "transparent"
                    border.width: current ? 1 : 0
                    border.color: Theme.accent
                    Image {
                        visible: tool.modelData.mode !== "text" && tool.modelData.mode !== "lens"
                        anchors.centerIn: parent
                        width: 20
                        height: 20
                        source: Theme.icons + tool.modelData.icon
                        sourceSize: Qt.size(width, height)
                    }
                    Glyph {
                        visible: tool.modelData.mode === "lens"
                        anchors.centerIn: parent
                        d: "M4 8V7a3 3 0 0 1 3-3h1 M16 4h1a3 3 0 0 1 3 3v1 M20 16v1a3 3 0 0 1-3 3h-1 M8 20H7a3 3 0 0 1-3-3v-1 M12 8.8a3.2 3.2 0 1 0 0 6.4a3.2 3.2 0 1 0 0-6.4z"
                    }
                    // Windows 11's text actions icon: four corners round three lines of text.
                    Shape {
                        visible: tool.modelData.mode === "text"
                        preferredRendererType: Shape.CurveRenderer
                        anchors.centerIn: parent
                        width: 24
                        height: 24
                        scale: 20 / 24
                        ShapePath {
                            strokeColor: Theme.text
                            strokeWidth: 1.6
                            fillColor: "transparent"
                            capStyle: ShapePath.RoundCap
                            joinStyle: ShapePath.RoundJoin
                            PathSvg {
                                path: "M3.5 8V6.5A2.5 2.5 0 0 1 6 4H8 M16 4h2a2.5 2.5 0 0 1 2.5 2.5V8 M3.5 16v1.5A2.5 2.5 0 0 0 6 20h2 M16 20h2a2.5 2.5 0 0 0 2.5-2.5V16 M7.5 9h9 M7.5 12h9 M7.5 15h5"
                            }
                        }
                    }
                    Tip { text: tool.modelData.tip; hovered: toolMouse.containsMouse }
                    MouseArea {
                        id: toolMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            root.shapeMenu = false
                            Snip.mode = tool.modelData.mode
                            root.resetText()
                        }
                    }
                }
            }
            Rectangle {
                visible: root.textState === "words"
                width: copyAllText.width + 24
                height: 36
                radius: Theme.radius
                color: copyAllMouse.containsMouse ? Theme.subtleHover : "transparent"
                Text {
                    id: copyAllText
                    anchors.centerIn: parent
                    text: qsTr("Copy all text")
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                }
                MouseArea { id: copyAllMouse; anchors.fill: parent; hoverEnabled: true; onClicked: root.copyAll() }
            }
            Rectangle { width: 1; height: 24; anchors.verticalCenter: parent.verticalCenter; color: Theme.stroke }
            Rectangle {
                width: 40
                height: 36
                radius: Theme.radius
                color: closeMouse.containsMouse ? Theme.subtleHover : "transparent"
                Image {
                    anchors.centerIn: parent
                    width: 16
                    height: 16
                    source: Theme.icons + "window-close-symbolic"
                    sourceSize: Qt.size(width, height)
                }
                Tip { text: qsTr("Close"); hovered: closeMouse.containsMouse }
                MouseArea {
                    id: closeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: Snip.cancel()
                }
            }
        }
    }

    // A click anywhere else closes the shape menu.
    MouseArea {
        anchors.fill: parent
        visible: root.shapeMenu
        z: 50
        onClicked: root.shapeMenu = false
    }
    // The shape menu (Windows 11's): under its button, the chosen one with a light line at its left.
    // It opens downward in about 110 ms (cut from below) and closes at once, like Windows'.
    Rectangle {
        id: shapeMenuBox
        property real reveal: root.shapeMenu ? 1 : 0
        Behavior on reveal {
            enabled: root.shapeMenu
            NumberAnimation { duration: Theme.animated(110); easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        }
        visible: reveal > 0
        clip: true
        z: 51
        x: toolbar.x + tools.x + shapeButton.x
        y: toolbar.y + toolbar.height + 6
        width: 160
        height: (shapeItems.height + 12) * reveal
        radius: Theme.radiusOverlay
        color: Theme.dialog
        border.width: 1
        border.color: Theme.stroke
        Column {
            id: shapeItems
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 6 }
            spacing: 2
            Repeater {
                model: [
                    { shape: "rect", text: qsTr("Rectangle") },
                    { shape: "window", text: qsTr("Window") },
                    { shape: "screen", text: qsTr("Full screen") }
                ]
                delegate: Rectangle {
                    id: shapeItem
                    required property var modelData
                    readonly property bool current: root.shownShape === modelData.shape
                    width: parent.width
                    height: 38
                    radius: Theme.radius
                    color: current || shapeItemMouse.containsMouse ? Theme.subtleHover : "transparent"
                    Rectangle {
                        visible: shapeItem.current
                        x: 2
                        anchors.verticalCenter: parent.verticalCenter
                        width: 3
                        height: 16
                        radius: 1.5
                        color: Theme.accent
                    }
                    Image {
                        x: 14
                        anchors.verticalCenter: parent.verticalCenter
                        width: 20
                        height: 20
                        source: Theme.icons + root.shapeIcon(shapeItem.modelData.shape)
                        sourceSize: Qt.size(width, height)
                    }
                    Text {
                        x: 44
                        anchors.verticalCenter: parent.verticalCenter
                        text: shapeItem.modelData.text
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                    }
                    MouseArea {
                        id: shapeItemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (Snip.mode === "record") {
                                root.recordShape = shapeItem.modelData.shape // stays a recording
                                root.selection = Qt.rect(0, 0, 0, 0)
                            } else if (Snip.mode === "lens") {
                                root.lensShape = shapeItem.modelData.shape // stays a Lens search
                                root.selection = Qt.rect(0, 0, 0, 0)
                            } else {
                                root.shape = shapeItem.modelData.shape
                                if (root.markup) { root.clearMarkup(); root.selection = Qt.rect(0, 0, 0, 0) }
                                Snip.mode = shapeItem.modelData.shape
                            }
                            root.shapeMenu = false
                            root.resetText()
                        }
                    }
                }
            }
        }
    }

    Item {
        id: keyItem
        focus: true
        Keys.onEscapePressed: Snip.cancel()
        Keys.onPressed: event => {
            if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_E) {
                root.toggleMarkup()
                return
            }
            if (root.markup) {
                if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_Z) root.undo()
                else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_Y) root.redo()
                else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) root.capture("capture")
                return
            }
            if (root.textState !== "") {
                if (event.matches(StandardKey.Copy)) root.copySelected()
                else if (event.matches(StandardKey.SelectAll)) root.selectAll()
                return
            }
            // Like Windows: Enter with a rectangle chosen confirms it.
            if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && root.hasSelection) {
                if (Snip.mode === "text") root.beginText(root.selection); else root.finish(root.selection)
            }
        }
    }
}
