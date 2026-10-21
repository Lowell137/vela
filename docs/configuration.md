# Vela configuration

Most user-facing settings should be changed through **Vela Settings**. This document describes the files and environment variables behind those controls, plus development/debug overrides that are intentionally kept out of the main README.

## Persistent compositor settings

The compositor stores non-display settings in:

```text
~/.config/vela/vela.conf
```

or, when `XDG_CONFIG_HOME` is set:

```text
$XDG_CONFIG_HOME/vela/vela.conf
```

The format is intentionally simple:

```ini
# comments begin with #
key=value
```

Vela Settings preserves unrelated lines when it updates a key and asks the compositor to reload the file. Boolean values are `yes` or `no` (`1` and `true` work too).

### `vela.conf` keys

| Key | Default | Meaning |
|---|---|---|
| `language` | system | Interface language: `it`, `en`, `tr`, or empty to follow the system (Italian or Turkish if the system is, English otherwise). Apps without a translation for the chosen language stay in English. |
| `screen-off` | `10` | Minutes of inactivity before the idle action. `0` disables it. |
| `lock-on-idle` | `yes` | Lock before turning displays off. |
| `terminal` | empty | Default terminal (Super+Enter): a program, empty for the first installed of konsole, foot, kitty, alacritty, gnome-terminal, xterm. Set in Apps > Default apps, whose list comes from the installed .desktop files (TerminalEmulator, or System+Monitor). |
| `task-manager` | empty | Default task manager: a monitor program (or btop/htop/top, opened in the terminal), empty for the first installed. Set in Apps > Default apps, whose list comes from the installed .desktop files (TerminalEmulator, or System+Monitor). |
| `decoration-ssd` | `kitty` | Comma-separated app ids that always get Vela's title bar, even when they never ask through xdg-decoration (kitty never asks). Empty disables it. |
| `polkit-agent` | `yes` | Start Vela's polkit agent (the User Account Control dialog) in the real session. `no` leaves the job to another agent. |
| `keyboard-layout` | system/default | Comma-separated XKB layouts, for example `it,us`. |
| `keyboard-variant` | empty | Comma-separated XKB variants matching `keyboard-layout`. |
| `keyboard-options` | empty | XKB options. |
| `keyboard-repeat-delay` | `400` | Key-repeat delay in milliseconds. |
| `keyboard-repeat-rate` | `30` | Key repeats per second. |
| `night-light` | `no` | Enable Night light. |
| `night-light-strength` | `48` | Night-light strength, 0–100. |
| `night-light-nvidia-fix` | `no` | Experimental NVIDIA workaround: apply Night light in Vulkan instead of the monitor's DRM gamma LUT (may disable direct scanout). |
| `night-light-schedule` | `no` | Night-light schedule: `no`, `sunset` or `hours`. |
| `night-light-from` | `21:00` | Manual Night-light start time. |
| `night-light-to` | `07:00` | Manual Night-light end time. |
| `color-filters` | `no` | Enable the color filter. |
| `color-filter` | `grayscale` | `grayscale`, `deuteranopia`, `protanopia` or `tritanopia`. |
| `color-filters-shortcut` | `no` | Allow Super+Ctrl+C to toggle color filters. |
| `magnifier-step` | `100` | Magnifier step in percent. |
| `sticky-keys` | `no` | Sticky modifier keys. |
| `tearing` | `yes` | Allow fullscreen apps that explicitly request tearing. |
| `direct-scanout` | `auto` | `auto` (`yes` is a compatibility alias): a fullscreen app's buffer goes straight to the screen when the output allows it. `no`: always composite; use it if a game shows a black screen or garbage with the hardware cursor active. Disabling scanout also disables asynchronous tearing (`tearing=yes` only takes effect on the scanout path), and may increase game latency. Applied by `reload-config`; `VELA_SCANOUT=0` takes precedence. Settings > Display > Gaming. |
| `variable-refresh` | `games` | VRR policy: `no`, `games` or `always`. |
| `mouse-speed` | `10` | Pointer speed, 1–20. |
| `mouse-precision` | `yes` | Pointer acceleration / enhanced precision. |
| `mouse-primary-button` | `left` | Primary mouse button: `left` or `right`. |
| `mouse-scroll-lines` | `3` | Scroll lines per wheel notch. |
| `middle-click-autoscroll` | `no` | Windows-style autoscroll: the middle button on a window (not the title bar, borders or shell) starts it, and the click doesn't reach the app (no paste, no new tab). Move the pointer away from the starting point to scroll that way, vertically and horizontally, faster the farther it goes (nothing within 12 px). A short click leaves it on until any button or Esc; pressed, dragged and released, it ends with the release (see the next two keys). It also ends on a focus change, the window closing, lock, Alt+Tab, Task View, a pointer constraint (games) or the output going away. The cursor is the theme's `all-scroll`. Terminals (`konsole`, `org.kde.konsole`, `foot`, `footclient`, `kitty`, `alacritty`, `vela-terminal`, `xterm`) are left out, so the middle button pastes there, unless a window rule says `autoscroll=yes`. Settings > Bluetooth & devices > Mouse. |
| `autoscroll-click` | `yes` | With `middle-click-autoscroll=yes`: a middle click (press and release) turns autoscroll on until the next click or Esc. `no`: a short middle click reaches the app (paste, new tab) and only `autoscroll-hold` scrolls; the press is held back until the pointer moves 4 px or 200 ms pass (then it is a held autoscroll), and if released earlier press and release reach the app together. |
| `autoscroll-hold` | `yes` | With `middle-click-autoscroll=yes`: pressing the middle button and dragging scrolls until the release. `no`: the release doesn't end it. With both `autoscroll-click` and `autoscroll-hold` off, the middle button is always the app's. |
| `autoscroll-speed` | `5` | Autoscroll speed, 1–10: each step is 25% faster. Per axis, `1.5 × 1.25^(speed−5) × (distance − 12)^1.3` px/s, at most 12000 px/s. |
| `touchpad` | `yes` | Enable touchpad input. |
| `touchpad-with-mouse` | `yes` | Keep the touchpad enabled when a mouse is connected. |
| `touchpad-speed` | `10` | Touchpad pointer speed, 1–20. |
| `touchpad-tap` | `yes` | Tap to click. |
| `touchpad-natural-scroll` | `yes` | Natural scrolling. |
| `touchpad-three-fingers` | `app` | Three-finger gesture mode: `app`, `desktop` or `no`. |
| `touchpad-four-fingers` | `desktop` | Four-finger gesture mode. |
| `capture-indicator` | `yes` | Show an indicator in the taskbar while the screen is shared or recorded (Settings > System > Display > Screen sharing). `no` also disables `capture-dnd`. |
| `capture-dnd` | `yes` | Silence notification popups while the screen is shared or recorded; they wait in the notification center. Added to the user's own Do Not Disturb, never replaces it (Settings > System > Notifications). |
| `fullscreen-dnd` | `yes` | Silence notification popups while a visible window is full screen on any output (games, video players); they wait in the notification center. Added to the user's own Do Not Disturb, never replaces it (Settings > System > Notifications). |
| `window-blur` | `no` | Blur behind windows that don't ask for it (Settings > Personalization > Window blur). |
| `window-blur-all` | `no` | `yes`: every window; `no`: only the `window-blur-include` apps. |
| `window-blur-include` | empty | Comma-separated app ids (or X11 classes) to blur, case-insensitive. |
| `window-blur-exclude` | empty | App ids never forced; wins over the other two. |
| `window-opacity` | `100` | The same windows drawn translucent, in percent (30–100), so the blur shows through them; title bar included, shadow and fullscreen windows not. `100` changes nothing. |
| `window-opacity-inactive` | `window-opacity` | The same for windows that aren't active. |
| `window-blur-strength` | `100` | How strong the window blur is, in percent (50–300) of the usual radius. |
| `window-blur-app` | none | Repeatable, `app-id\|strength=N,opacity=N` (either may be left out; `0`/missing: the general value). An app listed here is blurred like an included one, with its own strength (50–300) and opacity (30–100), unless excluded. |
| `taskbar-opacity`, `quicksettings-opacity` | `100` | The taskbar's and the quick settings panel's fill and acrylic tint, in percent of the theme's (0–200): lower is more see-through. |
| `taskbar-blur`, `quicksettings-blur` | `100` | The same two panels' blur strength, in percent of the theme's (50–300). |
| `remember-window-positions` | `yes` | An app's window reopens where it was closed: position, size, output, desktop and maximized, kept per app id in `~/.local/state/vela/window-positions.json` (`$XDG_STATE_HOME/vela/`), last closed window wins. A `window-rule` wins over it; dialogs, fullscreen windows, windows under 100 px and X11 windows that place themselves are left alone. If the output is gone, the window opens on the usual output, kept inside it. Session restore (reopening apps after login) is not part of this. |

## Taskbar settings

The shell's own settings (`~/.config/Vela/vela-shell.conf`, Qt's INI format) are written by Settings > Personalization > Taskbar and reread by the shell as soon as the file changes. The taskbar ones, in the `[taskbar]` group:

| Key | Default | Effect |
|---|---|---|
| `position` | `bottom` | The screen edge the taskbar sits on: `bottom`, `top`, `left` or `right`. At the sides it is 48 px wide, the Start button and apps go from the top, the clock (time, and day and month) at the bottom, and the buttons are dragged up and down. Start, quick settings, the notification center, menus, previews and tooltips open next to it; the hot corner stays at the top-left corner (only its 2 px corner fires, so a Start button there still works). Windows, snaps and Task View use the output minus the taskbar's side. The shell tells the compositor with `taskbar-position top\|bottom\|left\|right`. |
| `autoHide` | `false` | Hide the taskbar automatically: it slides out of its edge (down, up, or sideways) leaving 2 px at that edge of the screen, comes back when the pointer touches them, and goes away 0.5 s after the pointer leaves. It stays while the Start menu, a menu, a flyout or Task View is open. Maximized and snapped windows use the whole output; panels still sit next to the taskbar's reserved space. The shell tells the compositor with `taskbar-autohide yes\|no`. |
| `combine` | `always` | `always`: one button per app; `never`: one button per window (the app's first window keeps the pinned button, the others get their own). |
| `weather` | `on` | `on` shows the weather in the taskbar (icon and temperature, next to the tray; a click opens a flyout with feels-like, humidity, wind, the next 12 hours and 5 days). Off, nothing is requested from the network (the fork turns it on by default). Data comes from Open-Meteo (no API key) every 30 minutes and is cached in `~/.cache/Vela/vela-shell/weather.json`; without a network the last values show as out of date. |
| `weather-city` | empty | The place's name, only for display (the flyout title). |
| `weather-lat`, `weather-lon` | empty | The coordinates the forecast is for. Written by Settings > Personalization > Taskbar > Weather after a city search (geocoding by Open-Meteo, only while typing there). With no coordinates, the place is guessed once per session from the IP address (a request to `ipwho.is`, only city and coordinates are asked for, nothing is saved); a chosen city always wins. Offline, the widget stays hidden until the next try. Turn the widget off to stop all requests. |
| `weather-unit` | `c` | `c` (°C, km/h) or `f` (°F, mph). |

The taskbar's window previews have a media strip under a window whose app plays through MPRIS (Spotify, a browser's video, VLC, mpv...): the track, previous, play/pause and next. It is there while the player plays, or is paused with a track, and not otherwise (no room is left for it). The player is matched to the window by its `DesktopEntry` against the app id, then by its bus name (`org.mpris.MediaPlayer2.<name>[.instanceN]`) against the app id; when an app has several windows (browsers), only the one whose title holds the track's gets the strip. There is no setting.

## Window rules

`window-rule` is the one repeatable key: one line per rule, in `vela.conf` (Settings > Personalization > Window rules writes them).

```text
window-rule=<app-id>|<key>=<value>,<key>=<value>
```

```text
window-rule=discord|output=HDMI-A-1,desktop=2
window-rule=kitty|size=900x600,center=yes
window-rule=mpv|always-on-top=yes
window-rule=org.keepassxc.KeePassXC|capture=no
window-rule=firefox|title~=Private Browsing,capture=no
```

The app id (or X11 class) matches ignoring case; the first matching rule wins. Keys:

| Key | Meaning |
| --- | --- |
| `title~=text` | Only windows whose title contains `text` (ignoring case), checked when the window opens. |
| `output=NAME` | The output's name (`HDMI-A-1`) or its 1-based number. An unknown output is ignored. |
| `desktop=N` | Virtual desktop, 1-based. The window opens there without switching to it (and without taking focus); ignored if the desktop doesn't exist. |
| `position=X,Y` | Frame position from the corner of the output's usable area, kept inside it. |
| `size=WxH` | Size of the whole frame, title bar included; never larger than the usable area. |
| `center=yes` | Centered on the output (wins over `position`). |
| `maximized=yes` | Opens maximized on the output. |
| `fullscreen=no` | A window that asks to open fullscreen opens as a normal window. |
| `always-on-top=yes` | Starts always on top (not kept when the window closes). |
| `autoscroll=no` | No middle-click autoscroll for this app (the middle button reaches it as usual); `autoscroll=yes` turns it on for a terminal, which is left out by default. Applies at once on `reload-config`, also to open windows. Only in `vela.conf`: the Settings rule dialog doesn't offer it. |
| `capture=no` | Hidden from screen capture: the window stays on screen, but screenshots, recordings and screen sharing (and the Snipping Tool) show what lies under it; its popups, dialogs and child windows too. Default `yes`. See [renderer §15](renderer.md#15-hiding-a-window-from-screen-capture) for what it does and doesn't cover. |

Unknown keys and unusable values are ignored and logged. Rules apply once, when a window opens: you can move it afterwards. `reload-config` (and Settings) change the rules for windows opened from then on, not for windows already open (`capture` and `autoscroll` excepted: they apply to open windows too).

The Add a rule dialog in Settings has a "Pick a window" button: it sends `pick-window` on the command socket, the pointer becomes a crosshair, and the next left click on a window (the click reaches nobody) answers with its app id, title and output as one JSON line. Esc, a right click or 30 seconds without a click answer `cancel`; panels and the desktop are ignored.

The exception is `capture=no`, a privacy rule: it is checked again on `reload-config` for the windows already open (adding the rule hides them, removing it shows them again) and whenever a window's title changes (so `title~=` follows the title). The window menu's "Hide from screen capture" (title bar, Alt+Space, Task View) overrides the rule for that window until it closes. The command socket accepts `window <id|active> capture-hide on|off|toggle`.

The Settings app is the preferred way to edit these values because it applies validation and reloads the compositor at the correct time.

## Display configuration

Persistent display layout is stored separately in:

```text
~/.config/vela/outputs.conf
```

Each physical display gets its own section identified from make, model and serial when available:

```ini
[Vendor Model Serial]
enabled=yes
mode=2560x1440@180.000
scale=1.25
rotation=normal
position=0,0
```

Supported rotation names are `normal`, `90`, `180`, `270`, `flipped`, `flipped-90`, `flipped-180` and `flipped-270`.

Files written before October 2026 used Italian names (`schermi.conf`, and Italian keys and values in `vela.conf`). Vela still reads them and rewrites them with the English names.

Normally this file should be managed through **Settings → Displays** rather than edited by hand.

Vela keeps the *relative* monitor arrangement while shifting its global layout
so the upper-left corner of the enabled display group is always at (0,0).
For example, a 1920×1080 monitor physically **above** a 2048×1152 primary
will appear as (0,0) and (0,1080) respectively. This avoids exposing
negative monitor coordinates to XWayland/Wine while leaving the perceived
screen arrangement unchanged. Existing saved configurations with negative
positions are normalized when loaded; new positions are saved normalized.
Windows on a translated output move with it, including during Settings
preview/revert.

Settings talks directly to `wlr-output-management`; no external display utility
is required. Changes are previewed for 15 seconds. Vela's compositor keeps the
previous configuration and restores it when the deadline expires, Settings
exits or crashes, or the user chooses **Revert**. The countdown does not depend
on Settings remaining responsive. Only **Keep changes** attempts to write the new settings
to `outputs.conf`.

The preview uses `protocols/vela-output-preview-v1.xml`, bound on the same
Wayland connection as the output configuration. While a preview is pending,
competing display changes are rejected. If monitors are unplugged, restoration
covers the surviving original outputs and leaves newly connected outputs alone.
If a confirmed change cannot be saved, the active display configuration is
kept, but Settings warns that it may be lost after a restart. A failed apply
whose rollback also fails is reported distinctly.
Settings declines to apply changes on compositors that lack the preview extension.
Ordinary output-management clients retain their immediate-apply behavior when
no preview is pending. Backend failures are reported; restoration cannot guarantee
recovery from a hardware failure that also rejects the previous configuration.

## Runtime environment variables

These are useful for one-off overrides, nested sessions, experiments and troubleshooting.

### Display and input

| Variable | Effect |
|---|---|
| `VELA_SCALE` | Force output scale. A single value applies to all outputs (`1.25`); per-output values can be supplied as `DP-1=1.5,HDMI-A-1=1`. Without it, Vela chooses a scale from each monitor's DPI. |
| `VELA_OUTPUT_SIZE` | Resolution/refresh for nested or headless outputs, for example `1920x1080@144`. |
| `VELA_NATURAL_SCROLL=0` | Force classic touchpad scrolling; this override wins over the stored natural-scrolling setting. |
| `XKB_DEFAULT_LAYOUT` | Initial keyboard layout, for example `it`. |
| `VELA_TERMINAL` | Terminal launched by Super+Enter. Default: `konsole`. |

### Presentation, VRR and power

| Variable | Effect |
|---|---|
| `VELA_VRR=1` | Force VRR always on where supported. |
| `VELA_VRR=0` | Force VRR off. |
| `VELA_TEARING=0` | Disable tearing even for applications that request it. |
| `VELA_SCANOUT=0` | Disable direct scanout. Useful for A/B comparisons and debugging. |
| `VELA_READY_WAIT=0` | Apply app commits immediately even if the app's GPU has not finished drawing the buffer, so frames wait for it. Only for comparison: by default commits wait for their fences and the screen never waits for an app. |
| `VELA_LATCH=0` | Disable late latching and render when the backend frame event arrives. |
| `VELA_LATCH_MARGIN` | Minimum late-latching safety margin in milliseconds. Default: `1`; the frame clock can grow the effective margin after late frames. |
| `VELA_REALTIME=0` | Disable realtime scheduling for the compositor main thread. |
| `VELA_SCREEN_OFF` | Override idle timeout in minutes (fractions allowed). Default is 10 if no stored setting exists; `0` disables idle screen-off. Set, it also enables the idle timeout nested and headless (tests). |
| `VELA_LOCK_ON_IDLE=0` | Turn displays off on idle without locking first. |
| `VELA_LOCK` | Use another locker instead of `vela-lock`, for example `swaylock`. |
| `VELA_POLKIT_AGENT` | Start this polkit agent instead of `vela-polkit-agent`; `0` starts none. Without it, Vela's agent starts only in a real (DRM) session, never nested or headless. |
| `VELA_POLKIT_PROMPT` | For `vela-polkit-agent`: the dialog to run instead of `vela-polkit-prompt`. |

### Appearance and shell integration

| Variable | Effect |
|---|---|
| `VELA_WALLPAPER` | Override the wallpaper image. SVG, PNG, JPEG and other supported formats can be used. |
| `VELA_ICON_THEME` | Fallback icon theme when Qt cannot resolve one. Default: `breeze-dark`. |

## Development and renderer diagnostics

These variables are for diagnosis rather than normal user configuration.

| Variable | Effect |
|---|---|
| `VELA_STATS=1` | Print per-output FPS, frame cost, draw-to-light latency and missed vblanks every two seconds. |
| `VELA_VULKAN_VALIDATION=1` | Enable Vulkan validation layers. |
| `VELA_DEBUG=1` | Enable verbose wlroots logging. |
| `VELA_DEBUG_SCANOUT=1` | Log why a fullscreen surface is not eligible for direct scanout. |
| `VELA_DEBUG_DAMAGE=1` | Force full-output damage every frame, bypassing normal damage reduction for diagnosis. |
| `VELA_DEBUG_SYNC=1` | Make the CPU wait for the GPU on every renderer submission; useful when isolating synchronization problems. |
| `VELA_DEBUG_LINEAR=1` | Force linear render buffers instead of normal tiled/modifier allocation; useful for allocator/modifier diagnosis. |
| `VELA_DEBUG_INPUT=1` | Enable the virtual keyboard/pointer protocol used by `vela-input`. Off by default for security. |
| `VELA_FILES_TIMING=1` | Make `vela-files` print startup timing data; used by `scripts/measure.sh`. |
| `VELA_SCREEN_DUMP_OUTPUT` | Select which output `vela-screen-dump` captures by zero-based index. |

## Internal variables

The supervisor, session launcher and test harness also use internal variables such as `VELA_WAYLAND_SOCKET_FD`, `VELA_WAYLAND_DISPLAY`, `VELA_START_LOCKED`, `VELA_RESTARTED`, `VELA_BUILD` and `VELA_TEST_MARK`.

They are implementation details, are not intended as stable user configuration, and may change without notice.

## Examples

Run Vela nested at 1440p/180 Hz with 125% scale:

```sh
VELA_OUTPUT_SIZE=2560x1440@180 VELA_SCALE=1.25 sh scripts/run-nested.sh
```

Compare composited fullscreen rendering against direct scanout:

```sh
VELA_SCANOUT=0 VELA_DEBUG_SCANOUT=1 sh scripts/run-nested.sh
```

Run with Vulkan validation and renderer statistics:

```sh
VELA_VULKAN_VALIDATION=1 VELA_STATS=1 sh scripts/run-nested.sh
```

Force full damage while diagnosing rendering artifacts:

```sh
VELA_DEBUG_DAMAGE=1 sh scripts/run-nested.sh
```
