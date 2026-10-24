// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "capture_watch.h"
#include "command.h"

#include "a11y.h"
#include "autoscroll.h"
#include "bindings.h"
#include "buffer.h"
#include "decoration.h"
#include "icons.h"
#include "input.h"
#include "layer.h"
#include "lock.h"
#include "output.h"
#include "output_manager.h"
#include "scene/frame.h"
#include "scene/scene.h"
#include "server.h"
#include "shell.h"
#include "shortcuts.h"
#include "switcher.h"
#include "util.h"
#include "view.h"
#include "workspace.h"

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <wlr/backend/headless.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>

#define MAX_LINE 4096 // a connection sending more is closed

// A connection to the command socket.
struct client {
    struct wl_list link; // vela_commands.clients
    struct vela_server *server;
    int fd;
    struct wl_event_source *source;
    char buffer[MAX_LINE];
    size_t length;
};

struct vela_commands {
    int fd;
    struct wl_event_source *source;
    char path[108];
    struct wl_list clients;
    struct client *picker; // waiting for "pick-window"'s answer
    struct wl_event_source *pick_timer;
};

static void client_destroy(struct client *client);

#define PICK_TIMEOUT_MS 30000

bool vela_commands_picking(const struct vela_server *server)
{
    return server->commands && server->commands->picker;
}

void vela_commands_pick_done(struct vela_server *server, struct vela_view *view)
{
    struct vela_commands *commands = server->commands;
    if (!commands || !commands->picker) {
        return;
    }
    struct client *picker = commands->picker;
    commands->picker = NULL;
    wl_event_source_timer_update(commands->pick_timer, 0);
    struct vela_buffer reply = { 0 };
    if (view) {
        struct vela_output *out = vela_view_output(view);
        vela_buffer_append(&reply, "{\"appId\":");
        vela_buffer_append_json(&reply, vela_view_app_id(view));
        vela_buffer_append(&reply, ",\"title\":");
        vela_buffer_append_json(&reply, vela_view_title(view));
        vela_buffer_append(&reply, ",\"output\":");
        vela_buffer_append_json(&reply, out ? out->wlr->name : "");
        vela_buffer_append(&reply, "}\n");
    } else {
        vela_buffer_append(&reply, "cancel\n");
    }
    (void)!write(picker->fd, reply.data, reply.length);
    vela_buffer_finish(&reply);
    client_destroy(picker);
    vela_input_set_xcursor(server, "default");
}

static int pick_timeout(void *data)
{
    vela_commands_pick_done(data, NULL);
    return 0;
}

static void pick_begin(struct vela_server *server, struct client *client)
{
    struct vela_commands *commands = server->commands;
    vela_commands_pick_done(server, NULL); // a previous one is cancelled
    if (!commands->pick_timer) {
        commands->pick_timer = wl_event_loop_add_timer(server->loop, pick_timeout, server);
    }
    commands->picker = client;
    wl_event_source_timer_update(commands->pick_timer, PICK_TIMEOUT_MS);
    vela_input_set_xcursor(server, "crosshair");
}

static bool starts_with(const char *text, const char *prefix)
{
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

// Apply a caffeine request on the compositor thread. The confirmed command
// socket variant returns the authoritative resulting state to the shell.
static bool set_caffeine_command(struct vela_server *server, const char *arg)
{
    if (starts_with(arg, "on ")) {
        const char *value = arg + 3;
        char *end = NULL;
        errno = 0;
        const double minutes = strtod(value, &end);
        if (end == value || *end != '\0' || errno == ERANGE || !isfinite(minutes)
            || minutes <= 0 || minutes > VELA_CAFFEINE_MAX_MINUTES) {
            wlr_log(WLR_ERROR, "Invalid caffeine duration (expected >0 to 1440 minutes)");
            return false; // Never mutate existing state for invalid input.
        }
        if (!vela_lock_set_caffeine_for(server->lock, minutes)) {
            vela_a11y_announce(server->a11y); // Includes fail-closed timer errors.
            return false;
        }
    } else if (strcmp(arg, "on") == 0 || strcmp(arg, "off") == 0 || strcmp(arg, "toggle") == 0) {
        const bool on = strcmp(arg, "toggle") == 0 ? !server->lock->caffeine : strcmp(arg, "on") == 0;
        vela_lock_set_caffeine(server->lock, on);
    } else {
        wlr_log(WLR_ERROR, "Unknown caffeine command");
        return false;
    }
    vela_a11y_announce(server->a11y);
    return true;
}

// -------------------------------------------------------------- answers --

// Position of the window in the scene, from the bottom (fullscreen ones are in
// a layer above): tests read the stacking order from it.
static int stack_index(struct vela_server *server, const struct vela_view *view)
{
    int index = 0;
    struct vela_node *node;
    wl_list_for_each (node, &server->layers.windows->children, link) {
        if (node == &view->tree->node) {
            return index;
        }
        ++index;
    }
    return -1;
}

static void state_json(struct vela_server *server, struct vela_buffer *json)
{
    const char *t = "true";
    const char *f = "false";
    struct vela_view *focused = vela_views_focused(server);
    vela_buffer_appendf(json, "{\"locked\":%s,\"workspace\":%d,\"workspaces\":%d,\"focused\":",
        server->locked ? t : f, server->workspaces->current, server->workspaces->count);
    if (focused && focused->ext_handle && focused->ext_handle->identifier) {
        vela_buffer_append_json(json, focused->ext_handle->identifier);
    } else {
        vela_buffer_append(json, "null");
    }
    vela_buffer_appendf(json, ",\"held\":%llu,\"outputs\":[", (unsigned long long)server->ready.held_total);
    bool first = true;
    struct vela_output *output;
    wl_list_for_each (output, &server->outputs, link) {
        struct wlr_box box = vela_output_box(output);
        vela_buffer_append(json, first ? "{\"name\":" : ",{\"name\":");
        vela_buffer_append_json(json, output->wlr->name);
        vela_buffer_appendf(json,
            ",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"scale\":%g,\"transform\":%d,\"enabled\":%s,\"powered\":%s,\"frames\":%llu,"
            "\"missed\":%llu,\"capturePasses\":%llu,\"scanout\":%s}",
            box.x, box.y, box.width, box.height, (double)output->wlr->scale, (int)output->wlr->transform, output->wlr->enabled ? t : f,
            output->powered ? t : f, (unsigned long long)output->clock.frames_total,
            (unsigned long long)output->clock.missed_total,
            (unsigned long long)(output->frame ? output->frame->capture_passes : 0),
            output->frame && output->frame->scanout ? t : f);
        first = false;
    }
    vela_buffer_append(json, "],\"windows\":[");
    first = true;
    struct vela_view *view; // order of recent use: the first is on top
    wl_list_for_each (view, &server->views, link) {
        if (!view->mapped || !view->ext_handle || !view->ext_handle->identifier) {
            continue;
        }
        struct wlr_box frame = vela_view_frame_box(view);
        struct vela_output *out = vela_view_output(view);
        vela_buffer_append(json, first ? "{\"id\":" : ",{\"id\":");
        vela_buffer_append_json(json, view->ext_handle->identifier);
        vela_buffer_append(json, ",\"app\":");
        vela_buffer_append_json(json, vela_view_app_id(view));
        vela_buffer_append(json, ",\"title\":");
        vela_buffer_append_json(json, vela_view_title(view));
        vela_buffer_appendf(json, ",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"output\":", frame.x, frame.y,
            frame.width, frame.height);
        if (out) {
            vela_buffer_append_json(json, out->wlr->name);
        } else {
            vela_buffer_append(json, "null");
        }
        vela_buffer_appendf(json,
            ",\"workspace\":%d,\"sticky\":%s,\"maximized\":%s,\"minimized\":%s,\"fullscreen\":%s,\"below\":%s,\"snap\":",
            view->workspace, view->sticky ? t : f, view->maximized ? t : f, view->minimized ? t : f,
            view->fullscreen ? t : f, vela_view_stays_below(view) ? t : f);
        if (vela_snap_is_none(view->snap)) {
            vela_buffer_append(json, "null");
        } else {
            vela_buffer_appendf(json, "[%d,%d,%d,%d]", view->snap.x0, view->snap.y0, view->snap.x1, view->snap.y1);
        }
        vela_buffer_appendf(json, ",\"snapGroup\":%u,\"alwaysOnTop\":%s,\"captureHidden\":%s,\"z\":%d}",
            view->snap_group, view->always_on_top ? t : f, view->tree->node.capture_hidden ? t : f,
            stack_index(server, view));
        first = false;
    }
    // The layer-shell surfaces, bottom to top, and the one with the keyboard.
    vela_buffer_append(json, "],\"layers\":[");
    first = true;
    struct vela_layer_surface *layer;
    struct vela_layer_surface *keyboard_layer = NULL;
    struct wlr_surface *keyboard_surface = server->seat->keyboard_state.focused_surface;
    wl_list_for_each (layer, &server->layer_surfaces, link) {
        const struct wlr_layer_surface_v1 *wlr = layer->wlr;
        struct vela_output *out = vela_layer_surface_output(layer);
        vela_buffer_append(json, first ? "{\"namespace\":" : ",{\"namespace\":");
        vela_buffer_append_json(json, wlr->namespace ? wlr->namespace : "");
        vela_buffer_appendf(json,
            ",\"layer\":%d,\"keyboard\":%d,\"mapped\":%s,\"shell\":%s,\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"output\":",
            (int)wlr->current.layer, (int)wlr->current.keyboard_interactive, wlr->surface->mapped ? t : f,
            vela_capture_hide_trusts(server, wl_resource_get_client(wlr->resource)) ? t : f,
            (int)lround(layer->tree->node.x), (int)lround(layer->tree->node.y), wlr->surface->current.width, wlr->surface->current.height);
        if (out) {
            vela_buffer_append_json(json, out->wlr->name);
        } else {
            vela_buffer_append(json, "null");
        }
        double lx = 0, ly = 0;
        vela_node_coords(&layer->tree->node, &lx, &ly);
        vela_buffer_appendf(json, ",\"x\":%g,\"y\":%g,\"w\":%d,\"h\":%d}", lx, ly,
            wlr->surface->current.width, wlr->surface->current.height);
        if (keyboard_surface && wlr->surface == keyboard_surface) {
            keyboard_layer = layer;
        }
        first = false;
    }
    vela_buffer_append(json, "],\"focusedLayer\":");
    if (keyboard_layer) {
        vela_buffer_append_json(json, keyboard_layer->wlr->namespace ? keyboard_layer->wlr->namespace : "");
    } else {
        vela_buffer_append(json, "null");
    }
    vela_buffer_appendf(json, ",\"autoscroll\":%s,\"cursor\":", vela_autoscroll_active(server->autoscroll) ? t : f);
    if (server->cursor_name) {
        vela_buffer_append_json(json, server->cursor_name);
    } else {
        vela_buffer_append(json, "null");
    }
    vela_buffer_append(json, "}");
}

// The visible windows of the current desktop, topmost first, with their frame
// (bar included): for the Snipping Tool.
static void window_rects_json(struct vela_server *server, struct vela_buffer *json)
{
    // [{"id":...,"title":...,"x":..,"y":..,"w":..,"h":..}], topmost first.
    vela_buffer_append(json, "[");
    bool first = true;
    struct vela_view *view;
    wl_list_for_each (view, &server->views, link) {
        if (!view->mapped || view->minimized || !vela_view_on_current_workspace(view) || !view->ext_handle) {
            continue;
        }
        struct wlr_box frame = vela_view_frame_box(view);
        vela_buffer_append(json, first ? "{\"id\":" : ",{\"id\":");
        vela_buffer_append_json(json, view->ext_handle->identifier);
        vela_buffer_append(json, ",\"title\":");
        vela_buffer_append_json(json, vela_view_title(view));
        vela_buffer_appendf(json, ",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d}", frame.x, frame.y, frame.width,
            frame.height);
        first = false;
    }
    vela_buffer_append(json, "]");
}

// Questions with an answer: false if the line isn't one of them.
static bool answer(struct vela_server *server, int fd, const char *line)
{
    struct vela_buffer reply = { 0 };
    if (strcmp(line, "modifiers") == 0) {
        // The shell doesn't have the keyboard when the taskbar is clicked, so
        // it doesn't know whether Shift is held (Shift+right click: the window
        // menu).
        struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
        vela_buffer_appendf(&reply, "%u", keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0);
    } else if (strcmp(line, "workspaces") == 0) {
        vela_workspaces_json(server, &reply); // for a shell that has just started
    } else if (strcmp(line, "window-rects") == 0) {
        window_rects_json(server, &reply);
    } else if (strcmp(line, "state") == 0) {
        state_json(server, &reply);
    } else if (strcmp(line, "caffeine status") == 0) {
        vela_buffer_append(&reply, server->lock->caffeine ? "on" : "off");
    } else if (starts_with(line, "caffeine-set ")) {
        // Unlike the fire-and-forget CLI, the shell waits for this reply.
        // Apply first, then report the resulting compositor state, not the
        // requested one. A failed delivery cannot masquerade as "off".
        const bool ok = set_caffeine_command(server, line + 13);
        char json[512];
        if (!vela_a11y_json(server->a11y, json, sizeof(json))) {
            return true;
        }
        vela_buffer_appendf(&reply, "{\"ok\":%s,\"state\":%s}", ok ? "true" : "false", json);
    } else if (strcmp(line, "always-on-top status") == 0) {
        struct vela_view *active = vela_views_focused(server);
        vela_buffer_append(&reply, active && active->always_on_top ? "on" : "off");
    } else if (strcmp(line, "accessibility") == 0) {
        char json[512];
        if (!vela_a11y_json(server->a11y, json, sizeof(json))) {
            return true; // doesn't fit: no answer
        }
        vela_buffer_append(&reply, json);
    } else {
        return false;
    }
    vela_buffer_append(&reply, "\n");
    (void)!send(fd, reply.data, reply.length, MSG_NOSIGNAL);
    vela_buffer_finish(&reply);
    return true;
}

// ------------------------------------------------------------ commands --

static void refresh_decorations(struct vela_server *server)
{
    struct vela_view *view;
    wl_list_for_each (view, &server->views, link) {
        vela_view_refresh_decoration(view);
    }
}

// The title bar icons again if the icon theme changed (icon-theme= in
// vela.conf, or the mode picking its dark variant).
static void reload_icons(struct vela_server *server)
{
    if (!server->icons) {
        return; // not loaded yet: the first title bar reads the theme
    }
    struct vela_icons *fresh = vela_icons_create(server->light_apps);
    if (strcmp(vela_icons_theme(fresh), vela_icons_theme(server->icons)) == 0) {
        vela_icons_destroy(fresh);
        return;
    }
    vela_icons_destroy(server->icons); // the bars keep copies of their icons
    server->icons = fresh;
    ++server->wallpaper_tint_version;
    refresh_decorations(server);
}

// wallpaper-tint R G B (0-255): the wallpaper's average color, for the title
// bars.
static void wallpaper_tint(struct vela_server *server, const char *arguments)
{
    int r = 0;
    int g = 0;
    int b = 0;
    if (sscanf(arguments, "%d %d %d", &r, &g, &b) != 3) {
        return;
    }
    server->wallpaper_tint[0] = vela_clamp(r, 0, 255) / 255.0f;
    server->wallpaper_tint[1] = vela_clamp(g, 0, 255) / 255.0f;
    server->wallpaper_tint[2] = vela_clamp(b, 0, 255) / 255.0f;
    server->has_wallpaper_tint = true;
    ++server->wallpaper_tint_version;
    refresh_decorations(server);
}

// theme <shell> <apps> [<accent>]: "light" or "dark", Vela's mode and the
// apps' mode, and the accent as #rrggbb (without it the accent stays).
static void theme(struct vela_server *server, const char *arguments)
{
    char shell_mode[16] = "";
    char app_mode[16] = "";
    char accent_text[16] = "";
    if (sscanf(arguments, "%15s %15s %15s", shell_mode, app_mode, accent_text) < 2) {
        return;
    }
    unsigned rgb = 0;
    if (strlen(accent_text) == 7 && sscanf(accent_text, "#%6x", &rgb) == 1) {
        server->accent[0] = ((rgb >> 16) & 0xff) / 255.0f;
        server->accent[1] = ((rgb >> 8) & 0xff) / 255.0f;
        server->accent[2] = (rgb & 0xff) / 255.0f;
    }
    bool shell_light = strcmp(shell_mode, "light") == 0;
    bool apps_light = strcmp(app_mode, "light") == 0;
    if (shell_light == server->light_shell && apps_light == server->light_apps) {
        return;
    }
    server->light_shell = shell_light;
    server->light_apps = apps_light;
    // Windows 11's acrylic, dark or light: WinUI's
    // AcrylicBackgroundFillColorDefaultBrush (#2C2C2C at 0.15, luminosity
    // 0.96; #FCFCFC at 0, luminosity 0.85).
    server->scene->acrylic_tint = shell_light
        ? (struct wlr_render_color) { 0.988f, 0.988f, 0.988f, 0.0f }
        : (struct wlr_render_color) { 0.173f, 0.173f, 0.173f, 0.15f };
    server->scene->acrylic_luminosity = shell_light ? 0.85f : 0.96f;
    ++server->wallpaper_tint_version;
    reload_icons(server);
    refresh_decorations(server);
    struct vela_output *output;
    wl_list_for_each (output, &server->outputs, link) {
        vela_output_frame_damage_whole(output->frame);
    }
}

// workspace switch|close <n>, workspace new [switch], workspace rename <n>
// <name>, workspace move <from> <to>: from Task View.
static void workspace(struct vela_server *server, const char *line)
{
    char verb[16] = "";
    int a = -1;
    int b = -1;
    int consumed = 0;
    sscanf(line + 10, "%15s %n", verb, &consumed);
    size_t skip = vela_min((int)strlen(line), 10 + consumed);
    const char *rest = line + skip;
    if (strcmp(verb, "switch") == 0 && sscanf(rest, "%d", &a) == 1) {
        vela_workspaces_switch(server, a, true);
    } else if (strcmp(verb, "new") == 0) {
        int index = vela_workspaces_add(server);
        if (strcmp(rest, "switch") == 0) {
            vela_workspaces_switch(server, index, true);
        }
    } else if (strcmp(verb, "close") == 0 && sscanf(rest, "%d", &a) == 1) {
        vela_workspaces_remove(server, a);
    } else if (strcmp(verb, "rename") == 0 && sscanf(rest, "%d %n", &a, &consumed) >= 1) {
        vela_workspaces_rename(server, a, rest + vela_min((int)strlen(rest), consumed));
    } else if (strcmp(verb, "move") == 0 && sscanf(rest, "%d %d", &a, &b) == 2) {
        vela_workspaces_move(server, a, b);
    }
}

// on, off or toggle.
static bool wanted(const char *what, bool now)
{
    return strcmp(what, "toggle") == 0 ? !now : strcmp(what, "on") == 0;
}

// window <ext-foreign-toplevel identifier | active> <action>: from the window
// menu.
static void window(struct vela_server *server, const char *line)
{
    const char *id = line + 7;
    const char *space = strchr(id, ' ');
    if (!space) {
        return;
    }
    size_t id_length = (size_t)(space - id);
    struct vela_view *target = NULL;
    if (id_length == 6 && strncmp(id, "active", 6) == 0) {
        target = vela_views_focused(server);
    } else {
        struct vela_view *view;
        wl_list_for_each (view, &server->views, link) {
            const char *identifier = view->ext_handle ? view->ext_handle->identifier : NULL;
            if (identifier && strlen(identifier) == id_length && strncmp(identifier, id, id_length) == 0) {
                target = view;
            }
        }
    }
    if (target && !server->locked) {
        vela_window_action(server, target, space + 1);
    }
}

// "End task": kills the processes of that app's windows at once, without
// asking (like Windows).
static void end_task(struct vela_server *server, const char *app_id)
{
    int count = wl_list_length(&server->views);
    pid_t *pids = calloc((size_t)count + 1, sizeof(*pids));
    int n = 0;
    struct vela_view *view;
    wl_list_for_each (view, &server->views, link) {
        pid_t pid = vela_view_pid(view);
        bool known = false;
        for (int i = 0; i < n; ++i) {
            known = known || pids[i] == pid;
        }
        if (strcmp(app_id, vela_view_app_id(view)) == 0 && pid > 1 && pid != getpid() && !known) {
            pids[n++] = pid;
        }
    }
    for (int i = 0; i < n; ++i) {
        wlr_log(WLR_INFO, "End task: %s (process %d)", app_id, (int)pids[i]);
        kill(pids[i], SIGKILL);
    }
    free(pids);
}

static void run_command(struct vela_server *server, const char *line)
{
    struct vela_a11y *a11y = server->a11y;
    if (strcmp(line, "logout") == 0) {
        wlr_log(WLR_INFO, "Exit requested by the shell");
        wl_display_terminate(server->display);
    } else if (strcmp(line, "restart-shell") == 0) {
        wlr_log(WLR_INFO, "Shell restart requested");
        vela_shell_restart(server);
    } else if (starts_with(line, "wallpaper-tint ")) {
        wallpaper_tint(server, line + 15);
    } else if (starts_with(line, "theme ")) {
        theme(server, line + 6);
    } else if (starts_with(line, "blur-look ")) {
        float saturation = 1.0f;
        float strength = 1.0f;
        float luminosity = -1.0f;
        float tint = -1.0f;
        if (sscanf(line + 10, "%f %f %f %f", &saturation, &strength, &luminosity, &tint) >= 2) {
            vela_blur_set_look(saturation, strength, luminosity, tint);
            struct vela_output *output;
            wl_list_for_each (output, &server->outputs, link) {
                vela_output_frame_damage_whole(output->frame);
            }
        }
    } else if (starts_with(line, "blur-look-for ")) {
        // blur-look-for <scope> <saturation> <strength> <luminosity> <tint>
        char scope[64];
        float v[4];
        if (sscanf(line + 14, "%63s %f %f %f %f", scope, &v[0], &v[1], &v[2], &v[3]) == 5) {
            struct vela_blur_look look = { true, vela_clampf(v[0], 1.0f, 3.0f), vela_clampf(v[1], 0.25f, 3.0f),
                v[2] < 0.0f ? -1.0f : vela_clampf(v[2], 0.0f, 1.0f), v[3] < 0.0f ? -1.0f : vela_clampf(v[3], 0.0f, 1.0f) };
            struct vela_panel_look *entry = NULL;
            for (int i = 0; i < server->panel_look_count; ++i) {
                if (strcmp(server->panel_looks[i].scope, scope) == 0) {
                    entry = &server->panel_looks[i];
                }
            }
            if (!entry) {
                struct vela_panel_look *grown = realloc(server->panel_looks,
                    sizeof(*grown) * (size_t)(server->panel_look_count + 1));
                if (!grown) {
                    return;
                }
                server->panel_looks = grown;
                entry = &grown[server->panel_look_count++];
                entry->scope = strdup(scope);
            }
            entry->look = look;
            struct vela_layer_surface *layer;
            wl_list_for_each (layer, &server->layer_surfaces, link) {
                vela_layer_update_look(layer);
            }
            struct vela_output *output;
            wl_list_for_each (output, &server->outputs, link) {
                vela_output_frame_damage_whole(output->frame);
            }
        }
    } else if (starts_with(line, "taskbar-autohide ")) {
        const bool on = strcmp(line + 17, "yes") == 0;
        if (on != server->taskbar_autohide) {
            server->taskbar_autohide = on;
            struct vela_output *output;
            wl_list_for_each (output, &server->outputs, link) {
                vela_output_arrange_layers(output);
            }
        }
    } else if (starts_with(line, "taskbar-position ")) {
        const char *edge = line + 17;
        server->taskbar_edge = strcmp(edge, "top") == 0     ? VELA_TASKBAR_TOP
            : strcmp(edge, "left") == 0                     ? VELA_TASKBAR_LEFT
            : strcmp(edge, "right") == 0                    ? VELA_TASKBAR_RIGHT
                                                            : VELA_TASKBAR_BOTTOM;
    } else if (strcmp(line, "paste") == 0) {
        vela_input_paste(server->input); // Win+V: the shell has put the item on the clipboard
    } else if (strcmp(line, "modifiers") == 0 || strcmp(line, "workspaces") == 0
        || strcmp(line, "accessibility") == 0 || strcmp(line, "window-rects") == 0
        || strcmp(line, "caffeine status") == 0 || starts_with(line, "caffeine-set ")
        || strcmp(line, "always-on-top status") == 0 || strcmp(line, "pick-window") == 0) {
        // already answered to whoever asked
    } else if (starts_with(line, "workspace ")) {
        workspace(server, line);
    } else if (strcmp(line, "reload-config") == 0) {
        // Settings changed vela.conf.
        wlr_log(WLR_INFO, "Settings: reloading vela.conf");
        vela_lock_load_settings(server->lock);
        vela_input_reload(server->input); // keyboards, mice and touchpads
        vela_autoscroll_load_settings(server->autoscroll);
        vela_output_load_settings(server);
        vela_views_load_settings(server); // window blur, title bars for the listed app ids
        vela_a11y_load(a11y);
        vela_shortcuts_load();
        vela_workspaces_load_settings(server);
        vela_capture_watch_load(server->capture_watch);
        reload_icons(server);
    } else if (starts_with(line, "night-light ")) {
        // From quick settings: night-light on|off|toggle (and likewise
        // filters, magnifier, sticky keys).
        vela_a11y_set_night_light(a11y, wanted(line + 12, a11y->night_light), true);
    } else if (starts_with(line, "color-filter ")) {
        vela_a11y_set_color_filter(a11y, wanted(line + 13, a11y->color_filter), true);
    } else if (starts_with(line, "magnifier ")) {
        vela_a11y_set_magnifier(a11y, wanted(line + 10, a11y->magnifier));
    } else if (starts_with(line, "sticky-keys ")) {
        vela_a11y_set_sticky_keys(a11y, wanted(line + 12, a11y->sticky_keys), true);
    } else if (starts_with(line, "caffeine ")) {
        // Existing CLI remains fire-and-forget; only the shell uses the
        // caffeine-set command with a synchronized state reply.
        (void)set_caffeine_command(server, line + 9);
    } else if (starts_with(line, "always-on-top ")) {
        // on|off|toggle for the active window (also "window <id> always-on-top ...").
        struct vela_view *active = vela_views_focused(server);
        if (active && !server->locked) {
            vela_view_set_always_on_top(active, wanted(line + 14, active->always_on_top));
        }
    } else if (starts_with(line, "switcher-pick ")) {
        // A click on an Alt+Tab preview: switch to that window.
        vela_switcher_pick(server, atoi(line + 14));
    } else if (starts_with(line, "test-output ")) {
        vela_test_output_command(server, line + 12);
    } else if (strcmp(line, "test-power off") == 0 || strcmp(line, "test-power on") == 0) {
        // For tests: turns outputs off and on like inactivity does, headless
        // ones only (like test-output).
        struct vela_output *output;
        wl_list_for_each (output, &server->outputs, link) {
            if (wlr_output_is_headless(output->wlr)) {
                vela_output_set_powered(output, strcmp(line, "test-power on") == 0);
            }
        }
    } else if (strcmp(line, "lock") == 0) {
        vela_lock_screen(server->lock); // for example before suspending the computer
    } else if (strcmp(line, "minimize-all") == 0) {
        // Toggle: if there are non-minimized views, minimize them all and
        // remember the list (insert at head so restore order is least recent
        // first, active last -> on top). If all are minimized, restore them
        // in that order.
        bool any_visible = false;
        struct vela_view *view;
        wl_list_for_each (view, &server->views, link) {
            if (view->mapped && !view->minimized) {
                any_visible = true;
                break;
            }
        }
        if (any_visible) {
            wl_list_for_each (view, &server->views, link) {
                if (view->mapped && !view->minimized) {
                    vela_view_set_minimized(view, true);
                    // Minimized again after a manual restore: still listed.
                    wl_list_remove(&view->minimized_link);
                    wl_list_insert(&server->minimized_by_desktop, &view->minimized_link);
                }
            }
        } else {
            struct vela_view *next;
            wl_list_for_each_safe (view, next, &server->minimized_by_desktop, minimized_link) {
                wl_list_remove(&view->minimized_link);
                wl_list_init(&view->minimized_link);
                if (view->mapped) {
                    vela_view_set_minimized(view, false);
                }
            }
        }
    } else if (starts_with(line, "window ")) {
        window(server, line);
    } else if (starts_with(line, "end-task ")) {
        end_task(server, line + 9);
    } else if (line[0]) {
        wlr_log(WLR_DEBUG, "Unknown command: %s", line);
    }
}

// --------------------------------------------------------------- socket --

static void client_destroy(struct client *client)
{
    struct vela_commands *commands = client->server->commands;
    if (commands->picker == client) {
        commands->picker = NULL;
        wl_event_source_timer_update(commands->pick_timer, 0);
        vela_input_set_xcursor(client->server, "default");
    }
    wl_list_remove(&client->link);
    wl_event_source_remove(client->source);
    close(client->fd);
    free(client);
}

static int handle_client(int fd, uint32_t mask, void *data)
{
    struct client *client = data;
    struct vela_server *server = client->server;
    char chunk[256];
    ssize_t n = 0;
    while ((n = read(fd, chunk, sizeof(chunk))) > 0 && client->length < MAX_LINE) {
        size_t room = MAX_LINE - client->length;
        size_t take = (size_t)n < room ? (size_t)n : room;
        memcpy(client->buffer + client->length, chunk, take);
        client->length += take;
    }
    // The complete lines; the rest waits for the next piece.
    char lines[MAX_LINE + 1];
    size_t lines_length = 0;
    char *newline;
    while ((newline = memchr(client->buffer, '\n', client->length))) {
        size_t line_length = (size_t)(newline - client->buffer);
        memcpy(lines + lines_length, client->buffer, line_length);
        lines[lines_length + line_length] = '\0';
        lines_length += line_length + 1;
        client->length -= line_length + 1;
        memmove(client->buffer, newline + 1, client->length);
    }
    for (size_t at = 0; at < lines_length; at += strlen(lines + at) + 1) {
        if (strcmp(lines + at, "pick-window") == 0) {
            pick_begin(server, client);
        } else {
            answer(server, fd, lines + at);
        }
    }
    bool closed = n == 0 || (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) || client->length >= MAX_LINE;
    if (closed) {
        client_destroy(client);
    }
    // After the client is settled: a command can close everything.
    for (size_t at = 0; at < lines_length; at += strlen(lines + at) + 1) {
        run_command(server, lines + at);
    }
    return 0;
}

static int handle_connection(int fd, uint32_t mask, void *data)
{
    struct vela_server *server = data;
    int client_fd = accept4(fd, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (client_fd < 0) {
        return 0;
    }
    struct client *client = calloc(1, sizeof(*client));
    client->server = server;
    client->fd = client_fd;
    client->source = wl_event_loop_add_fd(server->loop, client_fd, WL_EVENT_READABLE, handle_client, client);
    wl_list_insert(server->commands->clients.prev, &client->link);
    return 0;
}

void vela_commands_listen(struct vela_server *server)
{
    struct vela_commands *commands = calloc(1, sizeof(*commands));
    commands->fd = -1;
    wl_list_init(&commands->clients);
    server->commands = commands;
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir
        || !vela_format(commands->path, sizeof(commands->path), "%s/vela-%s.sock", runtime_dir,
            server->socket_name)) {
        commands->path[0] = '\0';
        return;
    }
    struct sockaddr_un address = { .sun_family = AF_UNIX };
    snprintf(address.sun_path, sizeof(address.sun_path), "%s", commands->path);
    unlink(commands->path); // left over from a previous run
    commands->fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (commands->fd < 0 || bind(commands->fd, (struct sockaddr *)&address, sizeof(address)) != 0
        || listen(commands->fd, 4) != 0) {
        wlr_log_errno(WLR_ERROR, "Can't listen for commands on %s", commands->path);
        if (commands->fd >= 0) {
            close(commands->fd);
            commands->fd = -1;
            unlink(commands->path);
        }
        return;
    }
    chmod(commands->path, 0600);
    commands->source = wl_event_loop_add_fd(server->loop, commands->fd, WL_EVENT_READABLE, handle_connection, server);
}

void vela_commands_stop(struct vela_server *server)
{
    struct vela_commands *commands = server->commands;
    if (!commands) {
        return;
    }
    struct client *client, *next;
    wl_list_for_each_safe (client, next, &commands->clients, link) {
        client_destroy(client);
    }
    if (commands->pick_timer) {
        wl_event_source_remove(commands->pick_timer);
    }
    if (commands->source) {
        wl_event_source_remove(commands->source);
    }
    if (commands->fd >= 0) {
        close(commands->fd);
        unlink(commands->path);
    }
    free(commands);
    server->commands = NULL;
}
