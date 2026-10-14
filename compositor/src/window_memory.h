// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VELA_WINDOW_MEMORY_H
#define VELA_WINDOW_MEMORY_H

// Where each app's window was when it closed, so it reopens there (vela.conf:
// remember-window-positions=yes|no). One entry per app id (the last window to
// close wins), kept in $XDG_STATE_HOME/vela/window-positions.json (or
// ~/.local/state/vela/). Window rules win over it; see vela_view_mapped.
// Not session restore: nothing is reopened.

#include <stdbool.h>

struct vela_window_memory_entry {
    char *app;
    char output[64]; // output name
    int x, y; // the frame, from the corner of that output's usable area
    int width, height;
    bool maximized; // x, y, width and height are then the restore box
    int desktop; // 0-based
};

struct vela_window_memory {
    struct vela_window_memory_entry *items;
    int count;
    bool loaded;
};

// The entry for this app (ignoring case), or NULL. Reads the file on first use.
const struct vela_window_memory_entry *vela_window_memory_find(struct vela_window_memory *memory, const char *app_id);
// Replaces the app's entry and rewrites the file (atomically).
void vela_window_memory_set(struct vela_window_memory *memory, const char *app_id,
    const struct vela_window_memory_entry *entry);
void vela_window_memory_free(struct vela_window_memory *memory);

#endif
