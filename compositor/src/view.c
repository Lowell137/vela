// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "view.h"

#include "capture_watch.h"
#include "config.h"
#include "interact.h"
#include "focus.h"
#include "bindings.h"
#include "decoration.h"
#include "layer.h"
#include "listen.h"
#include "output.h"
#include "popup.h"
#include "scene/capture.h"
#include "scene/frame.h"
#include "scene/scene.h"
#include "server.h"
#include "snapshot.h"
#include "util.h"
#include "workspace.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/config.h>
#include <wlr/interfaces/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>
#if WLR_HAS_XWAYLAND
#include <wlr/xwayland.h>
#include <xcb/xcb_icccm.h>
#endif

#define CORNER_RADIUS 8.0 // logical, like Windows 11

// ------------------------------------------------------------ to the app --

struct wlr_surface *vela_view_surface(const struct vela_view *view)
{
#if WLR_HAS_XWAYLAND
    if (view->x11) {
        return view->x11->surface;
    }
#endif
    return view->xdg->base->surface;
}

int vela_view_title_bar_height(const struct vela_view *view)
{
    return view->decoration && !view->fullscreen ? VELA_DECORATION_HEIGHT : 0;
}

struct wlr_box vela_view_geometry(const struct vela_view *view)
{
    int bar = vela_view_title_bar_height(view);
    if (view->xdg) {
        // With Vela's bar the frame starts above the surface.
        struct wlr_box g = view->xdg->base->geometry;
        return (struct wlr_box) { g.x, g.y - bar, g.width, g.height + bar };
    }
#if WLR_HAS_XWAYLAND
    // X11: no shadow margins; Vela's bar, if any, sits on top.
    return (struct wlr_box) { 0, -bar, view->x11->width, view->x11->height + bar };
#else
    return (struct wlr_box) { 0 };
#endif
}

bool vela_view_configurable(const struct vela_view *view)
{
    if (view->xdg) {
        return view->xdg->base->initialized;
    }
#if WLR_HAS_XWAYLAND
    return view->x11->surface != NULL;
#else
    return false;
#endif
}

const char *vela_view_title(const struct vela_view *view)
{
    const char *title = view->xdg ? view->xdg->title : NULL;
#if WLR_HAS_XWAYLAND
    if (view->x11) {
        title = view->x11->title;
    }
#endif
    return title ? title : "";
}

const char *vela_view_app_id(const struct vela_view *view)
{
    // X11: the window class (WM_CLASS), which serves as app id.
    const char *id = view->xdg ? view->xdg->app_id : NULL;
#if WLR_HAS_XWAYLAND
    if (view->x11) {
        id = view->x11->class;
    }
#endif
    return id ? id : "";
}

void vela_view_set_always_on_top(struct vela_view *view, bool on)
{
    view->always_on_top = on;
    if (on) {
        vela_views_raise_always_on_top(view->server);
    }
}

void vela_views_raise_always_on_top(struct vela_server *server)
{
    // server->views is most recent first: raise from the least recent, so
    // they keep their relative order. Fullscreen ones are in another layer.
    struct vela_view *view;
    wl_list_for_each_reverse (view, &server->views, link) {
        if (view->always_on_top && view->mapped && !view->fullscreen) {
            vela_node_raise_to_top(&view->tree->node);
        }
    }
}

bool vela_view_is_system_prompt(const struct vela_view *view)
{
    // By app id (Wayland) or class (X11): KWallet and ksecretd, KDE's polkit
    // agent, pinentry, GNOME's keyring prompt.
    const char *app = vela_view_app_id(view);
    char id[256];
    size_t n = 0;
    for (; app[n] && n + 1 < sizeof(id); ++n) {
        id[n] = (char)tolower((unsigned char)app[n]);
    }
    id[n] = '\0';
    const char *known[] = {
        "kwalletd", "ksecretd", "polkit-kde-authentication-agent", "pinentry", "gcr-prompter", "systemprompter",
    };
    for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); ++i) {
        if (strstr(id, known[i])) {
            return true;
        }
    }
    return false;
}

static struct vela_view *view_parent(const struct vela_view *view)
{
    if (view->xdg) {
        return view->xdg->parent ? view->xdg->parent->base->data : NULL;
    }
#if WLR_HAS_XWAYLAND
    return view->x11->parent ? view->x11->parent->data : NULL;
#else
    return NULL;
#endif
}

void vela_view_configure_size(struct vela_view *view, int width, int height)
{
    // Sizes are of the whole frame: the app gets the part below Vela's bar.
    int app_height = height > 0 ? vela_max(1, height - vela_view_title_bar_height(view)) : 0;
    if (view->xdg) {
        wlr_xdg_toplevel_set_size(view->xdg, width, app_height);
        return;
    }
    view->x11_width = width;
    view->x11_height = app_height;
    vela_view_sync_x11(view);
}

void vela_view_send_maximized(struct vela_view *view, bool on)
{
    if (view->xdg) {
        wlr_xdg_toplevel_set_maximized(view->xdg, on);
        return;
    }
#if WLR_HAS_XWAYLAND
    wlr_xwayland_surface_set_maximized(view->x11, on, on);
#endif
}

static void send_fullscreen(struct vela_view *view, bool on)
{
    if (view->xdg) {
        wlr_xdg_toplevel_set_fullscreen(view->xdg, on);
        return;
    }
#if WLR_HAS_XWAYLAND
    wlr_xwayland_surface_set_fullscreen(view->x11, on);
#endif
}

void vela_view_send_tiled(struct vela_view *view, uint32_t edges)
{
    if (view->xdg) {
        wlr_xdg_toplevel_set_tiled(view->xdg, edges);
    }
}

static void send_activated(struct vela_view *view, bool on)
{
    if (view->xdg) {
        if (view->xdg->base->initialized) {
            wlr_xdg_toplevel_set_activated(view->xdg, on);
        }
        return;
    }
#if WLR_HAS_XWAYLAND
    if (view->x11->surface) {
        wlr_xwayland_surface_activate(view->x11, on);
        if (on) {
            wlr_xwayland_surface_restack(view->x11, NULL, XCB_STACK_MODE_ABOVE);
        }
    }
#endif
}

void vela_view_close(struct vela_view *view, const char *why)
{
    wlr_log(WLR_INFO, "Close requested (%s): %s", why, vela_view_app_id(view));
    if (view->xdg) {
        wlr_xdg_toplevel_send_close(view->xdg);
        return;
    }
#if WLR_HAS_XWAYLAND
    wlr_xwayland_surface_close(view->x11);
#endif
}

bool vela_view_resizable(const struct vela_view *view)
{
    if (view->xdg) {
        const struct wlr_xdg_toplevel_state *state = &view->xdg->current;
        bool fixed_width = state->max_width > 0 && state->min_width == state->max_width;
        bool fixed_height = state->max_height > 0 && state->min_height == state->max_height;
        return !(fixed_width && fixed_height);
    }
#if WLR_HAS_XWAYLAND
    if (view->x11 && view->x11->size_hints) {
        const xcb_size_hints_t *hints = view->x11->size_hints;
        bool has_min = hints->flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE;
        bool has_max = hints->flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE;
        return !(has_min && has_max && hints->min_width == hints->max_width
            && hints->min_height == hints->max_height);
    }
#endif
    return true;
}

pid_t vela_view_pid(const struct vela_view *view)
{
#if WLR_HAS_XWAYLAND
    if (view->x11) {
        return view->x11->pid;
    }
#endif
    if (view->xdg && view->xdg->resource) {
        pid_t pid = 0;
        wl_client_get_credentials(wl_resource_get_client(view->xdg->resource), &pid, NULL, NULL);
        return pid;
    }
    return 0;
}

// -------------------------------------------------------------- geometry --

struct wlr_box vela_view_frame_box(struct vela_view *view)
{
    struct wlr_box geometry = vela_view_geometry(view);
    return (struct wlr_box) {
        (int)lround(view->tree->node.x) + geometry.x,
        (int)lround(view->tree->node.y) + geometry.y,
        geometry.width,
        geometry.height,
    };
}

struct vela_output *vela_view_output(struct vela_view *view)
{
    struct wlr_box frame = vela_view_frame_box(view);
    struct vela_output *output = vela_output_at(view->server, frame.x + frame.width / 2.0, frame.y + frame.height / 2.0);
    return output ? output : vela_output_under_cursor(view->server);
}

struct wlr_box vela_view_minimize_target(struct vela_view *view)
{
    if (view->taskbar_rect.width > 0) {
        return view->taskbar_rect;
    }
    // Unknown button: the center of the taskbar's edge.
    struct vela_output *output = vela_view_output(view);
    struct wlr_box area = output ? vela_output_box(output) : (struct wlr_box) { 0, 0, 1920, 1080 };
    int x = area.x + area.width / 2 - 24;
    int y = area.y + area.height - 48;
    switch (output ? output->server->taskbar_edge : VELA_TASKBAR_BOTTOM) {
    case VELA_TASKBAR_TOP:
        y = area.y;
        break;
    case VELA_TASKBAR_LEFT:
    case VELA_TASKBAR_RIGHT:
        x = output->server->taskbar_edge == VELA_TASKBAR_LEFT ? area.x : area.x + area.width - 48;
        y = area.y + area.height / 2 - 24;
        break;
    default:
        break;
    }
    return (struct wlr_box) { x, y, 48, 48 };
}

// Where the window returns when it stops being maximized or fullscreen. If it
// appeared that way (many apps remember their state), there is no saved
// position: the size the X11 app asked for is used, otherwise two thirds of
// the output, centered.
struct wlr_box vela_view_restore_box(struct vela_view *view)
{
    if (view->restore.width > 0 && view->restore.height > 0) {
        return view->restore;
    }
    struct vela_output *output = vela_view_output(view);
    if (!output) {
        return view->restore;
    }
    const struct wlr_box area = output->usable;
    struct wlr_box frame = { 0, 0, area.width * 2 / 3, area.height * 2 / 3 };
    // A game opened directly in fullscreen may report its monitor-sized
    // geometry as the "initial" X11 size. Restoring that would recreate the
    // same oversized window instead of making a usable normal window.
    struct wlr_box full = vela_output_box(output);
    bool initial_fullscreen = view->x11 && view->x11_initial.width >= full.width
        && view->x11_initial.height >= full.height;
    if (view->x11 && !initial_fullscreen && view->x11_initial.width > 0
        && view->x11_initial.height > 0) {
        frame.width = view->x11_initial.width;
        frame.height = view->x11_initial.height + (view->decoration ? VELA_DECORATION_HEIGHT : 0);
    }
    frame = vela_output_fit(output, frame);
    frame.x = area.x + (area.width - frame.width) / 2;
    frame.y = area.y + (area.height - frame.height) / 2;
    return frame;
}

void vela_view_keep_in_place(struct vela_view *view)
{
    // Maximized or fullscreen windows can change their shadow margin
    // (geometry.x/y) when redrawing: we keep them aligned.
    if (view->animating || (!view->maximized && !view->fullscreen && vela_snap_is_none(view->snap))) {
        return;
    }
    struct vela_output *output = vela_view_output(view);
    if (!output) {
        return;
    }
    struct vela_area area = view->fullscreen ? vela_output_full_area(output)
        : view->maximized                    ? vela_output_usable_area(output)
                                             : vela_snap_area(output, view->snap);
    struct vela_placement place = vela_output_place(output, area);
    struct wlr_box geometry = vela_view_geometry(view);
    vela_node_set_position(&view->tree->node, place.x - geometry.x, place.y - geometry.y);
}

// --------------------------------------------------------------- Vela's bar --

static struct vela_decoration_state decoration_state(struct vela_view *view)
{
    struct vela_output *output = vela_view_output(view);
    return (struct vela_decoration_state) {
        .geometry = vela_view_geometry(view),
        .scale = output ? output->wlr->scale : 1.0f,
        .title = vela_view_title(view),
        .app_id = vela_view_app_id(view),
        .active = view->activated,
        .maximized = view->maximized,
        .fullscreen = view->fullscreen,
    };
}

void vela_view_refresh_decoration(struct vela_view *view)
{
    if (view->decoration) {
        struct vela_decoration_state state = decoration_state(view);
        vela_decoration_update(view->decoration, &state);
    }
}

void vela_view_update_decoration(struct vela_view *view)
{
    // X11 windows that leave the bar to the window manager (apps with a bar of
    // their own, like Steam, say no), but not splash screens; Wayland apps
    // that accept a server-side bar (xdg-decoration: Qt and KDE, Chromium if
    // chosen); and apps that never ask but expect the compositor's bar anyway
    // (kitty: decoration-ssd in vela.conf).
    bool wanted = view->xdg_decoration
        && view->xdg_decoration->current.mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
    if (!view->xdg_decoration && !view->x11) {
        wanted = vela_config_list_has(view->server->ssd_include, vela_view_app_id(view));
    }
#if WLR_HAS_XWAYLAND
    if (view->x11) {
        wanted = !view->x11->override_redirect && view->x11->decorations == WLR_XWAYLAND_SURFACE_DECORATIONS_ALL
            && !wlr_xwayland_surface_has_window_type(view->x11, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_SPLASH);
    }
#endif
    if (wanted && !view->decoration) {
        struct vela_decoration_state state = decoration_state(view); // still without the bar in the geometry
        view->decoration = vela_decoration_create(view->server, view->tree, &state);
    } else if (!wanted && view->decoration) {
        vela_decoration_destroy(view->decoration);
        view->decoration = NULL;
    } else {
        vela_view_refresh_decoration(view);
    }
}

// Vela's bar, unless the app explicitly asks to draw its own (§9.1): Chromium
// and Firefox with tabs in place of the title would draw theirs anyway, and
// the buttons would be doubled. Those that ask for nothing, or for ours, get
// Vela's.
static enum wlr_xdg_toplevel_decoration_v1_mode decoration_mode(const struct wlr_xdg_toplevel_decoration_v1 *decoration)
{
    return decoration->requested_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE
        ? WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE
        : WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
}

static void handle_decoration_mode(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, decoration_mode);
    if (view->xdg->base->initialized) {
        wlr_xdg_toplevel_decoration_v1_set_mode(view->xdg_decoration, decoration_mode(view->xdg_decoration));
    }
}

static void handle_decoration_destroy(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, decoration_destroy);
    vela_unlisten(&view->decoration_mode);
    vela_unlisten(&view->decoration_destroy);
    view->xdg_decoration = NULL;
    vela_view_update_decoration(view);
}

static void set_xdg_decoration(struct vela_view *view, struct wlr_xdg_toplevel_decoration_v1 *decoration)
{
    view->xdg_decoration = decoration;
    vela_unlisten(&view->decoration_mode);
    vela_unlisten(&view->decoration_destroy);
    vela_listen(&decoration->events.request_mode, &view->decoration_mode, handle_decoration_mode);
    vela_listen(&decoration->events.destroy, &view->decoration_destroy, handle_decoration_destroy);
    if (view->xdg->base->initialized) {
        wlr_xdg_toplevel_decoration_v1_set_mode(decoration, decoration_mode(decoration));
    }
}

static void handle_new_decoration(struct wl_listener *listener, void *data)
{
    struct wlr_xdg_toplevel_decoration_v1 *decoration = data;
    struct vela_view *view = decoration->toplevel->base->data;
    if (view) {
        set_xdg_decoration(view, decoration);
    }
}

// ---------------------------------------------------------------- taskbar --

static void update_ext_handle(struct vela_view *view)
{
    if (!view->ext_handle) {
        return;
    }
    const struct wlr_ext_foreign_toplevel_handle_v1_state state = {
        .title = vela_view_title(view),
        .app_id = vela_view_app_id(view),
    };
    wlr_ext_foreign_toplevel_handle_v1_update_state(view->ext_handle, &state);
}

// Dialogs declare the window they depend on, so the taskbar knows not to show
// them as separate apps.
static void update_handle_parent(struct vela_view *view)
{
    if (!view->handle) {
        return;
    }
    struct vela_view *owner = view_parent(view);
    wlr_foreign_toplevel_handle_v1_set_parent(view->handle, owner ? owner->handle : NULL);
}

void vela_view_title_changed(struct vela_view *view)
{
    if (view->handle) {
        wlr_foreign_toplevel_handle_v1_set_title(view->handle, vela_view_title(view));
    }
    update_ext_handle(view);
    vela_view_check_capture_rule(view); // capture=no with title~= follows the title
}

void vela_view_app_id_changed(struct vela_view *view)
{
    if (view->handle) {
        wlr_foreign_toplevel_handle_v1_set_app_id(view->handle, vela_view_app_id(view));
    }
    update_ext_handle(view);
}

void vela_view_parent_changed(struct vela_view *view)
{
    update_handle_parent(view);
    vela_views_update_capture_hidden(view->server); // a dialog of a hidden window is hidden
}

// --------------------------------------------- hidden from screen capture --

bool vela_capture_hide_trusts(struct vela_server *server, struct wl_client *client)
{
    // false here leaves even the shell's previews of hidden windows blank.
    return client && client == server->shell.client;
}

bool vela_view_capture_hidden(const struct vela_view *view)
{
    // The window or one of its parents (depth-limited: a parent cycle can't hang us).
    for (int depth = 0; view && depth < 32; view = view_parent(view), ++depth) {
        if (view->capture_choice ? view->capture_choice > 0 : view->capture_rule_hides) {
            return true;
        }
    }
    return false;
}

void vela_view_set_capture_hidden(struct vela_view *view, bool on)
{
    view->capture_choice = on ? 1 : -1;
    vela_views_update_capture_hidden(view->server);
}

static bool rule_hides(struct vela_view *view)
{
    const struct vela_window_rule *rule
        = vela_window_rules_find(&view->server->window_rules, vela_view_app_id(view), vela_view_title(view));
    return rule && rule->capture == 0;
}

void vela_view_check_capture_rule(struct vela_view *view)
{
    bool hides = view->mapped && rule_hides(view);
    if (hides != view->capture_rule_hides) {
        view->capture_rule_hides = hides;
        vela_views_update_capture_hidden(view->server);
    }
}

void vela_views_update_capture_hidden(struct vela_server *server)
{
    int count = 0;
    bool x11 = false;
    bool changed = false;
    struct vela_view *view;
    wl_list_for_each (view, &server->views, link) {
        bool hidden = vela_view_capture_hidden(view);
        if (hidden != view->tree->node.capture_hidden) {
            view->tree->node.capture_hidden = hidden;
            changed = true;
            if (hidden) {
                // Shares of this window that are running (a portal) stop now.
                vela_window_capture_destroy(view->capture);
                view->capture = NULL;
            }
        }
        count += hidden;
        x11 = x11 || (hidden && view->x11);
    }
    // X11 menus and the dragged icon don't know their window (§15.2).
    server->layers.x11_popups->node.capture_hidden = x11;
    server->layers.x11_covers->node.capture_hidden = x11;
    server->layers.drag->node.capture_hidden = count > 0;
    bool count_changed = count != server->capture_hidden_count;
    server->capture_hidden_count = count;
    struct vela_layer_surface *layer;
    wl_list_for_each (layer, &server->layer_surfaces, link) {
        vela_layer_update_capture_hidden(layer);
    }
    if (changed || count_changed) {
        // No capture stream keeps an old frame with (or without) the window.
        struct vela_output_frame *frame;
        wl_list_for_each (frame, &server->scene->frames, link) {
            vela_output_frame_damage_whole(frame);
        }
        wlr_log(WLR_INFO, "Windows hidden from screen capture: %d", count);
        vela_workspaces_announce(server); // Task View's menu shows the choice
    }
    if (count_changed) {
        vela_capture_watch_hidden_changed(server->capture_watch);
    }
}

static void handle_taskbar_activate(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, handle_activate);
    vela_focus_view(view->server, view);
}

static void handle_taskbar_close(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, handle_close);
    vela_view_close(view, "taskbar");
}

static void handle_taskbar_maximize(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, handle_maximize);
    struct wlr_foreign_toplevel_handle_v1_maximized_event *event = data;
    vela_view_set_maximized(view, event->maximized, true);
}

static void handle_taskbar_minimize(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, handle_minimize);
    struct wlr_foreign_toplevel_handle_v1_minimized_event *event = data;
    vela_view_set_minimized(view, event->minimized);
}

static void handle_taskbar_fullscreen(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, handle_fullscreen);
    struct wlr_foreign_toplevel_handle_v1_fullscreen_event *event = data;
    vela_view_set_fullscreen(view, event->fullscreen);
}

static void handle_taskbar_rectangle(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, handle_rectangle);
    struct wlr_foreign_toplevel_handle_v1_set_rectangle_event *event = data;
    // The taskbar tells us where this window's button is, in its surface
    // coordinates: we bring it to global coordinates.
    view->taskbar_rect = (struct wlr_box) { 0 };
    if (event->width <= 0 || event->height <= 0) {
        return;
    }
    struct vela_layer_surface *layer;
    wl_list_for_each (layer, &view->server->layer_surfaces, link) {
        if (layer->wlr->surface == event->surface) {
            double lx = 0.0;
            double ly = 0.0;
            vela_node_coords(&layer->tree->node, &lx, &ly);
            view->taskbar_rect = (struct wlr_box) {
                (int)lround(lx) + event->x,
                (int)lround(ly) + event->y,
                event->width,
                event->height,
            };
            return;
        }
    }
}

static void create_taskbar_handle(struct vela_view *view)
{
#if WLR_HAS_XWAYLAND
    if (view->x11 && view->x11->skip_taskbar) {
        return; // _NET_WM_STATE_SKIP_TASKBAR (Wine's fullscreen backdrop)
    }
#endif
    struct wlr_foreign_toplevel_handle_v1 *handle
        = wlr_foreign_toplevel_handle_v1_create(view->server->foreign_toplevels);
    view->handle = handle;
    handle->data = view;
    wlr_foreign_toplevel_handle_v1_set_title(handle, vela_view_title(view));
    wlr_foreign_toplevel_handle_v1_set_app_id(handle, vela_view_app_id(view));
    wlr_foreign_toplevel_handle_v1_set_maximized(handle, view->maximized);
    wlr_foreign_toplevel_handle_v1_set_fullscreen(handle, view->fullscreen);
    wlr_foreign_toplevel_handle_v1_set_minimized(handle, view->minimized);
    wlr_foreign_toplevel_handle_v1_set_activated(handle, view->activated);
    update_handle_parent(view);
    vela_listen(&handle->events.request_activate, &view->handle_activate, handle_taskbar_activate);
    vela_listen(&handle->events.request_close, &view->handle_close, handle_taskbar_close);
    vela_listen(&handle->events.request_maximize, &view->handle_maximize, handle_taskbar_maximize);
    vela_listen(&handle->events.request_minimize, &view->handle_minimize, handle_taskbar_minimize);
    vela_listen(&handle->events.request_fullscreen, &view->handle_fullscreen, handle_taskbar_fullscreen);
    vela_listen(&handle->events.set_rectangle, &view->handle_rectangle, handle_taskbar_rectangle);
}

static void destroy_taskbar_handle(struct vela_view *view)
{
    if (!view->handle) {
        return;
    }
    // wlroots checks that nobody listens to the handle any more when
    // destroying it.
    vela_unlisten(&view->handle_activate);
    vela_unlisten(&view->handle_close);
    vela_unlisten(&view->handle_maximize);
    vela_unlisten(&view->handle_minimize);
    vela_unlisten(&view->handle_fullscreen);
    vela_unlisten(&view->handle_rectangle);
    wlr_foreign_toplevel_handle_v1_destroy(view->handle);
    view->handle = NULL;
    view->taskbar_rect = (struct wlr_box) { 0 };
}

static void create_handles(struct vela_view *view)
{
    const struct wlr_ext_foreign_toplevel_handle_v1_state state = {
        .title = vela_view_title(view),
        .app_id = vela_view_app_id(view),
    };
    view->ext_handle = wlr_ext_foreign_toplevel_handle_v1_create(view->server->ext_toplevels, &state);
    view->ext_handle->data = view;
    if (vela_view_on_current_workspace(view)) {
        create_taskbar_handle(view);
    }
}

static void destroy_handles(struct vela_view *view)
{
    if (view->ext_handle) {
        wlr_ext_foreign_toplevel_handle_v1_destroy(view->ext_handle);
        view->ext_handle = NULL;
    }
    destroy_taskbar_handle(view);
}

void vela_view_show_in_taskbar(struct vela_view *view, bool on)
{
    if (on && !view->handle && view->mapped) {
        create_taskbar_handle(view);
    } else if (!on && view->handle) {
        destroy_taskbar_handle(view);
    }
}

void vela_view_set_activated(struct vela_view *view, bool on)
{
    view->activated = on;
    send_activated(view, on);
    vela_view_refresh_decoration(view);
    if (view->handle) {
        wlr_foreign_toplevel_handle_v1_set_activated(view->handle, on);
    }
}

// ------------------------------------------------------------- animation --

static void apply_open_frame(struct vela_view *view, double progress)
{
    // Maximized and fullscreen windows only fade in.
    int rise = (view->maximized || view->fullscreen) ? 0 : VELA_WINDOW_OPEN_RISE;
    vela_node_set_position(&view->tree->node, view->target_x, view->target_y + round((1.0 - progress) * rise));
    // Opacity reaches 1 a little before the motion ends: the window is
    // readable at once, the final motion is just "settling".
    vela_node_set_opacity(&view->tree->node, (float)fmin(1.0, progress * 1.4));
}

static void start_open_animation(struct vela_view *view)
{
    vela_tween_start(&view->open_tween, VELA_WINDOW_OPEN_MS, &vela_decelerate);
    view->animating = true;
    view->open_frames = 0;
    apply_open_frame(view, 0.0);
    vela_server_schedule_frames(view->server);
}

void vela_view_finish_open_animation(struct vela_view *view)
{
    if (!view->animating) {
        return;
    }
    view->animating = false;
    apply_open_frame(view, 1.0);
}

bool vela_views_tick(struct vela_server *server, double now_ms)
{
    bool running = false;
    struct vela_view *view, *next;
    wl_list_for_each_safe (view, next, &server->views, link) {
        if (!view->animating) {
            continue;
        }
        apply_open_frame(view, vela_tween_progress(&view->open_tween, now_ms));
        ++view->open_frames;
        if (vela_tween_finished(&view->open_tween, now_ms)) {
            wlr_log(WLR_DEBUG, "Open animation: %d frames", view->open_frames);
            vela_view_finish_open_animation(view);
        } else {
            running = true;
        }
    }
    return running;
}

bool vela_views_animating(struct vela_server *server)
{
    struct vela_view *view;
    wl_list_for_each (view, &server->views, link) {
        if (view->animating) {
            return true;
        }
    }
    return false;
}

void vela_view_update_shape(struct vela_view *view)
{
    struct vela_shape shape = { 0 };
    bool own_shadow = false;
    if (view->xdg && view->xdg->base->surface) {
        struct wlr_box g = view->xdg->base->geometry;
        int width = view->xdg->base->surface->current.width;
        int height = view->xdg->base->surface->current.height;
        own_shadow = g.x > 0 || g.y > 0 || (g.width > 0 && g.width < width) || (g.height > 0 && g.height < height);
    }
    if (view->mapped && !view->minimized && !view->maximized && !view->fullscreen && vela_snap_is_none(view->snap)
        && !own_shadow) {
        struct wlr_box g = vela_view_geometry(view);
        if (g.width > 0 && g.height > 0) {
            shape.enabled = true;
            shape.x = g.x;
            shape.y = g.y;
            shape.width = g.width;
            shape.height = g.height;
            shape.radius = CORNER_RADIUS;
            shape.shadow = true;
            shape.active = view->activated;
        }
    }
    vela_tree_set_shape(view->tree, &shape);
}

void vela_view_update_blur(struct vela_view *view)
{
    struct vela_server *server = view->server;
    const char *app_id = vela_view_app_id(view);
    const struct vela_blur_app *own = NULL;
    for (int i = 0; app_id && i < server->window_blur_app_count; ++i) {
        if (strcasecmp(server->window_blur_apps[i].app, app_id) == 0) {
            own = &server->window_blur_apps[i];
            break;
        }
    }
    bool forced = server->window_blur && view->mapped && !view->fullscreen
        && !vela_config_list_has(server->window_blur_exclude, app_id)
        && (server->window_blur_all || own || vela_config_list_has(server->window_blur_include, app_id));
    struct wlr_box box = { 0 };
    if (forced) {
        box = vela_view_geometry(view);
        // Vela's bar is above the surface, drawn by us: only the app's part.
        int bar = vela_view_title_bar_height(view);
        box.y += bar;
        box.height -= bar;
    }
    // The whole window (surfaces and bar) translucent, the shadow not: it
    // stays outside the window. The blur keeps its own strength (frame.c).
    int percent = view->activated ? server->window_opacity : server->window_opacity_inactive;
    if (own && own->opacity) {
        percent = own->opacity;
    }
    float opacity = forced ? percent / 100.0f : 1.0f;
    int strength = own && own->strength ? own->strength : server->window_blur_strength;
    if (view->surface_node) {
        vela_surface_node_set_blur(view->surface_node, &box);
        vela_surface_node_set_blur_scale(view->surface_node, strength / 100.0f);
        vela_node_set_opacity(&view->surface_node->node, opacity);
    }
    if (view->decoration) {
        vela_decoration_set_opacity(view->decoration, opacity);
    }
}

static void free_blur_apps(struct vela_server *server)
{
    for (int i = 0; i < server->window_blur_app_count; ++i) {
        free(server->window_blur_apps[i].app);
    }
    free(server->window_blur_apps);
    server->window_blur_apps = NULL;
    server->window_blur_app_count = 0;
}

// window-blur-app=<app-id>|strength=N,opacity=N (either may be missing).
static void add_blur_app(const char *value, void *data)
{
    struct vela_server *server = data;
    const char *bar = strchr(value, '|');
    if (!bar || bar == value) {
        return;
    }
    struct vela_blur_app entry = { strndup(value, (size_t)(bar - value)), 0, 0 };
    char *copy = strdup(bar + 1);
    char *save = NULL;
    for (char *pair = strtok_r(copy, ",", &save); pair; pair = strtok_r(NULL, ",", &save)) {
        if (strncmp(pair, "strength=", 9) == 0) {
            entry.strength = vela_clamp(atoi(pair + 9), 50, 300);
        } else if (strncmp(pair, "opacity=", 8) == 0) {
            entry.opacity = vela_clamp(atoi(pair + 8), 30, 100);
        }
    }
    free(copy);
    struct vela_blur_app *grown = realloc(server->window_blur_apps,
        sizeof(*grown) * (size_t)(server->window_blur_app_count + 1));
    if (!grown) {
        free(entry.app);
        return;
    }
    server->window_blur_apps = grown;
    grown[server->window_blur_app_count++] = entry;
}

void vela_views_load_settings(struct vela_server *server)
{
    struct vela_config settings;
    vela_config_read(&settings);
    server->window_blur = vela_config_flag(&settings, "window-blur", false);
    server->window_blur_all = vela_config_flag(&settings, "window-blur-all", false);
    free(server->window_blur_include);
    free(server->window_blur_exclude);
    const char *include = vela_config_get(&settings, "window-blur-include", "");
    const char *exclude = vela_config_get(&settings, "window-blur-exclude", "");
    server->window_blur_include = *include ? strdup(include) : NULL;
    server->window_blur_exclude = *exclude ? strdup(exclude) : NULL;
    server->window_opacity = vela_clamp(atoi(vela_config_get(&settings, "window-opacity", "100")), 30, 100);
    const char *inactive = vela_config_get(&settings, "window-opacity-inactive", "");
    server->window_opacity_inactive = *inactive ? vela_clamp(atoi(inactive), 30, 100) : server->window_opacity;
    server->window_blur_strength = vela_clamp(atoi(vela_config_get(&settings, "window-blur-strength", "100")), 50, 300);
    server->taskbar_look.blur = vela_clamp(atoi(vela_config_get(&settings, "taskbar-blur", "100")), 50, 300);
    server->taskbar_look.opacity = vela_clamp(atoi(vela_config_get(&settings, "taskbar-opacity", "100")), 0, 200);
    server->quicksettings_look.blur = vela_clamp(atoi(vela_config_get(&settings, "quicksettings-blur", "100")), 50, 300);
    server->quicksettings_look.opacity = vela_clamp(atoi(vela_config_get(&settings, "quicksettings-opacity", "100")), 0, 200);
    struct vela_layer_surface *panel;
    wl_list_for_each (panel, &server->layer_surfaces, link) {
        vela_layer_update_look(panel);
    }
    free_blur_apps(server);
    vela_config_each("window-blur-app", add_blur_app, server);
    vela_window_rules_load(&server->window_rules);
    server->remember_window_positions = vela_config_flag(&settings, "remember-window-positions", true);
    free(server->ssd_include);
    const char *ssd = vela_config_get(&settings, "decoration-ssd", "kitty");
    server->ssd_include = *ssd ? strdup(ssd) : NULL;
    vela_config_finish(&settings);
    // The windows get it on the next frame (vela_server_animate).
    vela_server_schedule_frames(server);
    // Open windows pick up a changed bar list at once (new ones on map and
    // first commit).
    struct vela_view *view;
    wl_list_for_each (view, &server->views, link) {
        if (view->mapped && view->xdg) {
            vela_view_update_decoration(view);
        }
        // Unlike the other rules, capture= reaches the open windows too: a
        // privacy rule must not wait for the next window (§15).
        view->capture_rule_hides = view->mapped && rule_hides(view);
    }
    vela_views_update_capture_hidden(server);
}

// ----------------------------------------------------- maximize/fullscreen --

static void apply_maximized(struct vela_view *view)
{
    struct vela_output *output = view->mapped ? vela_view_output(view) : vela_output_under_cursor(view->server);
    if (!output || !vela_view_configurable(view)) {
        return;
    }
    vela_view_send_maximized(view, true);
    struct vela_placement place = vela_output_place(output, vela_output_usable_area(output));
    // Set the scene position first. vela_view_configure_size() sends an X11
    // ConfigureWindow request using the scene node's *current* coordinates.
    // Sending the new size first leaves Xwayland at the old window position.
    vela_view_keep_in_place(view);
    vela_view_configure_size(view, place.width, place.height);
    if (view->handle) {
        wlr_foreign_toplevel_handle_v1_set_maximized(view->handle, true);
    }
}

// Back to the free frame it had before being maximized or fullscreen.
static void restore(struct vela_view *view)
{
    struct wlr_box back = vela_view_restore_box(view);
    if (view->mapped && back.width > 0) {
        struct wlr_box geometry = vela_view_geometry(view);
        vela_node_set_position(&view->tree->node, back.x - geometry.x, back.y - geometry.y);
    }
    // For X11, configure after positioning: size-only then move-only
    // configurations can leave the X11 window offset from Vela's scene.
    vela_view_configure_size(view, back.width, back.height);
}

void vela_view_set_maximized(struct vela_view *view, bool on, bool animate)
{
    if (!vela_view_configurable(view)) {
        return;
    }
    if (on == view->maximized || view->fullscreen) {
        // The protocol wants an answer to the request anyway.
        if (view->xdg) {
            wlr_xdg_surface_schedule_configure(view->xdg->base);
        }
        return;
    }
    vela_view_finish_open_animation(view);
    // Like Windows 11: the window morphs into the new frame.
    animate = animate && view->mapped && !view->minimized && view->tree->node.enabled
        && vela_view_on_current_workspace(view);

    if (on) {
        if (animate) {
            struct vela_output *output = vela_view_output(view);
            if (output) {
                struct vela_placement place = vela_output_place(output, vela_output_usable_area(output));
                vela_snapshot_morph(view->server, view,
                    (struct wlr_box) { (int)lround(place.x), (int)lround(place.y), place.width, place.height });
            }
        }
        vela_view_leave_snap_group(view);
        if (view->mapped && vela_snap_is_none(view->snap)) {
            view->restore = vela_view_frame_box(view); // from snapped it goes back to the free size
        }
        if (!vela_snap_is_none(view->snap)) {
            view->snap = vela_snap_none;
            vela_view_send_tiled(view, WLR_EDGE_NONE);
        }
        view->maximized = true;
        apply_maximized(view);
        vela_view_refresh_decoration(view); // the button becomes "restore"
        vela_workspaces_announce(view->server); // the taskbar's backdrop
        return;
    }

    if (animate) {
        vela_snapshot_morph(view->server, view, vela_view_restore_box(view));
    }
    view->maximized = false;
    vela_view_send_maximized(view, false);
    if (view->handle) {
        wlr_foreign_toplevel_handle_v1_set_maximized(view->handle, false);
    }
    restore(view);
    vela_view_refresh_decoration(view);
    vela_workspaces_announce(view->server);
}

void vela_view_set_fullscreen(struct vela_view *view, bool on)
{
    if (!vela_view_configurable(view)) {
        return;
    }
    // Actual fullscreen state changes override an outstanding Wine restore
    // hint. Redundant requests must not: Wine can echo fullscreen-off after
    // Vela reconciled a popup -> decorated style change.
    if (on != view->fullscreen) {
        view->x11_wine_restore_fullscreen = false;
    }
    if (on == view->fullscreen) {
        if (view->xdg) {
            wlr_xdg_surface_schedule_configure(view->xdg->base);
        }
        return;
    }
    vela_view_finish_open_animation(view);
    struct vela_output *output = view->mapped ? vela_view_output(view) : vela_output_under_cursor(view->server);

    if (on) {
        if (view->mapped && !view->maximized && vela_snap_is_none(view->snap)) {
            view->restore = vela_view_frame_box(view);
        }
        view->fullscreen = true;
        send_fullscreen(view, true);
        if (view->handle) {
            wlr_foreign_toplevel_handle_v1_set_fullscreen(view->handle, true);
        }
        // Above taskbar and panels. Position the scene before configuring
        // the X11 window so it gets the final fullscreen origin (not the
        // old centered windowed origin) in the same ConfigureWindow request.
        vela_node_reparent(&view->tree->node, view->server->layers.fullscreen);
        vela_view_keep_below(view); // Wine's backdrop must not cover the game
        vela_view_keep_in_place(view);
        if (output) {
            struct vela_placement place = vela_output_place(output, vela_output_full_area(output));
            vela_view_configure_size(view, place.width, place.height);
        }
        vela_view_refresh_decoration(view); // fullscreen hides the bar
        return;
    }

    view->fullscreen = false;
    send_fullscreen(view, false);
    if (view->handle) {
        wlr_foreign_toplevel_handle_v1_set_fullscreen(view->handle, false);
    }
    vela_node_reparent(&view->tree->node, view->server->layers.windows);
    vela_views_raise_always_on_top(view->server); // the flag was kept
    vela_view_keep_below(view);
    if (view->maximized) {
        apply_maximized(view);
    } else if (!vela_snap_is_none(view->snap)) {
        vela_view_apply_snap(view, vela_view_output(view));
    } else {
        restore(view);
    }
    vela_view_refresh_decoration(view);
}

// ------------------------------------------------------------ minimize --

void vela_view_finish_restore(struct vela_view *view)
{
    if (view->mapped && !view->minimized) {
        vela_node_set_enabled(&view->tree->node, true);
    }
}

// Like Windows: the window disappears, goes to the end of the Alt+Tab order
// and the keyboard moves to the next window. It's restored from the taskbar or
// with Alt+Tab.
void vela_view_set_minimized(struct vela_view *view, bool on)
{
    struct vela_server *server = view->server;
    if (!view->mapped || on == view->minimized) {
        return;
    }
    view->minimized = on;
    if (view->handle) {
        wlr_foreign_toplevel_handle_v1_set_minimized(view->handle, on);
    }
    vela_workspaces_announce(server); // rects leave minimized windows out
    if (!on) {
        // The real window shows again at the end of the flight
        // (finish_restore); meanwhile it already gets the keyboard.
        vela_focus_view(server, view);
        if (!vela_snapshot_animate(server, view, VELA_SNAPSHOT_RESTORE)) {
            vela_view_finish_restore(view);
        }
        return;
    }
    vela_view_finish_open_animation(view);
    bool was_focused = vela_views_focused(server) == view;
    vela_node_set_enabled(&view->tree->node, false);
    vela_snapshot_animate(server, view, VELA_SNAPSHOT_MINIMIZE);
    if (server->grabbed == view) { // it was being dragged
        vela_snap_end_zone(server, false);
        server->grabbed = NULL;
        server->cursor_mode = VELA_CURSOR_PASSTHROUGH;
    }
    wl_list_remove(&view->link);
    wl_list_insert(server->views.prev, &view->link);
    if (was_focused) {
        vela_view_set_activated(view, false);
        vela_focus_refocus(server);
    }
}

// Brings it to `output` at (x, y) (put back inside the usable area, maximized,
// snapped or fullscreen as before).
static void move_to_output(struct vela_view *view, struct vela_output *output, int x, int y)
{
    // The frame with Vela's bar on top: all inside the usable area.
    int bar = vela_view_title_bar_height(view);
    struct wlr_box geometry = vela_view_geometry(view);
    struct wlr_box frame = vela_view_frame_box(view);
    struct wlr_box outer = { x, y - bar, frame.width, frame.height + bar };
    struct wlr_box fitted = vela_output_fit(output, outer);
    if (!view->maximized && !view->fullscreen && vela_snap_is_none(view->snap)
        && (fitted.width != outer.width || fitted.height != outer.height)) {
        vela_view_configure_size(view, fitted.width, fitted.height - bar);
    }
    vela_node_set_position(&view->tree->node, fitted.x - geometry.x, fitted.y + bar - geometry.y);
    if (view->fullscreen) {
        struct vela_placement place = vela_output_place(output, vela_output_full_area(output));
        vela_view_configure_size(view, place.width, place.height);
        vela_view_keep_in_place(view);
    } else if (view->maximized) {
        apply_maximized(view);
    } else if (!vela_snap_is_none(view->snap)) {
        vela_view_apply_snap(view, output);
    }
}

// ----------------------------------------------------------- life cycle --

// Every listener of a view: they all start disconnected, so all of them
// can be disconnected at the end.
enum { VIEW_LISTENERS = 31 };

static void listeners(struct vela_view *view, struct wl_listener *out[VIEW_LISTENERS])
{
    struct wl_listener *all[VIEW_LISTENERS] = {
        &view->map,
        &view->unmap,
        &view->commit,
        &view->client_commit,
        &view->destroy,
        &view->request_move,
        &view->request_resize,
        &view->request_maximize,
        &view->request_fullscreen,
        &view->request_minimize,
        &view->request_window_menu,
        &view->set_title,
        &view->set_app_id,
        &view->set_parent,
        &view->new_popup,
        &view->decoration_mode,
        &view->decoration_destroy,
        &view->associate,
        &view->dissociate,
        &view->request_configure,
        &view->request_activate,
        &view->set_decorations,
        &view->set_override_redirect,
        &view->request_below,
        &view->request_skip_taskbar,
        &view->handle_activate,
        &view->handle_close,
        &view->handle_maximize,
        &view->handle_minimize,
        &view->handle_fullscreen,
        &view->handle_rectangle,
    };
    memcpy(out, all, sizeof(all));
}

bool vela_view_stays_below(const struct vela_view *view)
{
#if WLR_HAS_XWAYLAND
    return view->x11 && view->x11->below;
#else
    return false;
#endif
}

void vela_view_keep_below(struct vela_view *view)
{
    struct vela_tree *parent = view->tree->node.parent;
    if (!vela_view_stays_below(view) || !parent) {
        return;
    }
    struct vela_node *bottom = wl_container_of(parent->children.next, bottom, link);
    if (bottom != &view->tree->node) {
        vela_node_place_below(&view->tree->node, bottom);
    }
}

void vela_view_init(struct vela_view *view, struct vela_server *server)
{
    view->owner.kind = VELA_OWNER_VIEW;
    view->server = server;
    view->open_tween = (struct vela_tween) { -1.0, 1.0, NULL };
    wl_list_init(&view->link);
    wl_list_init(&view->minimized_link);
    view->tree = vela_tree_create(server->layers.windows);
    view->tree->node.data = &view->owner;
    // All listeners start disconnected: they can always be removed.
    struct wl_listener *all[VIEW_LISTENERS];
    listeners(view, all);
    for (int i = 0; i < VIEW_LISTENERS; ++i) {
        wl_list_init(&all[i]->link);
    }
}

// The window no longer exists for the user: out of the list and of whoever
// remembered it (desktops, snap group, animations, drags).
static void forget(struct vela_view *view)
{
    struct vela_server *server = view->server;
    bool was_focused = vela_views_focused(server) == view;
    vela_workspaces_forget(server, view);
    vela_view_leave_snap_group(view);
    wl_list_remove(&view->link);
    wl_list_init(&view->link);
    vela_snapshot_cancel(server, view);
    vela_focus_forget_view(server, view, was_focused);
}

void vela_view_mapped(struct vela_view *view)
{
    struct vela_server *server = view->server;
    view->mapped = true;
    if (view->xdg) {
        vela_view_update_decoration(view); // the bar is part of the frame to place
    }
    // Window rules (window_rules.h): once, now; the user can move it later.
    const struct vela_window_rule *rule
        = vela_window_rules_find(&server->window_rules, vela_view_app_id(view), vela_view_title(view));
    // Remembered state of the app (window_memory.h): below the rule, above the
    // default. Not for dialogs, fullscreen windows, or X11 windows that came
    // with a position of their own.
    const struct vela_window_memory_entry *mem = NULL;
    if (server->remember_window_positions && !view->fullscreen && !view_parent(view)
        && !(view->x11 && (view->x11_initial.x || view->x11_initial.y))) {
        mem = vela_window_memory_find(&server->window_memory, vela_view_app_id(view));
    }
    struct vela_output *output = NULL;
    if (rule && rule->output) {
        output = vela_output_named(server, rule->output);
        int index = atoi(rule->output);
        struct vela_output *candidate;
        wl_list_for_each (candidate, &server->outputs, link) {
            if (!output && index > 0 && --index == 0) {
                output = candidate;
            }
        }
    }
    if (!output && mem) {
        output = vela_output_named(server, mem->output);
    }
    if (!output) {
        output = vela_output_under_cursor(server);
    }
    if (rule && rule->fullscreen == 0 && view->fullscreen) {
        vela_view_set_fullscreen(view, false);
    }
    bool remembered_max = mem && mem->maximized && !(rule && rule->maximized != -1);
    if (((rule && rule->maximized == 1) || remembered_max) && !view->maximized && !view->fullscreen && output) {
        // Put it on the chosen output first: that's where it maximizes.
        struct wlr_box g = vela_view_geometry(view);
        vela_node_set_position(&view->tree->node, output->usable.x + (output->usable.width - g.width) / 2 - g.x,
            output->usable.y + (output->usable.height - g.height) / 2 - g.y);
        vela_view_set_maximized(view, true, false);
        if (remembered_max) {
            // Leaving maximized goes back to where it was.
            view->restore = vela_output_fit(output, (struct wlr_box) { output->usable.x + mem->x,
                                                        output->usable.y + mem->y, mem->width, mem->height });
        }
    }
    struct wlr_box geometry = vela_view_geometry(view);
    if (output && (view->fullscreen || view->maximized)) {
        struct vela_placement place = vela_output_place(output,
            view->fullscreen ? vela_output_full_area(output) : vela_output_usable_area(output));
        view->target_x = place.x - geometry.x;
        view->target_y = place.y - geometry.y;
    } else if (output) {
        // New windows in the center of the usable area, like Windows; never
        // larger than it (some apps remember the size they had on another
        // output or in another session).
        const struct wlr_box area = output->usable;
        int width = geometry.width;
        int height = geometry.height;
        if (rule && rule->has_size) {
            width = rule->width;
            height = rule->height;
        } else if (mem) {
            width = mem->width;
            height = mem->height;
        }
        struct wlr_box frame = vela_output_fit(output, (struct wlr_box) { 0, 0, width, height });
        if (frame.width != geometry.width || frame.height != geometry.height) {
            vela_view_configure_size(view, frame.width, frame.height);
        }
        int x = area.x + (area.width - frame.width) / 2;
        int y = area.y + (area.height - frame.height) / 2;
        if (rule && rule->has_position && rule->center != 1) {
            frame.x = area.x + rule->x;
            frame.y = area.y + rule->y;
            frame = vela_output_fit(output, frame);
            x = frame.x;
            y = frame.y;
        } else if (mem && !(rule && rule->center == 1)) {
            frame.x = area.x + mem->x;
            frame.y = area.y + mem->y;
            frame = vela_output_fit(output, frame);
            x = frame.x;
            y = frame.y;
        }
        view->target_x = x - geometry.x;
        view->target_y = y - geometry.y;
    }
    wl_list_insert(&server->views, &view->link);
    vela_workspaces_view_mapped(server, view);
    bool elsewhere = false;
    int desktop = rule && rule->desktop > 0 ? rule->desktop : (mem ? mem->desktop + 1 : 0);
    if (desktop > 0 && desktop <= server->workspaces->count) {
        // Not through move_view: the handles don't exist yet. No focus either,
        // so the desktop doesn't switch under the user.
        view->workspace = desktop - 1;
        elsewhere = !vela_view_on_current_workspace(view);
        if (elsewhere) {
            vela_node_set_enabled(&view->tree->node, false);
        }
    }
    create_handles(view);
    if (vela_view_stays_below(view)) {
        vela_view_keep_below(view); // no keyboard either: it is a backdrop
    } else if (!elsewhere) {
        vela_focus_new_view(server, view);
    }
    if (rule && rule->always_on_top == 1 && !view->fullscreen) {
        vela_view_set_always_on_top(view, true);
    }
    view->capture_rule_hides = rule && rule->capture == 0;
    vela_views_update_capture_hidden(server); // before its first frame
    start_open_animation(view);
    vela_workspaces_announce(server);
}

// Where the window closes is where it reopens (window_memory.h).
static void remember_position(struct vela_view *view)
{
    struct vela_server *server = view->server;
    if (!server->remember_window_positions || !view->mapped || view->fullscreen || view_parent(view)) {
        return;
    }
    struct vela_output *output = vela_view_output(view);
    if (!output) {
        return;
    }
    struct wlr_box frame = vela_view_frame_box(view);
    bool arranged = view->maximized || !vela_snap_is_none(view->snap);
    if (arranged && view->restore.width > 0 && view->restore.height > 0) {
        frame = view->restore;
    }
    struct vela_window_memory_entry entry = { .x = frame.x - output->usable.x, .y = frame.y - output->usable.y,
        .width = frame.width, .height = frame.height, .maximized = view->maximized, .desktop = view->workspace };
    snprintf(entry.output, sizeof(entry.output), "%s", output->wlr->name);
    vela_window_memory_set(&server->window_memory, vela_view_app_id(view), &entry);
}

/** Tear down an unmapped view and revoke any retained window-capture source. */
void vela_view_unmapped(struct vela_view *view)
{
    wlr_log(WLR_INFO, "Window unmapped: %s", vela_view_app_id(view));
    remember_position(view);
    // Close: the scene still has the last buffers (even if it has already
    // disabled the tree), we photograph them and fade them out. If the window
    // was hidden with a null buffer, the snapshot exists already.
    if (!view->minimized && !view->close_animated) {
        vela_snapshot_animate(view->server, view, VELA_SNAPSHOT_CLOSE);
    }
    view->close_animated = false;
    view->mapped = false;
    view->always_on_top = false;
    view->capture_rule_hides = false;
    view->capture_choice = 0;
    view->animating = false;
    if (view->minimized) {
        view->minimized = false;
        vela_node_set_enabled(&view->tree->node, true);
    }
    destroy_handles(view);
    // An unmapped view drops out of server->views; revoke its capture source
    // now so a retained session cannot outlive the view's visible lifetime
    // (and evade the lock-time revocation loop).
    vela_window_capture_destroy(view->capture);
    view->capture = NULL;
    forget(view);
    vela_views_update_capture_hidden(view->server);
    vela_workspaces_announce(view->server);
}

void vela_view_committed(struct vela_view *view)
{
    if (view->mapped) {
        vela_view_keep_in_place(view);
    }
    if (view->xdg) {
        vela_view_update_decoration(view); // the server-side bar starts (or ends) with this commit
    } else {
        vela_view_refresh_decoration(view); // size and scale may have changed
    }
}

void vela_view_destroy(struct vela_view *view)
{
    vela_decoration_destroy(view->decoration);
    view->decoration = NULL;
    destroy_handles(view);
    vela_window_capture_destroy(view->capture);
    view->capture = NULL;
    forget(view);
    // Closed while "show desktop" had it minimized: out of that list, or the
    // next restore removes freed memory.
    wl_list_remove(&view->minimized_link);
    struct wl_listener *all[VIEW_LISTENERS];
    listeners(view, all);
    for (int i = 0; i < VIEW_LISTENERS; ++i) {
        wl_list_remove(&all[i]->link);
    }
    if (view->surface_node) {
        vela_node_destroy(&view->surface_node->node);
    }
    vela_node_destroy(&view->tree->node);
    free(view);
}

// -------------------------------------------------------- Wayland windows --

static void handle_map(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, map);
    vela_view_mapped(view);
}

static void handle_unmap(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, unmap);
    vela_view_unmapped(view);
}

static void handle_commit(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, commit);
    struct wlr_xdg_toplevel *xdg = view->xdg;
    if (xdg->base->initial_commit) {
        // First commit: tell the client what we can do and let it choose its
        // size (0x0), unless it has already asked to start maximized or
        // fullscreen.
        wlr_xdg_toplevel_set_wm_capabilities(xdg,
            WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE | WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN
                | WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE | WLR_XDG_TOPLEVEL_WM_CAPABILITIES_WINDOW_MENU);
        // Vela draws the bar, if the app doesn't want one of its own (§9.1).
        if (view->xdg_decoration) {
            wlr_xdg_toplevel_decoration_v1_set_mode(view->xdg_decoration, decoration_mode(view->xdg_decoration));
        }
        if (xdg->requested.fullscreen) {
            vela_view_set_fullscreen(view, true);
        } else if (xdg->requested.maximized) {
            vela_view_set_maximized(view, true, true);
        } else {
            wlr_xdg_toplevel_set_size(xdg, 0, 0);
        }
        return;
    }
    vela_view_committed(view);
}

// The app is about to remove its content with a null buffer (window hidden):
// photograph it before the commit applies and the scene loses the buffer.
static void handle_client_commit(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, client_commit);
    const struct wlr_surface_state *pending = &view->xdg->base->surface->pending;
    if (view->mapped && !view->minimized && !view->close_animated && (pending->committed & WLR_SURFACE_STATE_BUFFER)
        && !pending->buffer) {
        view->close_animated = vela_snapshot_animate(view->server, view, VELA_SNAPSHOT_CLOSE);
    }
}

static void handle_destroy(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, destroy);
    vela_view_destroy(view);
}

static void handle_request_move(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, request_move);
    vela_interact_begin(view->server, view, VELA_CURSOR_MOVE, 0, false);
}

static void handle_request_resize(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, request_resize);
    struct wlr_xdg_toplevel_resize_event *event = data;
    vela_interact_begin(view->server, view, VELA_CURSOR_RESIZE, event->edges, false);
}

static void handle_request_maximize(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, request_maximize);
    if (view->xdg->base->initialized) {
        vela_view_set_maximized(view, view->xdg->requested.maximized, true);
    }
}

static void handle_request_fullscreen(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, request_fullscreen);
    if (view->xdg->base->initialized) {
        vela_view_set_fullscreen(view, view->xdg->requested.fullscreen);
    }
}

static void handle_request_minimize(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, request_minimize);
    vela_view_set_minimized(view, true);
}

// Apps with their own bar (GTK, libadwaita) ask for the window menu with a
// right click on their bar.
static void handle_request_window_menu(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, request_window_menu);
    struct wlr_xdg_toplevel_show_window_menu_event *event = data;
    vela_window_menu_show(view->server, view, view->tree->node.x + event->x, view->tree->node.y + event->y, false);
}

static void handle_set_title(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, set_title);
    vela_view_title_changed(view);
}

static void handle_set_app_id(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, set_app_id);
    vela_view_app_id_changed(view);
}

static void handle_set_parent(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, set_parent);
    vela_view_parent_changed(view);
}

static void handle_new_popup(struct wl_listener *listener, void *data)
{
    struct vela_view *view = wl_container_of(listener, view, new_popup);
    vela_popup_create(data, view->tree, &view->owner);
}

static void handle_new_toplevel(struct wl_listener *listener, void *data)
{
    struct vela_server *server = wl_container_of(listener, server, new_toplevel);
    struct wlr_xdg_toplevel *xdg = data;
    struct vela_view *view = calloc(1, sizeof(*view));
    vela_view_init(view, server);
    view->xdg = xdg;
    view->surface_node = vela_surface_node_create(view->tree, xdg->base->surface);
    xdg->base->data = view; // to get back to the parent window

    struct wlr_surface *surface = xdg->base->surface;
    vela_listen(&surface->events.map, &view->map, handle_map);
    vela_listen(&surface->events.unmap, &view->unmap, handle_unmap);
    vela_listen(&surface->events.commit, &view->commit, handle_commit);
    vela_listen(&surface->events.client_commit, &view->client_commit, handle_client_commit);
    vela_listen(&xdg->events.destroy, &view->destroy, handle_destroy);
    vela_listen(&xdg->events.request_move, &view->request_move, handle_request_move);
    vela_listen(&xdg->events.request_resize, &view->request_resize, handle_request_resize);
    vela_listen(&xdg->events.request_maximize, &view->request_maximize, handle_request_maximize);
    vela_listen(&xdg->events.request_fullscreen, &view->request_fullscreen, handle_request_fullscreen);
    vela_listen(&xdg->events.request_minimize, &view->request_minimize, handle_request_minimize);
    vela_listen(&xdg->events.request_show_window_menu, &view->request_window_menu, handle_request_window_menu);
    vela_listen(&xdg->events.set_title, &view->set_title, handle_set_title);
    vela_listen(&xdg->events.set_app_id, &view->set_app_id, handle_set_app_id);
    vela_listen(&xdg->events.set_parent, &view->set_parent, handle_set_parent);
    vela_listen(&xdg->base->events.new_popup, &view->new_popup, handle_new_popup);
}

// -------------------------------------------------------- active windows --

struct vela_view *vela_views_focused(struct vela_server *server)
{
    struct wlr_surface *focused = server->seat->keyboard_state.focused_surface;
    if (!focused) {
        return NULL;
    }
    struct vela_view *view;
    wl_list_for_each (view, &server->views, link) {
        if (vela_view_surface(view) == focused) {
            return view;
        }
    }
    return NULL;
}

struct vela_view *vela_views_system_prompt(struct vela_server *server)
{
    struct vela_view *view;
    wl_list_for_each (view, &server->views, link) {
        if (view->mapped && !view->minimized && vela_view_on_current_workspace(view)
            && vela_view_is_system_prompt(view)) {
            return view;
        }
    }
    return NULL;
}

// An app asks to bring one of its windows forward (a link opened in an already
// running Firefox, a click on a notification).
static void handle_request_activation(struct wl_listener *listener, void *data)
{
    struct vela_server *server = wl_container_of(listener, server, request_activation);
    struct wlr_xdg_activation_v1_request_activate_event *event = data;
    struct vela_view *view;
    wl_list_for_each (view, &server->views, link) {
        if (vela_view_surface(view) == event->surface) {
            vela_focus_new_view(server, view);
            return;
        }
    }
}

/** Reject foreign-toplevel capture during lock; accept only visible-session requests. */
static void handle_capture_request(struct wl_listener *listener, void *data)
{
    struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *request = data;
    struct vela_view *view = request->toplevel_handle->data;
    if (!view || view->server->locked
        || (view->tree->node.capture_hidden && !vela_capture_hide_trusts(view->server, request->client))) {
        // An isolated window source can bypass visible composition; refuse
        // access to any window hidden behind the lock screen, or hidden from
        // capture (§15.2). An inert source: the client's session stops at once.
        // new_id is private in 0.20 but has no public way to answer "no" (as ready.c does with synced).
        wlr_ext_image_capture_source_v1_create_resource(NULL, request->client, request->WLR_PRIVATE.new_id);
        return;
    }
    // The current look, even if minimized or covered by other windows: the
    // renderer draws the window alone.
    struct vela_server *server = view->server;
    if (!view->capture) {
        view->capture
            = vela_window_capture_create(server->scene, &view->tree->node, view, server->renderer, server->allocator);
    }
    struct wlr_ext_image_capture_source_v1 *source = vela_window_capture_source(view->capture);
    if (source) {
        wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request_accept(request, source);
    }
}

/** Revoke active foreign-toplevel capture sources when locking the session. */
void vela_views_revoke_capture_on_lock(struct vela_server *server)
{
    struct vela_view *view;
    wl_list_for_each (view, &server->views, link) {
        vela_window_capture_destroy(view->capture);
        view->capture = NULL;
    }
}

void vela_views_init(struct vela_server *server)
{
    struct wl_display *display = server->display;
    wl_list_init(&server->views);
    server->xdg_shell = wlr_xdg_shell_create(display, 5);
    vela_listen(&server->xdg_shell->events.new_toplevel, &server->new_toplevel, handle_new_toplevel);

    // The window list for the taskbar, with activate/minimize/close.
    server->foreign_toplevels = wlr_foreign_toplevel_manager_v1_create(display);

    // Output and window capture (Alt+Tab previews, and later screen sharing):
    // the standard ext-* protocols.
    server->ext_toplevels = wlr_ext_foreign_toplevel_list_v1_create(display, 1);
    vela_capture_watch_copy_capture(server->capture_watch, wlr_ext_image_copy_capture_manager_v1_create(display, 1));
    wlr_ext_output_image_capture_source_manager_v1_create(display, 1);
    struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1 *capture
        = wlr_ext_foreign_toplevel_image_capture_source_manager_v1_create(display, 1);
    vela_listen(&capture->events.new_request, &server->new_capture_request, handle_capture_request);

    // Vela's title bar for the apps that accept it (§9.1).
    struct wlr_xdg_decoration_manager_v1 *decorations = wlr_xdg_decoration_manager_v1_create(display);
    vela_listen(&decorations->events.new_toplevel_decoration, &server->new_decoration, handle_new_decoration);

    struct wlr_xdg_activation_v1 *activation = wlr_xdg_activation_v1_create(display);
    vela_listen(&activation->events.request_activate, &server->request_activation, handle_request_activation);
}

void vela_views_finish(struct vela_server *server)
{
    vela_unlisten(&server->new_toplevel);
    vela_unlisten(&server->new_capture_request);
    vela_unlisten(&server->new_decoration);
    vela_unlisten(&server->request_activation);
    if (server->output_check) {
        wl_event_source_remove(server->output_check);
        server->output_check = NULL;
    }
    free(server->seen_outputs);
    server->seen_outputs = NULL;
    free(server->window_blur_include);
    free(server->window_blur_exclude);
    server->window_blur_include = server->window_blur_exclude = NULL;
    free_blur_apps(server);
    vela_window_rules_free(&server->window_rules);
    vela_window_memory_free(&server->window_memory);
    free(server->ssd_include);
    server->ssd_include = NULL;
}

// ------------------------------------------------------------- outputs --

void vela_views_usable_changed(struct vela_server *server, struct vela_output *output)
{
    struct vela_view *view;
    wl_list_for_each (view, &server->views, link) {
        if (view->fullscreen || vela_view_output(view) != output) {
            continue;
        }
        if (view->maximized) {
            apply_maximized(view);
        } else if (!vela_snap_is_none(view->snap)) {
            vela_view_apply_snap(view, output);
        }
    }
}

bool vela_views_fullscreen_on(struct vela_server *server, struct vela_output *output)
{
    struct vela_view *view;
    wl_list_for_each (view, &server->views, link) {
        // Alt+Tabbed out of, it is below the desktop's windows (focus.c).
        if (view->mapped && view->fullscreen && !view->minimized && vela_view_on_current_workspace(view)
            && view->tree->node.parent == server->layers.fullscreen && vela_view_output(view) == output) {
            return true;
        }
    }
    return false;
}

bool vela_views_any_fullscreen(struct vela_server *server)
{
    struct vela_view *view;
    wl_list_for_each (view, &server->views, link) {
        if (view->mapped && view->fullscreen && !view->minimized && vela_view_on_current_workspace(view)) {
            return true;
        }
    }
    return false;
}

static const struct vela_seen_output *seen_output(const struct vela_seen_output *list, int count, const char *name)
{
    for (int i = 0; i < count; ++i) {
        if (strcmp(list[i].name, name) == 0) {
            return &list[i];
        }
    }
    return NULL;
}

static int compare_seen(const void *a, const void *b)
{
    return strcmp(((const struct vela_seen_output *)a)->name, ((const struct vela_seen_output *)b)->name);
}

void vela_views_check_outputs(struct vela_server *server)
{
    // The current outputs, by name (as they will be remembered next time).
    int count = wl_list_length(&server->outputs);
    struct vela_seen_output *now = calloc((size_t)(count ? count : 1), sizeof(*now));
    int n = 0;
    struct vela_output *output;
    wl_list_for_each (output, &server->outputs, link) {
        if (output->wlr->enabled && wlr_output_layout_get(server->output_layout, output->wlr)
            && !seen_output(now, n, output->wlr->name)) {
            snprintf(now[n].name, sizeof(now[n].name), "%s", output->wlr->name);
            now[n++].box = vela_output_box(output);
        }
    }
    qsort(now, (size_t)n, sizeof(*now), compare_seen);

    struct vela_view *view;
    wl_list_for_each (view, &server->views, link) {
        if (!view->mapped) {
            continue;
        }
        struct wlr_box frame = vela_view_frame_box(view);
        double cx = frame.x + frame.width / 2.0;
        double cy = frame.y + frame.height / 2.0;
        // The output it was moved away from is back: it returns there.
        const struct vela_seen_output *home = view->home_output[0] ? seen_output(now, n, view->home_output) : NULL;
        if (home) {
            struct vela_output *back = vela_output_named(server, view->home_output);
            if (back) {
                wlr_log(WLR_INFO, "%s connected again: \"%s\" goes back there", view->home_output,
                    vela_view_title(view));
                view->home_output[0] = '\0';
                move_to_output(view, back, home->box.x + view->home_x, home->box.y + view->home_y);
            }
            continue;
        }
        if (vela_output_at(server, cx, cy)) {
            continue;
        }
        // Its output is gone: remember which it was (from last time's
        // positions) and move to the nearest output.
        const char *from = NULL;
        for (int i = 0; i < server->seen_output_count; ++i) {
            const struct wlr_box *box = &server->seen_outputs[i].box;
            if (cx >= box->x && cx < box->x + box->width && cy >= box->y && cy < box->y + box->height) {
                from = server->seen_outputs[i].name;
                view->home_x = frame.x - box->x;
                view->home_y = frame.y - box->y;
            }
        }
        double nx = 0.0;
        double ny = 0.0;
        wlr_output_layout_closest_point(server->output_layout, NULL, cx, cy, &nx, &ny);
        struct vela_output *target = vela_output_at(server, nx, ny);
        if (!target && !wl_list_empty(&server->outputs)) {
            target = wl_container_of(server->outputs.next, target, link);
        }
        if (!target || !wlr_output_layout_get(server->output_layout, target->wlr)) {
            continue; // no output: wait for one to come back
        }
        if (from && !view->home_output[0]) {
            snprintf(view->home_output, sizeof(view->home_output), "%s", from);
        }
        struct wlr_box to = vela_output_box(target);
        wlr_log(WLR_INFO, "%s disconnected: \"%s\" moves to %s", from ? from : "An output", vela_view_title(view),
            target->wlr->name);
        // At the same relative point, if it fits.
        move_to_output(view, target, to.x + (from ? view->home_x : 0), to.y + (from ? view->home_y : 0));
    }
    free(server->seen_outputs);
    server->seen_outputs = now;
    server->seen_output_count = n;
}

static void handle_output_check(void *data)
{
    struct vela_server *server = data;
    server->output_check = NULL;
    vela_views_check_outputs(server);
}

void vela_views_check_outputs_later(struct vela_server *server)
{
    if (!server->output_check) {
        server->output_check = wl_event_loop_add_idle(server->loop, handle_output_check, server);
    }
}
