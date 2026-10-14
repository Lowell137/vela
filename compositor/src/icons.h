// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VELA_ICONS_H
#define VELA_ICONS_H

// App icons in the title bar (docs/renderer.md §9.6): from the app's .desktop
// file (Icon=) to KDE's icon theme (freedesktop spec), drawn at the exact
// physical size. SVGs through librsvg, always preferred; PNGs only when
// nothing else exists, scaled down with a quality filter. Without librsvg
// (optional) there are no icons.

#include "image.h"

#include <stdbool.h>

struct vela_icons;

// The theme (icon-theme= of vela.conf, matched to the apps' mode `light`, or
// KDE's) and its inheritance chain; .desktop files are read on the first
// request.
struct vela_icons *vela_icons_create(bool light);
void vela_icons_destroy(struct vela_icons *icons);
// The theme in use (the first of the chain).
const char *vela_icons_theme(const struct vela_icons *icons);

// The app's icon (Wayland app_id or X11 class), size x size pixels. Never
// NULL: an empty image (pixels NULL) when none is found. It stays valid, owned
// by `icons`, as long as `icons` exists.
const struct vela_image *vela_icons_app(struct vela_icons *icons, const char *app_id, int size);

#endif
