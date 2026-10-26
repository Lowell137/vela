// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VELA_CONFIG_H
#define VELA_CONFIG_H

// Vela's settings that don't concern outputs, in ~/.config/vela/vela.conf:
// key=value lines (# for comments). The Settings app writes them, then sends
// "reload-config" to the compositor.
//
//   screen-off=10            minutes of inactivity (0: never)
//   lock-on-idle=yes         lock before turning off
//   keyboard-layout=it,us    XKB layouts (Win+Space switches to the next)
//   keyboard-variant=,
//   keyboard-options=
//   keyboard-repeat-delay=400  ms before a held key repeats
//   keyboard-repeat-rate=30  repeats per second
//   night-light=no           night light on (quick settings and the schedule
//                            change it too)
//   night-light-strength=48  0-100
//   night-light-nvidia-fix=no  experiment: render Night light in Vulkan instead of DRM gamma
//   night-light-schedule=no|sunset|hours
//   night-light-from=21:00, night-light-to=07:00
//   color-filters=no         color filter on
//   color-filter=grayscale   grayscale, deuteranopia, protanopia, tritanopia
//   color-filters-shortcut=no  Win+Ctrl+C turns it on and off
//   magnifier-step=100       how much Win+plus zooms in (percent)
//   sticky-keys=no           Shift, Ctrl, Alt and Win stay pressed
//   tearing=yes              for fullscreen games that ask for it
//   direct-scanout=auto      auto, yes (the same) or no: never put a fullscreen
//                            app's buffer straight on screen (black screen or
//                            glitches in a game); VELA_SCANOUT=0 wins
//   variable-refresh=games   VRR: no, games (fullscreen apps), always
//   mouse-speed=10           1-20, like the Windows pointer
//   mouse-precision=yes      pointer acceleration
//   mouse-primary-button=left|right
//   mouse-scroll-lines=3     lines per wheel notch
//   middle-click-autoscroll=no  the middle button scrolls like Windows
//                            (autoscroll.h; window-rule autoscroll=no|yes)
//   autoscroll-speed=5       1-10
//   autoscroll-click=yes     a middle click turns it on (no: the click is the app's)
//   autoscroll-hold=yes      held and dragged, it scrolls until the release
//   touchpad=yes, touchpad-with-mouse=yes (stays on with a mouse too)
//   touchpad-speed=10, touchpad-tap=yes, touchpad-natural-scroll=yes
//   touchpad-three-fingers=app  gestures: app, desktop, no
//   touchpad-four-fingers=desktop
//   hot-corner=yes           the top-left corner opens Task View (interact.c)
//   hot-corner-glow=yes      a glow while the pointer is near it
//   hot-corner-animation=yes ripples when it opens Task View
//   capture-indicator=yes    a taskbar indicator while the screen is shared or
//                            recorded (capture_watch.c)
//   capture-dnd=yes          and notifications are silenced meanwhile
//   fullscreen-dnd=yes       notifications are silenced while a window is
//                            fullscreen (capture_watch.c)
//   snap-drag-layouts=yes    dragging a window to the top opens the snap layouts
//                            (snap.c)
//   snap-drag-trigger-distance=64  how close to the top they open (px, 4-200):
//                            the pointer holds the title bar, so the window
//                            reaching the top is enough
//   snap-drag-trigger-width=160  how far beside the panel they open (px, 0-960)
//   snap-drag-peek=yes       while a window is dragged, the panel's lower edge
//                            shows at the top before they open
//   snap-drag-scale=100      their size (percent, 60-200; at most 90% of the
//                            output's width)
//   language=                it, en, or empty (like the system)
//   polkit-agent=yes         start Vela's polkit agent (docs/polkit-agent.md)
//   icon-theme=              icon theme name (empty: KDE's, Breeze); X-dark/X-Dark
//                            is used in dark mode when installed
//   cursor-theme=            cursor theme name (empty: $XCURSOR_THEME); also
//                            exported to the apps Vela starts
//   cursor-size=24           cursor size in pixels (empty: $XCURSOR_SIZE, or 24)
//   window-blur=no           blur behind windows that don't ask for it (view.c)
//   window-blur-all=no       yes: every window; no: only window-blur-include
//   window-blur-include=     app ids, comma-separated
//   window-blur-exclude=     app ids never forced (wins over the other two)
//   window-opacity=100       the forced windows translucent (percent, 30-100;
//                            not fullscreen; title bar too, not the shadow)
//   window-opacity-inactive= the same when not active (empty: window-opacity)
//   window-rule=app|k=v,k=v  repeatable: how a new window of that app opens (see
//                            window_rules.h)
//
// The old Italian names are still read (legacy_names.h).

#include <stdbool.h>
#include <stddef.h>

// The file as read: one entry per key (for repeated keys the last line wins).
// Each entry is a single allocation, "key\0value".
struct vela_config_entry {
    char *key;
    const char *value; // inside the same allocation as key
};

struct vela_config {
    struct vela_config_entry *entries;
    int count;
    int capacity;
};

// Reads vela.conf (a missing file gives an empty list). Free with
// vela_config_finish.
void vela_config_read(struct vela_config *config);
void vela_config_finish(struct vela_config *config);

// The key's value, or `fallback` if it's missing.
const char *vela_config_get(const struct vela_config *config, const char *key, const char *fallback);
// "yes"/"1"/"true"; `fallback` if missing or empty.
bool vela_config_flag(const struct vela_config *config, const char *key, bool fallback);
// Whether a comma-separated list (spaces around items allowed) has `item`,
// ignoring case: X11 classes and app ids differ in case for the same app.
bool vela_config_list_has(const char *list, const char *item);

// Calls `callback` for every line of the file with this key, in order (the
// reader above keeps only the last): for repeatable keys such as window-rule.
void vela_config_each(const char *key, void (*callback)(const char *value, void *data), void *data);

// Changes one key in the file and leaves the rest as it is (comments too): for
// what is also turned on outside Settings (quick settings, shortcuts).
void vela_config_write(const char *key, const char *value);

// Rewrites vela.conf with the English names of keys and values if it still has
// the old Italian ones. At startup.
void vela_config_migrate(void);

// $XDG_CONFIG_HOME/vela/<name> (or ~/.config/vela/<name>); name NULL: the
// directory. false if the directory is unknown or the path doesn't fit in
// `size`.
bool vela_config_path(const char *name, char *out, size_t size);

#endif
