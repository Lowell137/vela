// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton

import QtQuick

// Vela's visual language, one for the shell and the apps: Windows 11's
// (Fluent) colors and sizes, light or dark, with the accent the user chose.
// The names follow WinUI's resources (TextFillColorSecondary is
// textSecondary, ControlCornerRadius is radius...), so a value can be checked
// against Microsoft's. Light or dark is Style's: the shell's mode for system
// UI, the apps' mode for apps (vela::controls::install).
QtObject {
    readonly property bool light: Style.light

    // The chosen theme's values (Style.theme, from themes/<name>.conf):
    // "dark.taskbar" or "light.taskbar" for one mode, "taskbar" for both. Not
    // in the theme: Vela's own value.
    function themed(key, fallback) {
        const t = Style.theme
        const modeKey = (light ? "light." : "dark.") + key
        return modeKey in t ? t[modeKey] : (key in t ? t[key] : fallback)
    }

    // --- the window: Mica, the same as Vela's title bar ---
    property bool windowActive: true // the app's Main.qml updates it
    readonly property color background: themed("background", windowActive ? Style.mica : Style.micaInactive)

    // --- controls and surfaces ---
    readonly property color control: themed("control", light ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(1, 1, 1, 0.061))
    readonly property color controlHover: themed("controlHover", light ? Qt.rgba(0.976, 0.976, 0.976, 0.5) : Qt.rgba(1, 1, 1, 0.084))
    readonly property color controlPressed: themed("controlPressed", light ? Qt.rgba(0.976, 0.976, 0.976, 0.3) : Qt.rgba(1, 1, 1, 0.033))
    readonly property color controlStroke: themed("controlStroke", light ? Qt.rgba(0, 0, 0, 0.07) : Qt.rgba(1, 1, 1, 0.07))
    readonly property color controlStrokeStrong: light ? Qt.rgba(0, 0, 0, 0.45) : Qt.rgba(1, 1, 1, 0.55)
    readonly property color subtleHover: themed("subtleHover", light ? Qt.rgba(0, 0, 0, 0.037) : Qt.rgba(1, 1, 1, 0.061))
    readonly property color subtlePressed: themed("subtlePressed", light ? Qt.rgba(0, 0, 0, 0.024) : Qt.rgba(1, 1, 1, 0.042))
    readonly property color stroke: themed("stroke", light ? Qt.rgba(0, 0, 0, 0.09) : Qt.rgba(1, 1, 1, 0.09)) // panels and menus
    // subtleHover at alpha 0: what a subtle control fades from ("transparent"
    // is black at alpha 0, and a fade from it would pass through gray).
    readonly property color subtleRest: light ? Qt.rgba(0, 0, 0, 0) : Qt.rgba(1, 1, 1, 0)
    readonly property color divider: light ? Qt.rgba(0, 0, 0, 0.08) : Qt.rgba(1, 1, 1, 0.084)
    readonly property color card: themed("card", light ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(1, 1, 1, 0.051))
    readonly property color cardHover: themed("cardHover", light ? Qt.rgba(0.976, 0.976, 0.976, 0.5) : Qt.rgba(1, 1, 1, 0.083))
    readonly property color cardPressed: themed("cardPressed", light ? Qt.rgba(0.976, 0.976, 0.976, 0.3) : Qt.rgba(1, 1, 1, 0.035))
    readonly property color cardStroke: themed("cardStroke", light ? Qt.rgba(0, 0, 0, 0.06) : Qt.rgba(0, 0, 0, 0.18))
    // Menus and flyouts inside a window (no blur of their own).
    readonly property color flyout: themed("flyout", light ? "#f9f9f9" : "#2c2c2c")
    readonly property color dialog: themed("dialog", light ? "#fbfbfb" : "#2b2b2b")
    readonly property color dialogFooter: themed("dialogFooter", light ? "#f3f3f3" : "#202020")
    readonly property color dialogStroke: light ? Qt.rgba(0, 0, 0, 0.12) : Qt.rgba(0, 0, 0, 0.45)
    readonly property color smoke: Qt.rgba(0, 0, 0, 0.3) // behind a window's dialogs
    // The veil over the screen behind a system dialog (UAC): dark in light
    // mode too, like Windows.
    readonly property color veil: Effects.blurAvailable ? Qt.rgba(0, 0, 0, light ? 0.35 : 0.45) : Qt.rgba(0, 0, 0, 0.6)
    readonly property color field: themed("field", light ? "#ffffff" : "#1e1e1e") // a text field while typing
    readonly property color focusRing: light ? Qt.rgba(0, 0, 0, 0.45) : Qt.rgba(1, 1, 1, 0.35)
    // Files: the files and navigation area, the selection, the scroll bars.
    readonly property color layer: light ? Qt.rgba(1, 1, 1, 0.5) : Qt.rgba(0.227, 0.227, 0.227, 0.30)
    readonly property color layerStroke: light ? Qt.rgba(0, 0, 0, 0.06) : Qt.rgba(0, 0, 0, 0.10)
    readonly property color selection: light ? Qt.rgba(0, 0, 0, 0.06) : Qt.rgba(1, 1, 1, 0.10)
    readonly property color selectionHover: light ? Qt.rgba(0, 0, 0, 0.09) : Qt.rgba(1, 1, 1, 0.13)
    readonly property color scrollTrack: light ? Qt.rgba(0, 0, 0, 0.04) : Qt.rgba(1, 1, 1, 0.05)
    readonly property color scrollThumb: light ? Qt.rgba(0, 0, 0, 0.35) : Qt.rgba(1, 1, 1, 0.35)
    readonly property color scrollThumbHover: light ? Qt.rgba(0, 0, 0, 0.55) : Qt.rgba(1, 1, 1, 0.55)

    // --- the shell's surfaces ---
    // With the compositor's blur (docs/renderer.md §8.3) panels are acrylic
    // like on Windows 11: semi-transparent over the blurred background.
    // Without it, opaque.
    readonly property bool acrylic: Effects.blurAvailable
    readonly property color desktop: "#06182d" // below the wallpaper, its darkest blue
    // taskbar-opacity= of vela.conf scales the fill's alpha (100%: the theme's).
    readonly property color taskbar: {
        const c = Qt.color(themed("taskbar", light
            ? (acrylic ? Qt.rgba(0.95, 0.95, 0.96, 0.66) : Qt.rgba(0.94, 0.94, 0.95, 0.96))
            : (acrylic ? Qt.rgba(0.11, 0.11, 0.13, 0.62) : Qt.rgba(0.10, 0.10, 0.12, 0.94))))
        const k = Style.theme["user-taskbar-opacity"]
        return k === undefined ? c : Qt.rgba(c.r, c.g, c.b, Math.max(0, Math.min(1, c.a * k)))
    }
    // The hairline of the taskbar's edge and pills; a theme with a see-through
    // taskbar sets it transparent (taskbar-stroke=#00000000).
    readonly property color taskbarStroke: themed("taskbar-stroke", stroke)
    // Acrylic: the compositor's blur is the whole fill, like Windows 11's
    // flyouts; the panel only gives it its shape (blur_mix.frag's mask).
    readonly property color surface: themed("surface", light
        ? (acrylic ? Qt.rgba(0.99, 0.99, 0.99, 0.04) : Qt.rgba(0.95, 0.95, 0.96, 0.98))
        : (acrylic ? Qt.rgba(0.17, 0.17, 0.17, 0.04) : Qt.rgba(0.14, 0.14, 0.16, 0.97)))
    // The shell's menus, surfaces of their own that the compositor blurs.
    readonly property color popup: themed("popup", light
        ? (acrylic ? Qt.rgba(0.98, 0.98, 0.98, 0.82) : "#f9f9f9")
        : (acrylic ? Qt.rgba(0.17, 0.17, 0.19, 0.78) : "#2a2a2f"))
    // Panels over the output (Task View, Snap Assist), and the window cards
    // on them.
    readonly property color backdrop: themed("backdrop", light
        ? (acrylic ? Qt.rgba(0.93, 0.93, 0.95, 0.55) : Qt.rgba(0.92, 0.92, 0.94, 0.96))
        : (acrylic ? Qt.rgba(0.05, 0.05, 0.07, 0.55) : Qt.rgba(0.07, 0.07, 0.09, 0.96)))
    readonly property color taskCard: themed("taskCard", light ? Qt.rgba(0.98, 0.98, 0.99, 0.92) : Qt.rgba(0.15, 0.15, 0.17, 0.92))
    readonly property color taskCardHover: themed("taskCardHover", light ? Qt.rgba(1, 1, 1, 0.97) : Qt.rgba(0.20, 0.20, 0.23, 0.95))
    // To lighten (or darken, in light mode) what's below: hover on colored
    // tiles, zone backgrounds in a panel.
    readonly property color overlay: light ? "black" : "white"
    readonly property color footer: light ? Qt.rgba(0, 0, 0, 0.035) : Qt.rgba(0, 0, 0, 0.18)
    readonly property int radiusIsland: 12 // the taskbar islands
    // The taskbar window's height; the shell sets it for the dock style.
    property int taskbarHeight: 48
    readonly property int topBarHeight: 28 // the dock style's top panel

    // --- text ---
    readonly property color text: themed("text", light ? Qt.rgba(0, 0, 0, 0.894) : "#ffffff")
    readonly property color textSecondary: themed("textSecondary", light ? Qt.rgba(0, 0, 0, 0.62) : Qt.rgba(1, 1, 1, 0.786))
    readonly property color textTertiary: themed("textTertiary", light ? Qt.rgba(0, 0, 0, 0.447) : Qt.rgba(1, 1, 1, 0.545))
    readonly property color critical: light ? "#c42b1c" : "#ff99a4"
    readonly property color caution: light ? "#9d5d00" : "#fce100"

    // --- accent: in dark mode the light tint with black text, in light mode
    // the dark one with white text ---
    readonly property color accent: themed("accent", Style.accent)
    readonly property color accentFill: light ? Qt.darker(accent, 1.15) : Qt.lighter(accent, 1.35)
    readonly property color accentFillHover: light ? Qt.darker(accent, 1.05) : Qt.lighter(accent, 1.25)
    readonly property color accentText: light ? "#ffffff" : "#000000"
    readonly property color link: light ? Qt.darker(accent, 1.3) : Qt.lighter(accent, 1.5)
    // Icons of the theme matching the mode ("image://icon/l/<name>"; the
    // prefix reloads them when the mode changes). Always white on the accent.
    readonly property string icons: "image://icon/" + Style.iconMode
    // Dark glyphs whatever the mode: for a surface that is always light (Windows 7's menus).
    readonly property string iconsOnLight: "image://icon/k/" + Style.iconMode.slice(2)
    readonly property string iconsOnAccent: Style.iconMode.startsWith("l/") ? "image://icon/w/" + Style.iconMode.slice(2) : icons

    // --- shapes and type ---
    readonly property int radius: themed("radius", 4) // controls
    readonly property int radiusCard: themed("radiusCard", 6)
    readonly property int radiusOverlay: themed("radiusOverlay", 8) // menus, flyouts, dialogs, panels

    // --- layout of the shell's surfaces (themes/<name>.conf: taskbar-*,
    // startmenu-*, quicksettings-*, notifications-*); defaults are Vela's ---
    readonly property int taskbarBarHeight: themed("taskbar-height", 48)
    readonly property int taskbarMargin: themed("taskbar-margin", 0) // floating: gap to the window's edges
    readonly property int taskbarRadius: themed("taskbar-radius", 0)
    readonly property int taskbarButtonWidth: themed("taskbar-button-width", 44)
    readonly property int taskbarButtonHeight: themed("taskbar-button-height", 40)
    readonly property int taskbarButtonRadius: themed("taskbar-button-radius", radius)
    readonly property int taskbarIconSize: themed("taskbar-icon-size", 26)
    readonly property int taskbarSpacing: themed("taskbar-spacing", 4)
    readonly property bool taskbarFit: themed("taskbar-fit", 0) !== 0 // apps and tray in one centered pill
    readonly property int taskbarSplitGap: themed("taskbar-split-gap", 0) // fit: the tray a pill of its own
    // A floating taskbar under a maximized window: the strip around it blurred
    // and tinted (vela.conf taskbar-backdrop=yes, Settings > Themes).
    readonly property bool taskbarBackdrop: themed("taskbar-backdrop", 0) !== 0
    readonly property color taskbarBackdropColor: themed("taskbar-backdrop-color", Qt.rgba(taskbar.r, taskbar.g, taskbar.b, 0.45))
    // taskbar-edge: apps and tray are two pills, the tray at the screen's right
    // edge (1: apps centered, like DockLike; 2: apps at the left, like
    // LayerMicaUI). taskbar-align-left: a full-width bar with its apps at the
    // left (Windows 7). taskbar-button-fill/-stroke: a tile under each open app.
    // taskbar-side-margin: a floating bar that spans the screen keeps this gap
    // at its left and right (Luminosity's DockMargin); default: taskbar-margin.
    readonly property int taskbarSideMargin: themed("taskbar-side-margin", taskbarMargin)
    readonly property int taskbarEdge: themed("taskbar-edge", 0)
    // taskbar-flush: the apps' pill sits on the screen's bottom edge, its
    // bottom corners square (DockLike); the tray's pill still floats.
    readonly property bool taskbarFlush: themed("taskbar-flush", 0) !== 0
    readonly property bool taskbarAlignLeft: themed("taskbar-align-left", 0) !== 0
    // taskbar-start-glyph: 0 Vela's sail, 1 a glass orb with the four-color flag
    // (Windows 7). taskbar-weather=0 hides the weather widget.
    readonly property int taskbarStartGlyph: themed("taskbar-start-glyph", 0)
    // Pictures of a theme (taskbar-orb=asset:aero/orbNormal.png ...): the Start
    // orb, the tile under an open app (stretched, with a 3 px border), and a
    // reflection over the bar. Empty without them: the drawn look.
    readonly property string taskbarOrb: themed("taskbar-orb", "")
    // taskbar-bounce=0: the icons do not shrink while pressed (Windows 7 had no such motion).
    readonly property bool taskbarBounce: themed("taskbar-bounce", 1) !== 0
    // The button of an app that is open hops once when it is clicked (a click brings its window).
    // taskbar-activate-hop=0 turns it off; by default it follows taskbar-bounce.
    readonly property bool taskbarActivateHop: animationsOn && themed("taskbar-activate-hop", taskbarBounce ? 1 : 0) !== 0
    readonly property string taskbarOrbPressed: themed("taskbar-orb-pressed", taskbarOrb)
    readonly property string taskbarTileImage: themed("taskbar-tile-image", "")
    readonly property string taskbarTileImageActive: themed("taskbar-tile-image-active", taskbarTileImage)
    readonly property string taskbarTileImageHover: themed("taskbar-tile-image-hover", taskbarTileImage)
    readonly property string taskbarTileImageIdleHover: themed("taskbar-tile-image-idle-hover", "") // under the pointer, app not open
    readonly property string taskbarReflection: themed("taskbar-reflection", "")
    readonly property real taskbarReflectionOpacity: themed("taskbar-reflection-opacity", 0.5)
    // taskbar-desktop-image: a strip picture for the show desktop corner (Windows 7's),
    // taskbar-task-view=0 hides the Task view button (Windows 7 had none).
    readonly property string taskbarDesktopImage: themed("taskbar-desktop-image", "")
    readonly property string taskbarDesktopImageHover: themed("taskbar-desktop-image-hover", taskbarDesktopImage)
    readonly property bool taskbarTaskView: themed("taskbar-task-view", 1) !== 0
    readonly property bool taskbarWeather: themed("taskbar-weather", 1) !== 0
    // The tile under an open app: taskbar-button-fill (open) and -fill-active
    // (the focused one) are the tile's lower color, a glossy gradient up to
    // the lighter top if taskbar-button-gloss=1; only open apps get one.
    readonly property color taskbarButtonFillActive: themed("taskbar-button-fill-active", taskbarButtonFill)
    readonly property bool taskbarButtonGloss: themed("taskbar-button-gloss", 0) !== 0
    readonly property color taskbarButtonFill: themed("taskbar-button-fill", "transparent")
    readonly property color taskbarButtonStroke: themed("taskbar-button-stroke", "transparent")
    readonly property int taskbarSplitRadius: themed("taskbar-split-radius", 5) // the corners where they part
    // A style's own panel tint (startmenu/, notifications/): else the surface's.
    readonly property color startMenuSurface: themed("startmenu-surface", surface)
    readonly property color quickSettingsSurface: themed("quicksettings-surface", surface)
    readonly property color notificationsSurface: themed("notifications-surface", surface)
    readonly property int startMenuWidth: themed("startmenu-width", 660)
    readonly property int startMenuHeight: themed("startmenu-height", 732) // at most
    readonly property int startMenuRadius: themed("startmenu-radius", radiusOverlay)
    readonly property bool startMenuSeven: themed("startmenu-layout", 0) === 1 // Windows 7's two columns
    readonly property bool menuSeven: themed("menu-layout", 0) === 1 // Windows 7's right-click menus: light, grey frame, blue highlight
    readonly property int startMenuColumns: themed("startmenu-columns", 6)
    readonly property int startMenuCellHeight: themed("startmenu-cell-height", 96)
    readonly property bool startMenuFooter: themed("startmenu-footer", 1) !== 0
    readonly property int quickSettingsWidth: themed("quicksettings-width", 384)
    readonly property int quickSettingsRadius: themed("quicksettings-radius", radiusOverlay)
    readonly property int quickSettingsColumns: themed("quicksettings-columns", 3)
    readonly property int quickSettingsTileHeight: themed("quicksettings-tile-height", 82)
    readonly property int quickSettingsTileRadius: themed("quicksettings-tile-radius", radius)
    // A split tile's two parts as separate buttons this far apart (0: one button).
    readonly property int quickSettingsSplitGap: themed("quicksettings-split-gap", 0)
    readonly property bool quickSettingsThickSlider: themed("quicksettings-thick-slider", 0) !== 0
    // Held, a slider's knob zooms a little with a bounce (Settings > Themes,
    // quicksettings-slider-press-zoom= in vela.conf).
    readonly property bool quickSettingsSliderPressZoom: themed("quicksettings-slider-press-zoom", 1) !== 0
    readonly property bool quickSettingsMediaCard: themed("quicksettings-media-card", 0) !== 0 // media below, a card of its own
    readonly property bool quickSettingsMediaTop: themed("quicksettings-media-top", 0) !== 0 // the media card above the panel
    readonly property int notificationsWidth: themed("notifications-width", 384)
    readonly property int notificationsRadius: themed("notifications-radius", radiusOverlay)
    readonly property int notificationsCardRadius: themed("notifications-card-radius", radius)
    // The calendar card: a big clock with seconds on top, weekdays on a strip,
    // today's shape (WindowGlass).
    readonly property bool notificationsClock: themed("notifications-clock", 0) !== 0
    readonly property bool notificationsWeekStrip: themed("notifications-week-strip", 0) !== 0
    readonly property int notificationsTodayRadius: themed("notifications-today-radius", 16)
    // Windows 7's glass over the quick settings and the notification center
    // (AeroGlass.qml): lit panel, white inner line, streaks of light.
    readonly property bool notificationsGlass: themed("notifications-glass", 0) !== 0
    readonly property string notificationsReflection: themed("notifications-reflection", "")
    readonly property real notificationsReflectionOpacity: themed("notifications-reflection-opacity", 0.3)
    // The focus timer is on by default; a theme can turn it off with notifications-focus=0.
    readonly property bool notificationsFocus: themed("notifications-focus", 1) !== 0
    // Shadows (Shadow.qml): WinUI's elevations for what floats.
    readonly property var shadow: ({
        flyout: { blur: 32, offset: 6, opacity: 0.45 },
        dialog: { blur: 48, offset: 16, opacity: 0.45 }
    })
    readonly property int fontCaption: 12
    readonly property int fontBody: 14
    readonly property int fontBodyLarge: 18
    readonly property int fontSubtitle: 20
    readonly property int fontTitle: 28

    // --- motion: Fluent-Qt's tokens (src/design/Animation.h, MIT): durations
    // and curves, the cubic ones as their CSS bezier equivalents ---
    // Settings > Personalization > Animations: all of them off, or faster
    // (speed 2: half the time) or slower. Every duration goes through animated().
    // The compositor's scale (compositor/src/motion.h; scripts/check-shared-values.py).
    readonly property var scale: ({ fast: 120, normal: 200, slow: 250, long: 300 })
    readonly property bool animationsOn: themed("animations", 1) !== 0
    readonly property real animationSpeed: Math.max(0.5, Math.min(2, themed("animation-speed", 1)))
    readonly property real motionScale: animationsOn ? 1 / animationSpeed : 0
    function animated(ms) { return Math.round(ms * motionScale) }
    readonly property int fast: animated(150) // instant feedback: clicks, small toggles
    readonly property int normal: animated(250) // expand/collapse, simple enter/exit
    readonly property int slow: animated(400) // page, popup, layout changes
    readonly property int verySlow: animated(700)
    // The selection pill of side menus (NavIndicator); the one of Files is off unless asked for.
    readonly property bool navPillAnimation: animationsOn && themed("nav-pill-animation", 1) !== 0
    readonly property bool filesPillAnimation: navPillAnimation && themed("files-pill-animation", 0) !== 0
    readonly property var decelerate: [0.33, 1.0, 0.68, 1.0, 1.0, 1.0] // OutCubic
    readonly property var accelerate: [0.32, 0.0, 0.67, 0.0, 1.0, 1.0] // InCubic
    readonly property var standard: [0.37, 0.0, 0.63, 1.0, 1.0, 1.0] // InOutSine
    readonly property var exit: [0.64, 0.0, 0.78, 0.0, 1.0, 1.0] // InQuint
    // entrance: Easing.OutBack (Fluent-Qt sets an amplitude, which OutBack ignores)
    readonly property color accentFillPressed: Qt.rgba(accentFill.r, accentFill.g, accentFill.b, 0.8)
    // Every movement is multiplied by `motion` (1 or 0): a panel's offset and
    // scale, a page's slide, a layout change's duration.
    readonly property bool animationEffects: animationsOn && Style.animationEffects
    readonly property real motion: animationEffects ? 1 : 0
    readonly property real duration: animationEffects ? Style.animationDuration : 1
    readonly property int long: animated(300) // what moves across the whole screen
}
