// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VELA_SHORTCUTS_H
#define VELA_SHORTCUTS_H

// User shortcuts, from vela.conf. They add to the built-in ones (bindings.c)
// and take the place of the built-in key of an action they remap.
//
//   shortcut-close=Super+Q          remaps a built-in action
//   shortcut-close=none             turns the built-in key of the action off
//   shortcut-custom-1=Super+T|foot  runs a command (/bin/sh -c)
//
// Actions: close, terminal, lock, start, winx, run, show-desktop,
// quick-settings, notification-center, settings, files, task-view, snip, snip-text,
// clipboard, sound-output.
//
// A combination is modifiers and a key joined with "+": Super (Win, Meta),
// Alt, Ctrl, Shift, then an XKB key name (Q, Return, F4, Left...).

#include <stdbool.h>
#include <stdint.h>

struct vela_server;

// (Re)reads the shortcuts from vela.conf. At startup and on "reload-config".
void vela_shortcuts_load(void);

// Runs the user shortcut for this key, if there is one.
bool vela_shortcuts_dispatch(struct vela_server *server, uint32_t modifiers, uint32_t sym);

// The top-left corner opens Task View (interact.c); "hot-corner=no" turns it off.
bool vela_hot_corner_enabled(void);
// What the shell shows of it: the glow while near ("hot-corner-glow=no") and
// the ripples when it fires ("hot-corner-animation=no").
bool vela_hot_corner_glow_enabled(void);
bool vela_hot_corner_animation_enabled(void);

// Dragging a window to the top of an output opens the snap layouts (snap.c);
// "snap-drag-layouts=no" turns it off.
bool vela_snap_drag_layouts_enabled(void);

// false if the user remapped (or turned off) the built-in key of this action.
bool vela_shortcuts_default_enabled(const char *action);

// "Super+Shift+S" to modifiers (WLR_MODIFIER_*) and a lowercase keysym.
bool vela_shortcut_parse(const char *text, uint32_t *modifiers, uint32_t *sym);

#endif
