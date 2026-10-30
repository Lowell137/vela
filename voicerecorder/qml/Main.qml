// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtCore
import Vela.Controls

// The Sound Recorder's window, the one of Windows 11's: the recordings at the
// left, at the right the big record button, or the player of the chosen
// recording. Rec records (src/recorder.h), Player plays (src/audioplayer.h),
// Lib is the folder (src/library.h).
Window {
    id: root
    flags: Qt.Window | Qt.FramelessWindowHint // no title bar from Vela
    width: 960
    height: 600
    minimumWidth: 640
    minimumHeight: 420
    visible: true
    color: Theme.background
    title: qsTr("Sound Recorder")

    onActiveChanged: Theme.windowActive = active

    readonly property bool maximized: visibility === Window.Maximized
    function toggleMaximized() { maximized ? showNormal() : showMaximized() }

    readonly property bool recording: Rec.state !== 0
    property string selectedPath: ""
    property var current: null // the chosen recording's map
    readonly property bool showRecordView: recording || selectedPath === ""
    readonly property real length: Player.duration > 0 ? Player.duration : (current ? current.seconds : 0)
    property bool trimming: false
    property real trimStart: 0
    property real trimEnd: 1
    property var live: [] // the meter's last values while recording

    Settings {
        id: settings
        property bool stereo: false
        property string device: ""
        property int volume: 100
    }
    Component.onCompleted: {
        Rec.stereo = settings.stereo
        Rec.device = settings.device
        Player.volume = settings.volume
        updateCurrent()
    }
    Connections {
        target: Rec
        function onStereoChanged() { settings.stereo = Rec.stereo }
        function onDeviceChanged() { settings.device = Rec.device }
        function onSaved(path) {
            Lib.refresh()
            root.select(path)
        }
        function onFailed(message) { root.showError(message) }
        function onTick() {
            if (Rec.state !== 1) return
            const next = root.live.length >= 240 ? root.live.slice(1) : root.live.slice()
            next.push(Rec.level)
            root.live = next
        }
        function onStateChanged() { if (Rec.state === 1 && root.live.length > 0 && Rec.elapsed < 100) root.live = [] }
    }
    Connections {
        target: Lib
        function onRecordingsChanged() { root.updateCurrent() }
        function onDone(path, error) {
            if (error !== "") {
                root.showError(error)
            } else {
                root.select(path, true)
            }
        }
    }
    Connections {
        target: Player
        function onVolumeChanged() { settings.volume = Player.volume }
    }

    function updateCurrent() {
        const list = Lib.recordings
        current = null
        for (let i = 0; i < list.length; i++) {
            if (list[i].path === selectedPath) current = list[i]
        }
        // The file went away (renamed elsewhere, deleted): back to the recorder.
        if (!current && selectedPath !== "") {
            selectedPath = ""
            Player.unload()
        }
    }
    function select(path, force) {
        if (path === selectedPath && !force) return
        trimming = false
        selectedPath = path
        updateCurrent()
        if (path === "") return
        Player.load(path)
        Lib.loadWaveform(path)
    }

    // --- numbers ---
    function clock(seconds) {
        const s = Math.max(0, Math.floor(seconds))
        const pad = n => String(n).padStart(2, "0")
        const h = Math.floor(s / 3600)
        return (h > 0 ? h + ":" + pad(Math.floor(s % 3600 / 60)) : Math.floor(s / 60)) + ":" + pad(s % 60)
    }
    function when(ms) { return new Date(ms).toLocaleString(Qt.locale(), Locale.ShortFormat) }

    function showError(message) {
        errorDialog.text = message
        errorDialog.open()
    }
    function togglePlay() {
        if (!Player.loaded) return
        // A finished recording starts again from its beginning.
        if (Player.paused && length > 0 && Player.position >= length - 0.05) Player.seek(0)
        Player.togglePause()
    }
    function recordingMenu(row, x, y) {
        select(row.path)
        menuLayer.open([
            { text: qsTr("&Rename"), icon: "document-edit", action: () => root.askRename() },
            { text: qsTr("&Delete"), icon: "edit-delete", action: () => deleteDialog.open() },
            { separator: true },
            { text: qsTr("Show in &folder"), icon: "folder-open-location", action: () => Lib.showInFolder(row.path) },
            { text: qsTr("&Copy path"), icon: "edit-copy", action: () => Lib.copyPath(row.path) },
            { text: qsTr("&Export as"), children: root.exportEntries(row.path) }
        ], x, y, {})
    }
    function exportEntries(path) {
        return [
            { text: "Ogg Vorbis (.ogg)", action: () => Lib.exportAs(path, "ogg") },
            { text: "AAC (.m4a)", action: () => Lib.exportAs(path, "m4a") }
        ]
    }
    function askRename() {
        renameDialog.text = ""
        nameInput.text = current ? current.base : ""
        renameDialog.open()
        nameInput.forceActiveFocus()
        nameInput.selectAll()
    }
    function deviceMenu(x, y) {
        const entries = [{ text: qsTr("Default"), checked: Rec.device === "", action: () => { Rec.device = "" } }]
        for (const d of Rec.devices) {
            entries.push({ text: d.label, checked: Rec.device === d.name, action: () => { Rec.device = d.name } })
        }
        menuLayer.open(entries, x, y, {})
    }
    readonly property string deviceLabel: {
        for (const d of Rec.devices) if (d.name === Rec.device) return d.label
        return qsTr("Default")
    }

    Shortcut { sequence: "Space"; enabled: !root.showRecordView && !renameDialog.visible; onActivated: root.togglePlay() }
    Shortcut {
        sequence: "Ctrl+R"
        onActivated: {
            if (Rec.state === 0) {
                root.select("")
                Rec.start()
            } else {
                Rec.stop()
            }
        }
    }
    Shortcut { sequence: "F2"; enabled: root.current !== null && !root.recording; onActivated: root.askRename() }
    Shortcut { sequence: "Delete"; enabled: root.current !== null && !root.recording; onActivated: deleteDialog.open() }

    // --- the title bar ---
    Item {
        id: strip
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 44

        DragHandler {
            target: null
            onActiveChanged: if (active) root.startSystemMove()
        }
        TapHandler { onDoubleTapped: root.toggleMaximized() }

        Image {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            source: "image://appicon/audio-input-microphone"
            sourceSize: Qt.size(width, height)
        }
        Text {
            x: 44
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.text
            opacity: root.active ? 1 : 0.5
            font.pixelSize: Theme.fontCaption
        }
        Row {
            anchors { right: parent.right; top: parent.top }
            WindowButton { glyph: "M 0 5.5 H 10"; onClicked: root.showMinimized() }
            WindowButton {
                glyph: root.maximized ? "M 0.5 2.5 H 7.5 V 9.5 H 0.5 Z M 2.5 2.5 V 0.5 H 9.5 V 7.5 H 7.5" : "M 0.5 0.5 H 9.5 V 9.5 H 0.5 Z"
                onClicked: root.toggleMaximized()
            }
            WindowButton { glyph: "M 0 0 L 10 10 M 10 0 L 0 10"; closes: true; onClicked: root.close() }
        }
    }

    // --- the recordings, at the left ---
    Rectangle {
        id: side
        anchors { left: parent.left; top: strip.bottom; bottom: parent.bottom; margins: 8; topMargin: 0 }
        width: Math.min(320, Math.max(220, root.width * 0.32))
        radius: Theme.radiusCard
        color: Theme.layer
        border.width: 1
        border.color: Theme.layerStroke

        Text {
            id: sideTitle
            x: 16
            y: 12
            text: qsTr("Recordings")
            color: Theme.text
            font.pixelSize: Theme.fontBodyLarge
            font.weight: Font.DemiBold
        }
        Text {
            anchors.centerIn: parent
            visible: Lib.recordings.length === 0
            width: parent.width - 48
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTr("No recordings yet. Press the record button to make one.")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontBody
        }
        ListView {
            id: list
            // The current entry's pill: it slides between entries.
            NavIndicator {
                id: navIndicator
                parent: list.contentItem
            }
            anchors { left: parent.left; right: parent.right; top: sideTitle.bottom; bottom: parent.bottom; margins: 6; topMargin: 10 }
            clip: true
            model: Lib.recordings
            ScrollBar {
                flickable: list
                anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
            }
            delegate: Rectangle {
                id: row
                required property var modelData
                readonly property bool chosen: modelData.path === root.selectedPath
                width: ListView.view.width - 8
                height: 56
                radius: Theme.radius
                color: chosen ? Theme.selection : rowMouse.containsMouse ? Theme.subtleHover : "transparent"
                onChosenChanged: navIndicator.track(row, chosen)
                Component.onCompleted: navIndicator.track(row, chosen)
                Column {
                    anchors { fill: parent; leftMargin: 14; rightMargin: 10 }
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    topPadding: 9
                    Text {
                        width: parent.width
                        text: row.modelData.base
                        elide: Text.ElideRight
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                    }
                    Text {
                        width: parent.width
                        text: root.when(row.modelData.modified) + (row.modelData.seconds > 0 ? "  ·  " + root.clock(row.modelData.seconds) : "")
                        elide: Text.ElideRight
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                    }
                }
                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: mouse => {
                        if (root.recording) return // the recorder keeps the right side
                        if (mouse.button === Qt.RightButton) {
                            const p = mapToItem(null, mouse.x, mouse.y)
                            root.recordingMenu(row.modelData, p.x, p.y)
                        } else {
                            root.select(row.modelData.path)
                        }
                    }
                }
            }
        }
    }

    // --- the right side: recorder or player ---
    Item {
        id: stage
        anchors { left: side.right; right: parent.right; top: strip.bottom; bottom: parent.bottom; margins: 8; topMargin: 0 }

        // The recorder: timer, the last seconds of level, the buttons.
        Item {
            anchors.fill: parent
            visible: root.showRecordView

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 48, 520)
                spacing: 20

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: root.clock(Rec.elapsed / 1000)
                    color: Theme.text
                    font.pixelSize: 56
                    font.weight: Font.Light
                    opacity: Rec.state === 2 ? 0.5 : 1
                }
                Waveform {
                    width: parent.width
                    height: 80
                    visible: root.recording
                    peaks: root.live
                    color: Theme.accentFill
                }
                Text {
                    width: parent.width
                    visible: !root.recording
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Press the button to start recording")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontBody
                }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 20
                    // Pause or resume
                    Rectangle {
                        visible: root.recording
                        anchors.verticalCenter: parent.verticalCenter
                        width: 48
                        height: 48
                        radius: 24
                        color: pauseMouse.containsMouse ? Theme.controlHover : Theme.control
                        border.width: 1
                        border.color: Theme.controlStroke
                        Row {
                            visible: Rec.state === 1
                            anchors.centerIn: parent
                            spacing: 4
                            Rectangle { width: 4; height: 16; radius: 1; color: Theme.text }
                            Rectangle { width: 4; height: 16; radius: 1; color: Theme.text }
                        }
                        Image {
                            visible: Rec.state === 2
                            anchors.centerIn: parent
                            width: 20
                            height: 20
                            source: Theme.icons + "media-playback-start"
                            sourceSize: Qt.size(20, 20)
                        }
                        MouseArea {
                            id: pauseMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: Rec.state === 1 ? Rec.pause() : Rec.resume()
                        }
                    }
                    // Record, or stop and keep
                    Rectangle {
                        width: 72
                        height: 72
                        radius: 36
                        color: "transparent"
                        border.width: 3
                        border.color: Theme.text
                        Rectangle {
                            anchors.centerIn: parent
                            width: root.recording ? 26 : 56
                            height: width
                            radius: root.recording ? 4 : width / 2
                            color: "#c42b1c" // the record red, as Windows' own
                            scale: recordMouse.pressed ? 0.9 : 1
                            Behavior on width { NumberAnimation { duration: Theme.fast } }
                            Behavior on radius { NumberAnimation { duration: Theme.fast } }
                        }
                        MouseArea {
                            id: recordMouse
                            anchors.fill: parent
                            onClicked: root.recording ? Rec.stop() : Rec.start()
                        }
                    }
                    // Throw the take away
                    ToolButton {
                        visible: root.recording
                        anchors.verticalCenter: parent.verticalCenter
                        icon.name: "edit-delete"
                        tooltip: qsTr("Discard")
                        onClicked: Rec.discard()
                    }
                }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 8
                    visible: !root.recording
                    ToolButton {
                        text: Rec.stereo ? qsTr("Stereo") : qsTr("Mono")
                        menu: true
                        onClicked: {
                            const p = mapToItem(null, 0, height + 4)
                            menuLayer.open([
                                { text: qsTr("Mono"), checked: !Rec.stereo, action: () => { Rec.stereo = false } },
                                { text: qsTr("Stereo"), checked: Rec.stereo, action: () => { Rec.stereo = true } }
                            ], p.x, p.y, {})
                        }
                    }
                    ToolButton {
                        text: root.deviceLabel
                        menu: true
                        onClicked: {
                            Rec.refreshDevices()
                            const p = mapToItem(null, 0, height + 4)
                            root.deviceMenu(p.x, p.y)
                        }
                    }
                }
            }
        }

        // The player of the chosen recording.
        Item {
            anchors.fill: parent
            visible: !root.showRecordView

            Column {
                id: header
                x: 16
                y: 8
                width: parent.width - 32
                spacing: 4
                Text {
                    width: parent.width
                    text: root.current ? root.current.base : ""
                    elide: Text.ElideRight
                    color: Theme.text
                    font.pixelSize: Theme.fontSubtitle
                    font.weight: Font.DemiBold
                }
                Text {
                    text: root.current ? root.when(root.current.modified) : ""
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                }
            }

            // The waveform is the seek track; in trim mode two handles mark the part kept.
            Item {
                id: track
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 16 }
                height: 140
                readonly property real played: root.length > 0 ? Math.min(1, Player.position / root.length) : 0
                readonly property bool ready: Lib.waveformPath === root.selectedPath && Lib.waveform.length > 0

                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radiusCard
                    color: Theme.layer
                    border.width: 1
                    border.color: Theme.layerStroke
                }
                Text {
                    anchors.centerIn: parent
                    visible: !track.ready
                    text: Lib.waveformPath === root.selectedPath && root.selectedPath !== "" && Lib.waveform.length === 0 ? "" : qsTr("Waveform is shown for WAV recordings")
                    color: Theme.textTertiary
                    font.pixelSize: Theme.fontCaption
                }
                Item {
                    id: bars
                    anchors { fill: parent; leftMargin: 8; rightMargin: 8; topMargin: 8; bottomMargin: 8 }
                    visible: track.ready
                    Waveform {
                        anchors.fill: parent
                        peaks: Lib.waveform
                        color: Theme.controlStrokeStrong
                    }
                    Item {
                        width: parent.width * track.played
                        height: parent.height
                        clip: true
                        Waveform {
                            width: bars.width
                            height: bars.height
                            peaks: Lib.waveform
                            color: Theme.accentFill
                        }
                    }
                    // The playhead
                    Rectangle {
                        x: bars.width * track.played - 1
                        width: 2
                        height: parent.height
                        color: Theme.accentFill
                    }
                    // What trimming drops is dimmed.
                    Rectangle {
                        visible: root.trimming
                        width: bars.width * root.trimStart
                        height: parent.height
                        color: Qt.rgba(0, 0, 0, 0.35)
                    }
                    Rectangle {
                        visible: root.trimming
                        x: bars.width * root.trimEnd
                        width: bars.width * (1 - root.trimEnd)
                        height: parent.height
                        color: Qt.rgba(0, 0, 0, 0.35)
                    }
                }
                // Click or drag to seek.
                MouseArea {
                    anchors.fill: bars
                    enabled: root.length > 0
                    function seekTo(x) { Player.seek(Math.max(0, Math.min(1, x / width)) * root.length) }
                    onPressed: mouse => seekTo(mouse.x)
                    onPositionChanged: mouse => { if (pressed) seekTo(mouse.x) }
                }
                Repeater {
                    model: root.trimming ? 2 : 0
                    delegate: Rectangle {
                        id: handle
                        required property int index
                        readonly property bool isStart: index === 0
                        x: bars.x + bars.width * (isStart ? root.trimStart : root.trimEnd) - width / 2
                        y: 0
                        width: 10
                        height: track.height
                        radius: 3
                        color: Theme.accentFill
                        opacity: handleMouse.containsMouse || handleMouse.pressed ? 1 : 0.8
                        MouseArea {
                            id: handleMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.SizeHorCursor
                            onPositionChanged: mouse => {
                                if (!pressed) return
                                const f = Math.max(0, Math.min(1, (handle.x + mouse.x) / bars.width - bars.x / bars.width))
                                // Never closer than a tenth of a second.
                                const gap = root.length > 0 ? 0.1 / root.length : 0.01
                                if (handle.isStart) root.trimStart = Math.min(f, root.trimEnd - gap)
                                else root.trimEnd = Math.max(f, root.trimStart + gap)
                            }
                        }
                    }
                }
            }

            // Position, length
            Row {
                id: times
                anchors { left: track.left; top: track.bottom; topMargin: 8 }
                Text { text: root.clock(Player.position); color: Theme.text; font.pixelSize: Theme.fontBody }
                Text { text: " / " + root.clock(root.length); color: Theme.textSecondary; font.pixelSize: Theme.fontBody }
            }
            Text {
                visible: root.trimming
                anchors { right: track.right; top: track.bottom; topMargin: 8 }
                text: qsTr("Keeping %1").arg(root.clock((root.trimEnd - root.trimStart) * root.length))
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
            }

            // Play, volume
            Row {
                id: controls
                anchors { horizontalCenter: parent.horizontalCenter; top: times.bottom; topMargin: 20 }
                spacing: 16
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 48
                    height: 48
                    radius: 24
                    color: playMouse.containsMouse ? Theme.accentFillHover : Theme.accentFill
                    Image {
                        anchors.centerIn: parent
                        width: 20
                        height: 20
                        source: Theme.iconsOnAccent + (Player.paused ? "media-playback-start" : "media-playback-pause")
                        sourceSize: Qt.size(20, 20)
                    }
                    MouseArea { id: playMouse; anchors.fill: parent; hoverEnabled: true; onClicked: root.togglePlay() }
                }
                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16
                    height: 16
                    source: Theme.icons + (Player.volume === 0 ? "audio-volume-muted" : "audio-volume-high")
                    sourceSize: Qt.size(16, 16)
                }
                Slider {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 140
                    from: 0
                    to: 100
                    value: Player.volume
                    onMoved: v => Player.volume = Math.round(v)
                }
            }

            // What can be done with it
            Row {
                anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 12 }
                spacing: 4
                visible: !root.trimming
                ToolButton {
                    icon.name: "edit-cut"
                    text: qsTr("Trim")
                    usable: track.ready && root.length > 0 && !Lib.busy
                    onClicked: {
                        root.trimStart = 0
                        root.trimEnd = 1
                        root.trimming = true
                    }
                }
                ToolButton { icon.name: "document-edit"; text: qsTr("Rename"); onClicked: root.askRename() }
                ToolButton {
                    icon.name: "document-save-as"
                    text: qsTr("Export")
                    menu: true
                    usable: !Lib.busy
                    onClicked: {
                        const p = mapToItem(null, 0, -4)
                        menuLayer.open(root.exportEntries(root.selectedPath), p.x, p.y, { above: true })
                    }
                }
                ToolButton { icon.name: "folder-open-location"; text: qsTr("Show in folder"); onClicked: Lib.showInFolder(root.selectedPath) }
                ToolButton { icon.name: "edit-copy"; text: qsTr("Copy path"); onClicked: Lib.copyPath(root.selectedPath) }
                ToolButton { icon.name: "edit-delete"; text: qsTr("Delete"); onClicked: deleteDialog.open() }
            }
            Row {
                anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 12 }
                spacing: 4
                visible: root.trimming
                ToolButton {
                    icon.name: "document-save"
                    text: qsTr("Save a copy")
                    usable: !Lib.busy
                    onClicked: {
                        root.trimming = false
                        Lib.trim(root.selectedPath, root.trimStart * root.length, root.trimEnd * root.length, false)
                    }
                }
                ToolButton { icon.name: "document-save-all"; text: qsTr("Replace original"); usable: !Lib.busy; onClicked: trimDialog.open() }
                ToolButton { icon.name: "tab-close"; text: qsTr("Cancel"); onClicked: root.trimming = false }
            }
            // A new recording button, back to the recorder
            ToolButton {
                anchors { right: parent.right; top: parent.top; margins: 8 }
                icon.name: "audio-input-microphone"
                text: qsTr("Record")
                onClicked: root.select("")
            }
        }
    }

    MenuLayer {
        id: menuLayer
        anchors.fill: parent
        z: 100
    }

    // --- dialogs ---
    Dialog {
        id: errorDialog
        title: qsTr("Sound Recorder")
    }
    Dialog {
        id: deleteDialog
        title: qsTr("Delete this recording?")
        text: root.current ? qsTr("\"%1\" will be moved to the trash.").arg(root.current.base) : ""
        buttons: [{ text: qsTr("Delete"), accent: true, value: "ok" }, { text: qsTr("Cancel"), value: "cancel" }]
        onChosen: value => {
            if (value !== "ok" || !root.current) return
            Player.unload()
            if (!Lib.remove(root.selectedPath)) root.showError(qsTr("The recording could not be moved to the trash."))
            root.select("")
        }
    }
    Dialog {
        id: trimDialog
        title: qsTr("Replace the original?")
        text: qsTr("The recording is cut to the part you chose. The parts outside it are lost.")
        buttons: [{ text: qsTr("Replace"), accent: true, value: "ok" }, { text: qsTr("Cancel"), value: "cancel" }]
        onChosen: value => {
            if (value !== "ok") return
            root.trimming = false
            Player.unload() // mpv has the file open
            Lib.trim(root.selectedPath, root.trimStart * root.length, root.trimEnd * root.length, true)
        }
    }
    Dialog {
        id: renameDialog
        title: qsTr("Rename")
        buttons: [{ text: qsTr("Rename"), accent: true, value: "ok" }, { text: qsTr("Cancel"), value: "cancel" }]
        // Stays open on a name that is refused.
        onChosen: value => {
            if (value !== "ok" || !root.current) return
            const error = Lib.rename(root.selectedPath, nameInput.text)
            if (error !== "") {
                text = error
                open()
                nameInput.forceActiveFocus()
                return
            }
            const base = nameInput.text.trim()
            const old = root.current.path
            const dot = old.lastIndexOf(".")
            const slash = old.lastIndexOf("/")
            const renamed = old.substring(0, slash + 1) + base + old.substring(dot)
            root.selectedPath = renamed
            root.updateCurrent()
            Player.load(renamed)
            Lib.loadWaveform(renamed)
        }
        Rectangle {
            width: parent.width
            height: 32
            radius: Theme.radius
            color: Theme.control
            border.width: 1
            border.color: nameInput.activeFocus ? Theme.accentFill : Theme.controlStroke
            TextInput {
                id: nameInput
                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                verticalAlignment: TextInput.AlignVCenter
                clip: true
                color: Theme.text
                selectionColor: Theme.accent
                selectedTextColor: "white"
                font.pixelSize: Theme.fontBody
                Keys.onReturnPressed: renameDialog.close("ok")
                Keys.onEnterPressed: renameDialog.close("ok")
                Keys.onEscapePressed: renameDialog.close("cancel")
            }
        }
    }

    ResizeEdges {}
}
