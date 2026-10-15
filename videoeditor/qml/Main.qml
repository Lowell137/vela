// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtCore
import Vela.Controls
import Vela.VideoEditor.Backend

// The Video Editor's window, like Windows 11's (Clipchamp's basics): its own
// title bar and command bar; the project's media at the left, the preview in
// the middle, the selected clip's settings at the right and the timeline
// below. The project's life (new, open, save, autosave, the questions on
// closing) lives here; the timeline itself is Project (timeline.h).
Window {
    id: root
    flags: Qt.Window | Qt.FramelessWindowHint // no title bar from Vela
    width: 1280
    height: 820
    minimumWidth: 960
    minimumHeight: 620
    visible: true
    color: Theme.background
    title: (Project.modified ? "● " : "") + projectName + qsTr(" - Video Editor")

    onActiveChanged: Theme.windowActive = active

    readonly property bool maximized: visibility === Window.Maximized
    function toggleMaximized() { maximized ? showNormal() : showMaximized() }

    // --- preferences ---
    Settings {
        id: settings
        property string exportFolder: ""
        property string quality: "high"
        property bool hardware: true
        property string recent: "[]" // the last projects' paths, as JSON
        property real zoom: 40
    }
    readonly property string exportFolder: settings.exportFolder !== "" ? settings.exportFolder : localPath(StandardPaths.writableLocation(StandardPaths.MoviesLocation))
    function setExportFolder(path) { settings.exportFolder = path }
    property alias defaultQuality: settings.quality
    property alias hardwareEncoder: settings.hardware

    // --- small helpers ---
    function localPath(url) { return decodeURIComponent(url.toString().replace(/^file:\/\//, "")) }
    function fileName(path) { return path.split("/").pop() }
    function clock(seconds) {
        const s = Math.max(0, seconds)
        const m = Math.floor(s / 60)
        return m + ":" + (s % 60).toFixed(1).padStart(4, "0")
    }
    function duration(seconds) { // the time left
        const s = Math.max(1, Math.round(seconds))
        return s < 60 ? qsTr("%1 s").arg(s) : qsTr("%1 min %2 s").arg(Math.floor(s / 60)).arg(s % 60)
    }
    function showError(message) {
        errorDialog.text = message
        errorDialog.open()
    }

    // --- the project ---
    property string projectPath: ""
    readonly property string projectName: projectPath !== "" ? fileName(projectPath).replace(/\.velaedit$/, "") : qsTr("Untitled")
    readonly property string autosavePath: projectPath !== "" ? projectPath + ".autosave" : Media.cache + "/untitled.velaedit.autosave"
    property var recent: JSON.parse(settings.recent)
    function remember(path) {
        recent = [path].concat(recent.filter(p => p !== path)).slice(0, 8)
        settings.recent = JSON.stringify(recent)
    }

    // Asks to save first when there are changes; `then()` unless cancelled.
    function confirmDiscard(then) {
        if (!Project.modified) {
            then()
            return
        }
        saveDialog.then = then
        saveDialog.text = qsTr("Do you want to save the changes to %1?").arg(projectName)
        saveDialog.open()
    }
    function forget() { // the unsaved changes are let go
        Media.removeFile(autosavePath)
    }
    function newProject() {
        confirmDiscard(() => {
            forget()
            projectPath = ""
            Project.clear()
            select(-1, -1)
            seek(0)
        })
    }
    function openProject(path) {
        const error = Project.load(path)
        if (error !== "") {
            showError(qsTr("%1 can't be opened: %2").arg(fileName(path)).arg(error))
            return
        }
        projectPath = path
        remember(path)
        select(-1, -1)
        seek(0)
        if (Media.exists(autosavePath) && Media.newer(autosavePath, path)) {
            restoreDialog.open()
        } else {
            checkMissing()
        }
    }
    function checkMissing() {
        if (Project.missing().length > 0) relinkDialog.open()
    }
    function chooseProject() {
        confirmDiscard(() => pick(false, projectPath !== "" ? "file://" + projectPath.replace(/\/[^/]*$/, "") : "",
            "", [qsTr("Video Editor projects (*.velaedit)")], files => {
                forget()
                openProject(localPath(files[0]))
            }))
    }
    function save(then) {
        if (projectPath === "") {
            saveAs(then)
            return
        }
        const error = Project.save(projectPath)
        if (error !== "") {
            showError(qsTr("%1 can't be saved: %2").arg(fileName(projectPath)).arg(error))
            return
        }
        forget()
        if (then) then()
    }
    function saveAs(then) {
        pickSave(projectPath !== "" ? projectPath.replace(/\/[^/]*$/, "") : localPath(StandardPaths.writableLocation(StandardPaths.MoviesLocation)),
            projectName + ".velaedit", [qsTr("Video Editor projects (*.velaedit)")], file => {
                let path = localPath(file)
                if (!path.endsWith(".velaedit")) path += ".velaedit"
                const old = autosavePath
                projectPath = path
                Media.removeFile(old)
                remember(path)
                save(then)
            })
    }
    // Every 30 seconds the changes are kept next to the project.
    Timer {
        interval: 30000
        repeat: true
        running: Project.modified
        onTriggered: Project.saveCopy(root.autosavePath)
    }
    property bool closing: false
    onClosing: close => {
        if (closing) return
        close.accepted = false
        confirmDiscard(() => {
            Exporter.cancel()
            forget()
            closing = true
            root.close()
        })
    }
    Component.onCompleted: {
        if (StartFile !== "") openProject(StartFile)
        else if (Media.exists(autosavePath)) restoreDialog.open()
    }

    Connections {
        target: Project
        function onMediaChanged() { Media.setFiles(Project.media) }
        function onChanged() {
            const list = root.selectedList()
            if (root.selIndex >= list.length) root.select(-1, -1)
            root.reload()
        }
    }
    Connections {
        target: Media
        function onItemsChanged() { if (Media.pending === 0) Project.media = Media.paths() }
        function onPlaced(item, at) { Project.insertMedia(item, at) }
        function onStillReady() {
            root.playedEdl = "" // a picture's still is there now
            root.reload()
        }
        function onFailed(name) { root.showError(qsTr("%1 can't be used: it isn't a video, a picture or a sound that ffmpeg knows.").arg(name)) }
    }

    // --- files ---
    property var picker: null
    function ensurePicker() {
        if (!picker) {
            const component = Qt.createComponent("FilePicker.qml")
            picker = component.createObject(root, { parentWindow: root })
            if (!picker) showError(component.errorString())
        }
        return picker
    }
    function pick(multiple, folder, name, filters, done) {
        if (!ensurePicker()) return
        picker.nameFilters = filters.concat([qsTr("All files (*)")])
        picker.pick(false, folder, name, files => { if (files.length > 0) done(multiple ? files : [files[0]]) })
    }
    function pickSave(folder, name, filters, done) {
        if (!ensurePicker()) return
        picker.nameFilters = filters
        picker.pick(true, "file://" + folder, name, files => { if (files.length > 0) done(files[0]) })
    }
    function addMedia() {
        pick(true, StandardPaths.writableLocation(StandardPaths.MoviesLocation), "",
            [qsTr("Videos, pictures and sounds (*.mp4 *.mkv *.webm *.mov *.avi *.m4v *.mpg *.ts *.png *.jpg *.jpeg *.webp *.bmp *.gif *.mp3 *.wav *.flac *.ogg *.opus *.m4a *.aac)")],
            files => Media.add(files))
    }

    // --- selection and edits ---
    property int selTrack: -1
    property int selIndex: -1
    function selectedList() { return selTrack === 0 ? Project.video : selTrack === 1 ? Project.audio : selTrack === 2 ? Project.texts : [] }
    readonly property var selected: { Project.video; return selectedList()[selIndex] || null }
    function select(track, index) {
        selTrack = track
        selIndex = index
    }
    function split() {
        if (selected && selected.kind !== "gap" && Project.split(selTrack, selIndex, playhead)) return
        Project.split(0, Project.clipAt(playhead).index, playhead)
    }
    function removeSelected(ripple) {
        if (!selected) return
        Project.remove(selTrack, selIndex, ripple)
        select(-1, -1)
    }
    function addText() {
        const at = playhead
        Project.addText(at, qsTr("Your text"))
        const texts = Project.texts
        for (let i = texts.length - 1; i >= 0; i--) {
            if (Math.abs(texts[i].start - at) < 0.001) {
                select(2, i)
                return
            }
        }
    }

    // --- the preview ---
    property real playhead: 0
    readonly property bool loaded: !player.info.idle
    readonly property bool playing: loaded && !player.info.paused
    // Right after a load or a seek mpv still reports the old position.
    property bool settling: false
    Timer { id: settle; interval: 400; onTriggered: root.settling = false }
    function hold() {
        settling = true
        settle.restart()
    }
    function reload() { reloadTimer.restart() }
    Timer { id: reloadTimer; interval: 120; onTriggered: root.loadPreview() }
    // What the players have: an edit that doesn't change it (a fade, a save)
    // doesn't reload them.
    property string playedEdl
    property string playedMusic
    function loadPreview() {
        if (!player.ready) return
        playhead = Math.min(playhead, Project.length)
        const edl = Project.edl(Media.cache)
        if (edl !== playedEdl) {
            hold()
            playedEdl = edl
            if (edl === "") player.command(["stop"])
            else player.command(["loadfile", edl, "replace", "-1", "start=" + Project.toEdl(playhead)])
        }
        const music = Project.musicEdl(Media.cache)
        if (music !== playedMusic) {
            playedMusic = music
            if (music === "") musicPlayer.command(["stop"])
            else musicPlayer.command(["loadfile", music, "replace", "-1", "start=" + playhead])
        }
        applyClip()
    }
    function seek(at) {
        playhead = Math.max(0, Math.min(at, Project.length))
        hold()
        if (loaded) player.command(["seek", String(Project.toEdl(playhead)), "absolute+exact"])
        musicPlayer.command(["seek", String(playhead), "absolute+exact"])
    }
    function togglePlay() {
        if (!loaded) return
        const pause = playing
        if (!pause && playhead >= Project.videoLength - 0.05) seek(0)
        player.set("pause", pause ? "yes" : "no")
        musicPlayer.set("pause", pause ? "yes" : "no")
    }
    function step(forward) {
        musicPlayer.set("pause", "yes")
        player.command([forward ? "frame-step" : "frame-back-step"])
    }
    readonly property real mpvPosition: player.info.position
    onMpvPositionChanged: {
        if (!settling && loaded) {
            playhead = Project.fromEdl(mpvPosition)
            if (playing) timeline.follow()
        }
    }
    // The music follows the picture.
    Timer {
        interval: 500
        repeat: true
        running: root.playing
        onTriggered: {
            if (Math.abs(musicPlayer.info.position - root.playhead) > 0.2 && root.playhead < Project.length)
                musicPlayer.command(["seek", String(root.playhead), "absolute+exact"])
            musicPlayer.set("pause", "no")
        }
    }
    onPlayingChanged: if (!playing) musicPlayer.set("pause", "yes")

    // The clip under the playhead: its speed, volume, turn and crop.
    readonly property var current: { Project.video; const i = Project.clipAt(playhead).index; return i >= 0 ? Project.video[i] : null }
    readonly property real currentOffset: Project.video.length >= 0 ? Project.clipAt(playhead).offset : 0
    onCurrentChanged: applyClip()
    function applyClip() {
        const c = current
        player.set("speed", c ? String(c.speed) : "1")
        player.set("volume", c ? String(c.muted ? 0 : c.volume * 100) : "100")
        player.set("video-rotate", c ? String(c.rotate) : "0")
        player.set("panscan", c && c.crop !== "" ? "1" : "0")
    }
    readonly property int musicIndex: {
        const list = Project.audio
        for (let i = list.length - 1; i >= 0; i--) {
            if (playhead >= list[i].start && playhead < list[i].start + list[i].length) return i
        }
        return -1
    }
    readonly property real musicVolume: musicIndex >= 0 ? (Project.audio[musicIndex].muted ? 0 : Project.audio[musicIndex].volume * 100) : 100
    onMusicVolumeChanged: musicPlayer.set("volume", String(musicVolume))

    // The picture's shape once turned and cropped, as the export makes it.
    function cropRatio(crop) {
        const parts = crop.split(":")
        return parts.length === 2 ? parts[0] / parts[1] : 0
    }
    function aspect(c) {
        let w = c.width, h = c.height
        if (c.rotate % 180 === 90) [w, h] = [h, w]
        const r = cropRatio(c.crop)
        return r > 0 ? r : h > 0 ? w / h : 16 / 9
    }
    readonly property real canvasAspect: {
        const c = Project.video.find(v => v.width > 0 && v.height > 0)
        return c ? aspect(c) : 16 / 9
    }

    // --- menus ---
    function moreEntries() {
        return [
            { text: qsTr("&New project"), shortcut: "Ctrl+N", icon: "document-new", action: () => root.newProject() },
            { text: qsTr("&Open project..."), shortcut: "Ctrl+O", icon: "document-open", action: () => root.chooseProject() },
            { text: qsTr("Open &recent"), enabled: recent.length > 0, children: recent.map(p => ({
                text: root.fileName(p), action: () => root.confirmDiscard(() => { root.forget(); root.openProject(p) })
            })) },
            { separator: true },
            { text: qsTr("&Save"), shortcut: "Ctrl+S", icon: "document-save", action: () => root.save() },
            { text: qsTr("Save &as..."), shortcut: "Ctrl+Shift+S", icon: "document-save-as", action: () => root.saveAs() },
            { separator: true },
            { text: qsTr("&Export..."), shortcut: "Ctrl+E", enabled: Project.video.length > 0, action: () => exportDialog.start() },
            { text: qsTr("Se&ttings"), icon: "configure", action: () => root.showSettings = true },
            { text: qsTr("&Keyboard shortcuts"), shortcut: "F1", action: () => shortcutsDialog.open() }
        ]
    }
    function clipEntries() {
        return [
            { text: qsTr("Split at the playhead"), shortcut: "S", icon: "edit-cut", enabled: !!selected && selected.kind !== "gap", action: () => root.split() },
            { text: qsTr("Delete"), shortcut: "Del", icon: "edit-delete", action: () => root.removeSelected(true) },
            { text: qsTr("Delete and leave a gap"), shortcut: "Shift+Del", enabled: selTrack === 0, action: () => root.removeSelected(false) },
            { separator: true },
            { text: qsTr("Detach the audio"), icon: "audio-x-generic", enabled: selTrack === 0 && !!selected && selected.kind === "video" && selected.hasAudio && !selected.muted,
              action: () => Project.detachAudio(selIndex) }
        ]
    }
    function openClipMenu(x, y) { menuLayer.open(clipEntries(), x, y) }
    function openMenu(button, entries) {
        const p = button.mapToItem(null, 0, button.height)
        menuLayer.open(entries, p.x, p.y + 4)
    }

    Shortcut { sequence: "Ctrl+N"; onActivated: root.newProject() }
    Shortcut { sequence: "Ctrl+O"; onActivated: root.chooseProject() }
    Shortcut { sequence: "Ctrl+S"; onActivated: root.save() }
    Shortcut { sequence: "Ctrl+Shift+S"; onActivated: root.saveAs() }
    Shortcut { sequence: "Ctrl+I"; onActivated: root.addMedia() }
    Shortcut { sequence: "Ctrl+E"; enabled: Project.video.length > 0; onActivated: exportDialog.start() }
    Shortcut { sequence: "Ctrl+Z"; onActivated: Project.undo() }
    Shortcut { sequences: ["Ctrl+Shift+Z", "Ctrl+Y"]; onActivated: Project.redo() }
    Shortcut { sequence: "Space"; onActivated: root.togglePlay() }
    Shortcut { sequence: "S"; onActivated: root.split() }
    Shortcut { sequence: "T"; onActivated: root.addText() }
    Shortcut { sequence: "Delete"; onActivated: root.removeSelected(true) }
    Shortcut { sequence: "Shift+Delete"; onActivated: root.removeSelected(false) }
    Shortcut { sequence: ","; onActivated: root.step(false) }
    Shortcut { sequence: "."; onActivated: root.step(true) }
    Shortcut { sequence: "Home"; onActivated: root.seek(0) }
    Shortcut { sequence: "End"; onActivated: root.seek(Project.videoLength) }
    Shortcut { sequences: ["Ctrl+=", "Ctrl++"]; onActivated: timeline.zoomTo(timeline.pps * 1.25, timeline.width / 2) }
    Shortcut { sequence: "Ctrl+-"; onActivated: timeline.zoomTo(timeline.pps * 0.8, timeline.width / 2) }
    Shortcut { sequence: "Escape"; onActivated: root.select(-1, -1) }
    Shortcut { sequence: "F1"; onActivated: shortcutsDialog.open() }

    // --- the title bar ---
    Item {
        id: strip
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 32
        DragHandler {
            target: null
            onActiveChanged: if (active) root.startSystemMove()
        }
        TapHandler { onDoubleTapped: root.toggleMaximized() }
        Image {
            x: 14
            y: 8
            width: 16
            height: 16
            source: "image://appicon/kdenlive"
            sourceSize: Qt.size(width, height)
        }
        Text {
            x: 42
            width: parent.width - x - 3 * 46 - 8
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.text
            opacity: root.active ? 1 : 0.5
            font.pixelSize: Theme.fontCaption
            elide: Text.ElideMiddle
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

    // --- the command bar ---
    Item {
        id: commands
        anchors { left: parent.left; right: parent.right; top: strip.bottom; leftMargin: 8; rightMargin: 8 }
        height: 44
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            ToolButton { icon: "list-add"; text: qsTr("Add media"); tooltip: qsTr("Add videos, pictures and sounds (Ctrl+I)"); onClicked: root.addMedia() }
            ToolButton { icon: "draw-text"; text: qsTr("Text"); tooltip: qsTr("Add a text at the playhead (T)"); onClicked: root.addText() }
            Rectangle { width: 1; height: 20; anchors.verticalCenter: parent.verticalCenter; color: Theme.divider }
            ToolButton { icon: "edit-undo"; tooltip: qsTr("Undo (Ctrl+Z)"); usable: Project.canUndo; onClicked: Project.undo() }
            ToolButton { icon: "edit-redo"; tooltip: qsTr("Redo (Ctrl+Shift+Z)"); usable: Project.canRedo; onClicked: Project.redo() }
            Rectangle { width: 1; height: 20; anchors.verticalCenter: parent.verticalCenter; color: Theme.divider }
            ToolButton { icon: "edit-cut"; text: qsTr("Split"); tooltip: qsTr("Split at the playhead (S)"); usable: Project.video.length > 0 || Project.texts.length > 0; onClicked: root.split() }
            ToolButton { icon: "edit-delete"; tooltip: qsTr("Delete (Del)"); usable: !!root.selected; onClicked: root.removeSelected(true) }
        }
        Row {
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            spacing: 4
            ToolButton { icon: "document-save"; tooltip: qsTr("Save (Ctrl+S)"); onClicked: root.save() }
            ToolButton { icon: "configure"; tooltip: qsTr("Settings"); onClicked: root.showSettings = true }
            ToolButton { id: moreButton; text: "⋯"; tooltip: qsTr("More"); onClicked: root.openMenu(moreButton, root.moreEntries()) }
            Button {
                accent: true
                text: qsTr("Export")
                usable: Project.video.length > 0
                onClicked: exportDialog.start()
            }
        }
    }

    // --- the media ---
    Rectangle {
        id: pool
        anchors { left: parent.left; top: commands.bottom; bottom: timelineFrame.top }
        width: 248
        color: Theme.background
        Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.divider }

        Text {
            id: poolTitle
            x: 16
            height: 36
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Media")
            color: Theme.text
            font.pixelSize: Theme.fontBody
            font.weight: Font.DemiBold
        }
        ToolButton {
            anchors { right: parent.right; rightMargin: 8; verticalCenter: poolTitle.verticalCenter }
            icon: "list-add"
            tooltip: qsTr("Add media (Ctrl+I)")
            onClicked: root.addMedia()
        }

        Column {
            visible: Media.items.length === 0
            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 20 }
            spacing: 12
            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 48
                height: 48
                source: Theme.icons + "video-x-generic"
                sourceSize: Qt.size(48, 48)
                opacity: 0.6
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Add videos, pictures and music, or drop them here")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                wrapMode: Text.Wrap
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Add media")
                onClicked: root.addMedia()
            }
        }

        GridView {
            id: grid
            anchors { left: parent.left; right: parent.right; top: poolTitle.bottom; bottom: parent.bottom; leftMargin: 8; rightMargin: 4 }
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            cellWidth: Math.floor(width / 2)
            cellHeight: 116
            model: Media.items
            delegate: Item {
                id: tile
                required property var modelData
                width: grid.cellWidth
                height: grid.cellHeight
                readonly property bool usable: !modelData.loading && !modelData.missing
                Rectangle {
                    anchors { fill: parent; margins: 4 }
                    radius: Theme.radiusCard
                    color: tileMouse.containsMouse ? Theme.cardHover : Theme.card
                    border.width: 1
                    border.color: Theme.cardStroke
                    Rectangle {
                        id: thumbBox
                        x: 4
                        y: 4
                        width: parent.width - 8
                        height: 64
                        radius: Theme.radius
                        color: Qt.rgba(0, 0, 0, 0.25)
                        clip: true
                        Image {
                            anchors.fill: parent
                            visible: !!tile.modelData.thumb
                            source: tile.modelData.thumb ?? ""
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                        }
                        Image {
                            anchors.centerIn: parent
                            visible: !tile.modelData.thumb
                            width: 28
                            height: 28
                            source: Theme.icons + (tile.modelData.kind === "audio" ? "audio-x-generic" : tile.modelData.missing ? "dialog-warning" : "video-x-generic")
                            sourceSize: Qt.size(28, 28)
                            opacity: tile.modelData.loading ? 0.3 : 0.8
                        }
                        Rectangle {
                            visible: tile.modelData.kind === "video" || tile.modelData.kind === "audio"
                            anchors { right: parent.right; bottom: parent.bottom; margins: 4 }
                            width: durationText.implicitWidth + 8
                            height: 16
                            radius: 2
                            color: Qt.rgba(0, 0, 0, 0.6)
                            Text {
                                id: durationText
                                anchors.centerIn: parent
                                text: root.clock(tile.modelData.duration || 0)
                                color: "white"
                                font.pixelSize: 10
                            }
                        }
                    }
                    Text {
                        anchors { left: parent.left; right: parent.right; top: thumbBox.bottom; margins: 6 }
                        text: tile.modelData.missing ? qsTr("Missing: %1").arg(tile.modelData.name) : tile.modelData.name
                        color: tile.modelData.missing ? Theme.critical : Theme.text
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideMiddle
                    }
                }
                // A click adds it at the end; dragged, it goes where it's dropped.
                MouseArea {
                    id: tileMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    property point from
                    property bool dragging: false
                    onPressed: mouse => {
                        from = Qt.point(mouse.x, mouse.y)
                        if (mouse.button === Qt.RightButton) {
                            const p = mapToItem(null, mouse.x, mouse.y)
                            menuLayer.open([
                                { text: qsTr("Add to the timeline"), enabled: tile.usable, action: () => Project.insertMedia(tile.modelData) },
                                { text: qsTr("Remove from the project"), icon: "edit-delete", action: () => Media.remove(tile.modelData.path) }
                            ], p.x, p.y)
                        }
                    }
                    onPositionChanged: mouse => {
                        if (!pressed || !tile.usable || !(pressedButtons & Qt.LeftButton)) return
                        const p = mapToItem(null, mouse.x, mouse.y)
                        if (!dragging && Math.hypot(mouse.x - from.x, mouse.y - from.y) < 6) return
                        dragging = true
                        ghost.text = tile.modelData.name
                        ghost.x = p.x - 10
                        ghost.y = p.y - 10
                        ghost.visible = true
                    }
                    onReleased: mouse => {
                        ghost.visible = false
                        if (!dragging) return
                        dragging = false
                        const p = mapToItem(null, mouse.x, mouse.y)
                        if (timeline.overTracks(p.x, p.y)) Project.insertMedia(tile.modelData, timeline.timeAt(p.x))
                    }
                    onDoubleClicked: if (tile.usable) Project.insertMedia(tile.modelData)
                }
            }
        }
    }

    // --- the preview ---
    Rectangle {
        id: stage
        anchors { left: pool.right; right: inspector.left; top: commands.bottom; bottom: transport.top }
        color: "black"
        clip: true

        // The export's frame, and the clip's picture inside it.
        Item {
            id: canvas
            anchors.centerIn: parent
            readonly property real fitWidth: Math.min(stage.width, stage.height * root.canvasAspect)
            width: fitWidth
            height: fitWidth / root.canvasAspect

            // Always there: mpv needs it to draw.
            MpvPlayer {
                id: player
                anchors.centerIn: parent
                readonly property real clipAspect: root.current && root.current.crop !== "" ? root.cropRatio(root.current.crop) : 0
                width: clipAspect > 0 ? Math.min(parent.width, parent.height * clipAspect) : parent.width
                height: clipAspect > 0 ? width / clipAspect : parent.height
                transform: Scale {
                    origin.x: player.width / 2
                    origin.y: player.height / 2
                    xScale: root.current && root.current.flipH ? -1 : 1
                    yScale: root.current && root.current.flipV ? -1 : 1
                }
                onReadyChanged: {
                    player.set("keep-open", "yes")
                    player.set("pause", "yes")
                    player.set("volume-max", "200")
                    player.set("audio-client-name", "Video Editor")
                    root.loadPreview()
                }
                onFailed: message => root.showError(qsTr("The preview can't play: %1").arg(message))
            }
            // Fades, to and from black.
            Rectangle {
                anchors.fill: parent
                color: "black"
                opacity: {
                    const c = root.current
                    if (!c) return 0
                    const o = root.currentOffset
                    if (c.fadeIn > 0 && o < c.fadeIn) return 1 - o / c.fadeIn
                    if (c.fadeOut > 0 && c.length - o < c.fadeOut) return 1 - (c.length - o) / c.fadeOut
                    return 0
                }
            }
            // The texts, as drawtext will draw them.
            Repeater {
                model: Project.texts
                delegate: Text {
                    required property var modelData
                    readonly property real scale_: canvas.height / 1080
                    visible: root.playhead >= modelData.start && root.playhead < modelData.start + modelData.length
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: modelData.position === "top" ? canvas.height / 10
                        : modelData.position === "center" ? (canvas.height - height) / 2
                        : canvas.height - height - canvas.height / 10
                    text: modelData.text
                    color: modelData.color
                    font.pixelSize: Math.max(1, modelData.textSize * scale_)
                    style: Text.Outline
                    styleColor: Qt.rgba(0, 0, 0, 0.6)
                }
            }
        }

        Column {
            visible: Project.video.length === 0
            anchors.centerIn: parent
            spacing: 12
            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 64
                height: 64
                source: Theme.icons + "kdenlive"
                sourceSize: Qt.size(64, 64)
                opacity: 0.8
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Put videos and pictures on the timeline to see them here")
                color: "white"
                opacity: 0.8
                font.pixelSize: Theme.fontBody
            }
        }
        TapHandler { onTapped: root.togglePlay() }
    }

    // A file being dragged from the media to the timeline.
    Rectangle {
        id: ghost
        property string text
        visible: false
        z: 50
        width: ghostText.implicitWidth + 16
        height: 28
        radius: Theme.radius
        color: Theme.flyout
        border.width: 1
        border.color: Theme.stroke
        Text { id: ghostText; anchors.centerIn: parent; text: ghost.text; color: Theme.text; font.pixelSize: Theme.fontCaption }
    }

    // --- under the preview: the time and the transport ---
    Item {
        id: transport
        anchors { left: pool.right; right: inspector.left; bottom: timelineFrame.top }
        height: 48
        Text {
            anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
            text: root.clock(root.playhead) + "  /  " + root.clock(Project.videoLength)
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
        }
        Row {
            anchors.centerIn: parent
            spacing: 4
            ToolButton { icon: "media-skip-backward"; tooltip: qsTr("To the start (Home)"); onClicked: root.seek(0) }
            ToolButton { text: "‹"; tooltip: qsTr("Previous frame (,)"); usable: root.loaded; onClicked: root.step(false) }
            ToolButton {
                icon: root.playing ? "media-playback-pause" : "media-playback-start"
                tooltip: root.playing ? qsTr("Pause (Space)") : qsTr("Play (Space)")
                usable: root.loaded
                onClicked: root.togglePlay()
            }
            ToolButton { text: "›"; tooltip: qsTr("Next frame (.)"); usable: root.loaded; onClicked: root.step(true) }
            ToolButton { icon: "media-skip-forward"; tooltip: qsTr("To the end (End)"); onClicked: root.seek(Project.videoLength) }
        }
        Row {
            anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
            spacing: 2
            ToolButton { icon: "zoom-out"; tooltip: qsTr("Zoom out (Ctrl+-)"); onClicked: timeline.zoomTo(timeline.pps * 0.8, timeline.width / 2) }
            ToolButton { icon: "zoom-fit-best"; tooltip: qsTr("Fit the timeline"); onClicked: timeline.fit() }
            ToolButton { icon: "zoom-in"; tooltip: qsTr("Zoom in (Ctrl+=)"); onClicked: timeline.zoomTo(timeline.pps * 1.25, timeline.width / 2) }
        }
    }

    Inspector {
        id: inspector
        anchors { right: parent.right; top: commands.bottom; bottom: timelineFrame.top }
        width: 280
        win: root
    }

    // --- the timeline ---
    Rectangle {
        id: timelineFrame
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 212
        color: Theme.background
        Rectangle { width: parent.width; height: 1; color: Theme.divider }
        TimelineView {
            id: timeline
            anchors { fill: parent; topMargin: 1; rightMargin: 8 }
            win: root
            pps: settings.zoom
            onPpsChanged: settings.zoom = pps
        }
    }

    // Files dropped anywhere else join the media.
    DropArea {
        anchors.fill: parent
        z: -1
        keys: ["text/uri-list"]
        onDropped: drop => { if (drop.hasUrls) Media.add(drop.urls) }
    }

    // A hidden second player for the audio track.
    MpvPlayer {
        id: musicPlayer
        visible: false
        Component.onCompleted: {
            set("vid", "no")
            set("keep-open", "yes")
            set("pause", "yes")
            set("volume-max", "200")
            set("audio-client-name", "Video Editor")
        }
    }

    // --- pages and dialogs ---
    property bool showSettings: false
    Loader {
        anchors { fill: parent; topMargin: strip.height }
        active: root.showSettings
        z: 60
        sourceComponent: SettingsPage {
            win: root
            onClosed: root.showSettings = false
            Component.onCompleted: forceActiveFocus()
        }
    }

    ExportDialog { id: exportDialog; win: root }

    Dialog {
        id: errorDialog
        title: qsTr("Video Editor")
    }
    Dialog {
        id: saveDialog
        property var then: null
        title: qsTr("Video Editor")
        buttons: [{ text: qsTr("Save"), accent: true, value: "save" }, { text: qsTr("Don't save"), value: "discard" }, { text: qsTr("Cancel"), value: "cancel" }]
        onChosen: value => {
            const then = saveDialog.then
            saveDialog.then = null
            if (value === "save") root.save(then)
            else if (value === "discard") then()
        }
    }
    Dialog {
        id: restoreDialog
        title: qsTr("Restore the unsaved changes?")
        text: qsTr("The Video Editor kept changes to %1 that weren't saved.").arg(root.projectName)
        buttons: [{ text: qsTr("Restore"), accent: true, value: "restore" }, { text: qsTr("Discard them"), value: "discard" }]
        onChosen: value => {
            if (value === "restore") {
                const error = Project.restore(root.autosavePath)
                if (error !== "") root.showError(error)
            } else {
                root.forget()
            }
            root.checkMissing()
        }
    }
    // Files the project uses that aren't where they were.
    Dialog {
        id: relinkDialog
        title: qsTr("Some files are missing")
        text: qsTr("Find them to use them again: the preview and the export need every file.")
        buttons: [{ text: qsTr("Close"), value: "cancel" }]
        property var files: []
        onVisibleChanged: if (visible) files = Project.missing()
        Column {
            width: parent.width
            spacing: 6
            Repeater {
                model: relinkDialog.files
                delegate: Item {
                    required property string modelData
                    width: parent.width
                    height: 32
                    Text {
                        anchors { left: parent.left; right: findButton.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
                        text: modelData
                        color: Theme.text
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideMiddle
                    }
                    Button {
                        id: findButton
                        anchors.right: parent.right
                        text: qsTr("Find...")
                        onClicked: root.pick(false, "", "", [qsTr("%1 (%1)").arg(root.fileName(modelData))], files => {
                            Project.relink(modelData, root.localPath(files[0]))
                            relinkDialog.files = Project.missing()
                            if (relinkDialog.files.length === 0) relinkDialog.close("cancel")
                        })
                    }
                }
            }
        }
    }

    Dialog {
        id: shortcutsDialog
        title: qsTr("Keyboard shortcuts")
        buttons: [{ text: qsTr("Close"), accent: true, value: "cancel" }]
        Grid {
            columns: 2
            columnSpacing: 24
            rowSpacing: 6
            Repeater {
                model: [
                    "Space", qsTr("Play or pause"),
                    ", .", qsTr("Previous or next frame"),
                    "Home End", qsTr("To the start or the end"),
                    "S", qsTr("Split at the playhead"),
                    "Del", qsTr("Delete and close the gap"),
                    "Shift+Del", qsTr("Delete and leave a gap"),
                    "T", qsTr("Add a text"),
                    "Ctrl+Z", qsTr("Undo"),
                    "Ctrl+Shift+Z", qsTr("Redo"),
                    "Ctrl+= Ctrl+-", qsTr("Zoom the timeline"),
                    "Ctrl+I", qsTr("Add media"),
                    "Ctrl+S", qsTr("Save"),
                    "Ctrl+E", qsTr("Export")
                ]
                delegate: Text {
                    required property string modelData
                    required property int index
                    text: modelData
                    color: index % 2 ? Theme.text : Theme.textSecondary
                    font.pixelSize: Theme.fontBody
                    font.weight: index % 2 ? Font.Normal : Font.DemiBold
                }
            }
        }
    }

    MenuLayer {
        id: menuLayer
        anchors.fill: parent
        z: 100
    }

    ResizeEdges {}
}
