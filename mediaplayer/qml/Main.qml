// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtCore
import Vela.Controls
import Vela.MediaPlayer.Backend

// The Media Player's window, the one of Windows 11's: its own title bar, the
// picture, the folder's other files at the side and the transport, which
// floats over the picture's bottom and hides with the pointer while a video
// plays; the mini player is the picture alone in a small window.
Window {
    id: root
    flags: Qt.Window | Qt.FramelessWindowHint // no title bar from Vela
    width: 1000
    height: 640
    minimumWidth: mini ? 240 : 560
    minimumHeight: mini ? 135 : 360
    visible: true
    color: Theme.background
    title: Playlist.name !== "" ? Playlist.name + qsTr(" - Media Player") : qsTr("Media Player")

    onActiveChanged: Theme.windowActive = active

    readonly property bool maximized: visibility === Window.Maximized
    readonly property bool fullScreen: visibility === Window.FullScreen
    function toggleMaximized() { maximized ? showNormal() : showMaximized() }
    function toggleFullScreen() {
        setMini(false)
        fullScreen ? showNormal() : showFullScreen()
    }

    // --- what mpv says ---
    readonly property var info: player.info
    readonly property bool loaded: !info.idle
    readonly property bool hasVideo: info.tracks.some(t => t.type === "video" && t.selected)
    readonly property real volume: info.volume
    onVolumeChanged: settings.volume = volume

    Settings {
        id: settings
        property real volume: 100
        property bool playlistOpen: true
        property bool shuffle: false
        property bool repeat: false
    }

    function play() {
        if (!player.ready || Playlist.index < 0) return
        player.remember() // the one that was playing
        player.load(Playlist.current)
        player.set("pause", "no")
    }
    function togglePause() {
        if (loaded) player.set("pause", info.paused ? "no" : "yes")
        else play() // the list's last file ended: again
    }
    // The next file: any other one when shuffling, the first again after the
    // last when repeating.
    readonly property bool hasNext: Playlist.index < Playlist.files.length - 1
        || (Playlist.index >= 0 && (settings.repeat || (settings.shuffle && Playlist.files.length > 1)))
    function next() {
        const count = Playlist.files.length
        if (settings.shuffle && count > 1) {
            Playlist.index = (Playlist.index + 1 + Math.floor(Math.random() * (count - 1))) % count
        } else if (Playlist.index < count - 1) {
            Playlist.index++
        } else if (settings.repeat && count === 1) {
            play()
        } else if (settings.repeat) {
            Playlist.index = 0
        }
    }
    // The song's artist; a video's folder.
    readonly property string byline: {
        const key = Object.keys(info.metadata).find(k => k.toLowerCase() === "artist")
        return key ? info.metadata[key] : Playlist.folder.split("/").pop()
    }
    function seekBy(seconds) { player.command(["seek", String(seconds), "relative"]) }
    function changeVolume(by) { player.set("volume", String(Math.max(0, Math.min(100, info.volume + by)))) }
    function clock(seconds) {
        const s = Math.max(0, Math.floor(seconds))
        const m = Math.floor(s % 3600 / 60)
        return (s >= 3600 ? Math.floor(s / 3600) + ":" + String(m).padStart(2, "0") : m) + ":" + String(s % 60).padStart(2, "0")
    }
    function fileName(url) { return decodeURIComponent(url.toString().split("/").pop()) }

    Connections {
        target: Playlist
        function onIndexChanged() {
            root.play()
            list.positionViewAtIndex(Playlist.index, ListView.Contain)
        }
    }
    onClosing: player.remember()

    // --- the mini player: the picture alone, in a small window ---
    property bool mini: false
    property size normalSize
    function setMini(on) {
        if (on === mini) return
        if (on) {
            if (fullScreen || maximized) showNormal()
            normalSize = Qt.size(width, height)
            mini = true
            width = 400
            height = 225
        } else {
            mini = false
            width = normalSize.width
            height = normalSize.height
        }
    }

    // --- the transport hides with the pointer when it floats over the picture ---
    property bool awake: true
    readonly property bool barShown: !hasVideo || awake || info.paused || menuLayer.isOpen
    // Subtitles stay above the transport.
    onBarShownChanged: player.set("sub-pos", barShown && !mini ? "80" : "100")
    onMiniChanged: player.set("sub-pos", barShown && !mini ? "80" : "100")
    function wake() {
        awake = true
        doze.restart()
    }
    Timer {
        id: doze
        interval: 1200
        onTriggered: root.awake = false
    }

    // --- files ---
    property var picker: null
    function pick(filters, folder, done) {
        if (!picker) {
            const component = Qt.createComponent("FilePicker.qml")
            picker = component.createObject(root, { parentWindow: root })
            if (!picker) {
                showError(component.errorString())
                return
            }
        }
        picker.nameFilters = filters
        picker.pick(false, folder, "", files => { if (files.length > 0) done(files[0]) })
    }
    function open() {
        const folder = Playlist.index >= 0 ? "file://" + Playlist.folder : StandardPaths.writableLocation(StandardPaths.MoviesLocation).toString()
        pick([qsTr("Videos and music (%1)").arg(Playlist.filter()), qsTr("All files (*)")], folder, file => Playlist.open(file))
    }
    function openSubtitle() {
        pick([qsTr("Subtitles (*.srt *.ass *.ssa *.vtt *.sub)"), qsTr("All files (*)")], "file://" + Playlist.folder,
            file => player.command(["sub-add", file.toString()]))
    }
    function showError(message) {
        errorDialog.text = message
        errorDialog.open()
    }

    // --- menus ---
    function speedEntries() {
        return [0.25, 0.5, 0.75, 1, 1.25, 1.5, 1.75, 2].map(speed => ({
            text: speed + "×",
            radio: true,
            checked: Math.abs(info.speed - speed) < 0.01,
            action: () => player.set("speed", String(speed))
        }))
    }
    function trackName(track) {
        return [track.title, track.lang].filter(Boolean).join(" · ") || qsTr("Track %1").arg(track.id)
    }
    function trackEntries() {
        const subtitles = info.tracks.filter(t => t.type === "sub")
        const audio = info.tracks.filter(t => t.type === "audio")
        const entries = [
            { header: qsTr("Subtitles") },
            { text: qsTr("Off"), radio: true, checked: !subtitles.some(t => t.selected), action: () => player.set("sid", "no") }
        ]
        subtitles.forEach(t => entries.push({ text: trackName(t), radio: true, checked: !!t.selected, action: () => player.set("sid", String(t.id)) }))
        entries.push({ separator: true }, { text: qsTr("Open a subtitle file..."), action: () => root.openSubtitle() })
        if (audio.length > 1) {
            entries.push({ separator: true }, { header: qsTr("Audio") })
            audio.forEach(t => entries.push({ text: trackName(t), radio: true, checked: !!t.selected, action: () => player.set("aid", String(t.id)) }))
        }
        return entries
    }
    function moreEntries() {
        return [
            { text: qsTr("&Open..."), shortcut: "Ctrl+O", action: () => root.open() },
            { separator: true },
            { text: qsTr("Playback &speed"), children: speedEntries() },
            { text: qsTr("&Playlist"), shortcut: "Ctrl+L", checked: settings.playlistOpen, action: () => settings.playlistOpen = !settings.playlistOpen }
        ]
    }
    function openMenu(button, entries) {
        const p = button.mapToItem(null, 0, 0)
        menuLayer.open(entries, p.x, p.y - 4, { above: true })
    }

    Shortcut { sequence: "Ctrl+O"; onActivated: root.open() }
    Shortcut { sequence: "Space"; onActivated: root.togglePause() }
    Shortcut { sequence: "Left"; onActivated: root.seekBy(-5) }
    Shortcut { sequence: "Right"; onActivated: root.seekBy(5) }
    Shortcut { sequence: "Up"; onActivated: root.changeVolume(5) }
    Shortcut { sequence: "Down"; onActivated: root.changeVolume(-5) }
    Shortcut { sequence: "M"; onActivated: player.set("mute", info.muted ? "no" : "yes") }
    Shortcut { sequence: "Ctrl+B"; onActivated: Playlist.index-- }
    Shortcut { sequence: "Ctrl+F"; onActivated: root.next() }
    Shortcut { sequence: "Ctrl+L"; onActivated: settings.playlistOpen = !settings.playlistOpen }
    Shortcut { sequence: "Ctrl+M"; onActivated: root.setMini(!root.mini) }
    Shortcut { sequences: ["F11", "F"]; onActivated: root.toggleFullScreen() }
    Shortcut {
        sequence: "Escape"
        enabled: root.fullScreen || root.mini
        onActivated: root.mini ? root.setMini(false) : root.showNormal()
    }

    // --- the title bar ---
    Item {
        id: strip
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: visible ? 32 : 0
        visible: !root.fullScreen && !root.mini

        DragHandler {
            target: null
            onActiveChanged: if (active) root.startSystemMove()
        }
        TapHandler {
            onDoubleTapped: root.toggleMaximized()
        }

        Image {
            x: 14
            y: 8
            width: 16
            height: 16
            source: "image://appicon/multimedia-video-player"
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

    // --- the picture ---
    Rectangle {
        id: stage
        anchors {
            left: parent.left
            right: side.left
            top: strip.bottom
            bottom: parent.bottom
        }
        color: root.hasVideo || root.fullScreen ? "black" : Theme.background

        // Always there, even with nothing to show: mpv needs it to draw.
        MpvPlayer {
            id: player
            anchors.fill: parent
            onReadyChanged: {
                player.set("volume", String(settings.volume))
                root.play() // the file the Media Player was started with
            }
            onEnded: root.next()
            onFailed: message => root.showError(qsTr("%1 can't be played: %2").arg(Playlist.name).arg(message))
        }

        // Music, or nothing yet.
        Column {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: root.mini ? 0 : -bar.height / 2
            visible: !root.hasVideo
            spacing: 16
            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                width: root.mini ? 48 : 96
                height: width
                source: Theme.icons + (root.loaded ? "audio-x-generic" : "multimedia-video-player")
                sourceSize: Qt.size(width, height)
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(implicitWidth, stage.width - 48)
                text: root.loaded ? root.info.title : qsTr("Open a video or a song, or drop it here")
                color: root.fullScreen ? "white" : Theme.text
                font.pixelSize: Theme.fontBody
                elide: Text.ElideMiddle
            }
            ToolButton {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: !root.loaded && !root.mini
                icon: "document-open"
                text: qsTr("Open")
                onClicked: root.open()
            }
        }

        TapHandler {
            onTapped: if (root.loaded) root.togglePause()
            onDoubleTapped: root.mini ? root.setMini(false) : root.toggleFullScreen()
        }
        DragHandler { // the mini player has no title bar: it moves by its picture
            target: null
            enabled: root.mini
            onActiveChanged: if (active) root.startSystemMove()
        }
        WheelHandler {
            onWheel: event => root.changeVolume(event.angleDelta.y > 0 ? 5 : -5)
        }
        DropArea {
            anchors.fill: parent
            onDropped: drop => { if (drop.hasUrls) Playlist.open(drop.urls[0]) }
        }

        // The closed playlist's way back, in the picture's corner.
        Rectangle {
            anchors { right: parent.right; top: parent.top; margins: 12 }
            visible: !settings.playlistOpen && !root.fullScreen && !root.mini && root.barShown
            width: 40
            height: 40
            radius: Theme.radius
            color: Theme.background
            ToolButton {
                anchors.centerIn: parent
                icon: "view-media-playlist"
                tooltip: qsTr("Playlist (Ctrl+L)")
                onClicked: settings.playlistOpen = true
            }
        }

        // The mini player's few commands, while the pointer moves over it.
        Rectangle {
            anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 12 }
            visible: root.mini && root.awake
            width: miniRow.width + 8
            height: 40
            radius: Theme.radius
            color: Theme.background
            Row {
                id: miniRow
                anchors.centerIn: parent
                ToolButton { icon: "media-skip-backward"; usable: Playlist.index > 0; onClicked: Playlist.index-- }
                ToolButton { icon: root.loaded && !root.info.paused ? "media-playback-pause" : "media-playback-start"; onClicked: root.togglePause() }
                ToolButton { icon: "media-skip-forward"; usable: root.hasNext; onClicked: root.next() }
                ToolButton { icon: "view-restore"; tooltip: qsTr("Leave the mini player (Esc)"); onClicked: root.setMini(false) }
                ToolButton { icon: "window-close"; onClicked: root.close() }
            }
        }
    }

    // --- the folder's files ---
    Rectangle {
        id: side
        anchors { right: parent.right; top: strip.bottom; bottom: parent.bottom }
        visible: settings.playlistOpen && !root.fullScreen && !root.mini
        width: visible ? 280 : 0
        color: Theme.background

        Text {
            id: sideTitle
            x: 16
            height: 40
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Playlist")
            color: Theme.text
            font.pixelSize: Theme.fontBody
            font.weight: Font.DemiBold
        }
        ToolButton {
            anchors { right: parent.right; rightMargin: 6; verticalCenter: sideTitle.verticalCenter }
            icon: "window-close"
            tooltip: qsTr("Close the playlist (Ctrl+L)")
            onClicked: settings.playlistOpen = false
        }
        ListView {
            id: list
            anchors { left: parent.left; right: parent.right; top: sideTitle.bottom; bottom: parent.bottom; margins: 4 }
            clip: true
            model: Playlist.files
            boundsBehavior: Flickable.StopAtBounds
            delegate: Rectangle {
                id: row
                required property url modelData
                required property int index
                readonly property bool current: index === Playlist.index
                width: list.width
                height: 36
                radius: Theme.radius
                color: rowMouse.pressed ? Theme.subtlePressed : rowMouse.containsMouse || current ? Theme.subtleHover : "transparent"
                Rectangle {
                    visible: row.current
                    anchors.verticalCenter: parent.verticalCenter
                    width: 3
                    height: 16
                    radius: 1.5
                    color: Theme.accentFill
                }
                Text {
                    anchors { left: parent.left; right: parent.right; leftMargin: 12; rightMargin: 8; verticalCenter: parent.verticalCenter }
                    text: root.fileName(row.modelData)
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    elide: Text.ElideMiddle
                }
                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: row.current ? root.togglePause() : Playlist.index = row.index
                }
            }
        }
    }

    // --- the transport, over the picture's bottom ---
    Rectangle {
        id: bar
        anchors { left: parent.left; right: side.left; bottom: parent.bottom }
        height: 104
        visible: !root.mini
        color: root.hasVideo ? Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.82) : Theme.background
        opacity: root.barShown ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.fast } }

        MouseArea { anchors.fill: parent } // a click between the buttons isn't one on the picture

        Text {
            id: elapsed
            x: 16
            width: 48
            anchors.verticalCenter: seek.verticalCenter
            text: root.clock(seek.pressed ? seek.value : root.info.position)
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
        }
        Slider {
            id: seek
            anchors { left: elapsed.right; right: total.left; top: parent.top; topMargin: 4 }
            to: Math.max(1, root.info.duration)
            // While it's dragged the picture follows roughly; let go, exactly.
            onMoved: value => { if (root.loaded) player.command(["seek", String(value), "absolute+keyframes"]) }
            onReleased: value => { if (root.loaded) player.command(["seek", String(value), "absolute"]) }
        }
        Binding {
            target: seek
            property: "value"
            value: root.info.position
            when: !seek.pressed
            restoreMode: Binding.RestoreNone
        }
        Text {
            id: total
            anchors { right: parent.right; rightMargin: 16; verticalCenter: seek.verticalCenter }
            width: 48
            horizontalAlignment: Text.AlignRight
            text: root.clock(root.info.duration)
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
        }

        // What's playing.
        Column {
            anchors { left: parent.left; leftMargin: 16; right: transport.left; rightMargin: 16; verticalCenter: transport.verticalCenter }
            visible: root.loaded
            spacing: 2
            Text {
                width: parent.width
                text: root.info.title
                color: Theme.text
                font.pixelSize: 16
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: root.byline
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                elide: Text.ElideRight
            }
        }

        Row {
            id: transport
            anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 10 }
            spacing: 6
            ToolButton {
                anchors.verticalCenter: parent.verticalCenter
                icon: "media-playlist-shuffle"
                tooltip: qsTr("Shuffle")
                checked: settings.shuffle
                onClicked: settings.shuffle = !settings.shuffle
            }
            ToolButton {
                anchors.verticalCenter: parent.verticalCenter
                icon: "media-skip-backward"
                tooltip: qsTr("Previous (Ctrl+B)")
                usable: Playlist.index > 0
                onClicked: Playlist.index--
            }
            // Play, the round one.
            Rectangle {
                width: 48
                height: 48
                radius: 24
                color: playMouse.pressed ? Theme.subtlePressed : playMouse.containsMouse ? Theme.subtleHover : "transparent"
                border.width: 2
                border.color: Theme.accentFill
                opacity: Playlist.index >= 0 ? 1 : 0.4
                Image {
                    anchors.centerIn: parent
                    width: 20
                    height: 20
                    source: Theme.icons + (root.loaded && !root.info.paused ? "media-playback-pause" : "media-playback-start")
                    sourceSize: Qt.size(width, height)
                }
                MouseArea {
                    id: playMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: Playlist.index >= 0
                    onClicked: root.togglePause()
                }
            }
            ToolButton {
                anchors.verticalCenter: parent.verticalCenter
                icon: "media-skip-forward"
                tooltip: qsTr("Next (Ctrl+F)")
                usable: root.hasNext
                onClicked: root.next()
            }
            ToolButton {
                anchors.verticalCenter: parent.verticalCenter
                icon: "media-playlist-repeat"
                tooltip: qsTr("Repeat")
                checked: settings.repeat
                onClicked: settings.repeat = !settings.repeat
            }
        }
        Row {
            anchors { right: parent.right; rightMargin: 12; verticalCenter: transport.verticalCenter }
            spacing: 2
            ToolButton {
                anchors.verticalCenter: parent.verticalCenter
                icon: root.info.muted || root.info.volume === 0 ? "audio-volume-muted" : "audio-volume-high"
                tooltip: qsTr("Mute (M)")
                onClicked: player.set("mute", root.info.muted ? "no" : "yes")
            }
            Slider {
                id: loudness
                anchors.verticalCenter: parent.verticalCenter
                visible: bar.width > 760
                width: 100
                to: 100
                onMoved: value => player.set("volume", String(value))
            }
            Binding {
                target: loudness
                property: "value"
                value: root.info.volume
                when: !loudness.pressed
                restoreMode: Binding.RestoreNone
            }
            ToolButton {
                id: trackButton
                anchors.verticalCenter: parent.verticalCenter
                icon: "media-view-subtitles-symbolic"
                tooltip: qsTr("Subtitles and audio")
                usable: root.loaded
                onClicked: root.openMenu(trackButton, root.trackEntries())
            }
            ToolButton {
                anchors.verticalCenter: parent.verticalCenter
                visible: !root.fullScreen
                icon: "view-media-playlist"
                tooltip: qsTr("Playlist (Ctrl+L)")
                checked: settings.playlistOpen
                onClicked: settings.playlistOpen = !settings.playlistOpen
            }
            ToolButton {
                anchors.verticalCenter: parent.verticalCenter
                icon: root.fullScreen ? "view-restore" : "view-fullscreen"
                tooltip: root.fullScreen ? qsTr("Leave full screen (Esc)") : qsTr("Full screen (F11)")
                onClicked: root.toggleFullScreen()
            }
            ToolButton {
                anchors.verticalCenter: parent.verticalCenter
                icon: "window-minimize-pip"
                tooltip: qsTr("Mini player (Ctrl+M)")
                onClicked: root.setMini(true)
            }
            ToolButton {
                id: moreButton
                anchors.verticalCenter: parent.verticalCenter
                text: "⋯"
                tooltip: qsTr("More")
                onClicked: root.openMenu(moreButton, root.moreEntries())
            }
        }
    }

    // Any movement of the pointer brings the transport back; at rest over a
    // full screen picture the pointer goes too.
    HoverHandler {
        cursorShape: root.fullScreen && !root.barShown ? Qt.BlankCursor : Qt.ArrowCursor
        // Only a real movement: the point also "changes" with every frame
        // of the video drawn under a resting pointer.
        property point last
        onPointChanged: {
            if (point.position.x !== last.x || point.position.y !== last.y) {
                last = point.position
                root.wake()
            }
        }
    }

    MenuLayer {
        id: menuLayer
        anchors.fill: parent
        z: 100
    }

    Dialog {
        id: errorDialog
        title: qsTr("Media Player")
    }

    ResizeEdges {}
}
