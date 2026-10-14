// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VELA_WINDOW_RULES_H
#define VELA_WINDOW_RULES_H

// Window rules: how a new window of an app opens. In vela.conf, repeatable:
//
//   window-rule=<app-id>|<key>=<value>,<key>=<value>
//
// The app id matches ignoring case. The first rule that matches wins. Keys:
//   title~=text    only windows whose title contains it (ignoring case)
//   output=NAME    an output's name (HDMI-A-1) or its number (1-based)
//   desktop=N      virtual desktop, 1-based
//   position=X,Y   from the corner of the output's usable area
//   size=WxH       the whole frame (title bar included)
//   center=yes     centered on the output (wins over position)
//   maximized=yes  opens maximized
//   fullscreen=no  opens as a normal window even if it asks for fullscreen
//   always-on-top=yes
//   capture=no     hidden from screen capture (docs/renderer.md §15)
//   autoscroll=no  no middle-click autoscroll (autoscroll.h); yes turns it on
//                  for a terminal too
// Unknown keys are logged and ignored. Applied once, when the window maps;
// capture= and autoscroll= also when the title changes and on reload-config, to the windows
// already open (a privacy rule must not wait for the next window).

#include <stdbool.h>

struct vela_window_rule {
    char *app;
    char *title; // substring, or NULL
    char *output; // or NULL
    int desktop; // 0: not set
    bool has_position, has_size;
    int x, y, width, height;
    // -1: not set
    int center, maximized, fullscreen, always_on_top, capture, autoscroll;
};

struct vela_window_rules {
    struct vela_window_rule *items;
    int count;
};

// Parses one window-rule value; false (and nothing added) if it is unusable.
bool vela_window_rules_add(struct vela_window_rules *rules, const char *value);
// Reads every window-rule line of vela.conf (replacing `rules`).
void vela_window_rules_load(struct vela_window_rules *rules);
void vela_window_rules_free(struct vela_window_rules *rules);
// The first rule for this window, or NULL.
const struct vela_window_rule *vela_window_rules_find(const struct vela_window_rules *rules, const char *app_id,
    const char *title);

#endif
