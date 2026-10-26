// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "snap.h"

#include "buffer.h"
#include "decoration.h"
#include "focus.h"
#include "interact.h"
#include "output.h"
#include "scene/scene.h"
#include "shell.h"
#include "server.h"
#include "shortcuts.h"
#include "view.h"
#include "workspace.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_seat.h>
#include <xkbcommon/xkbcommon.h>
#include <wlr/util/edges.h>

struct vela_area vela_snap_area(const struct vela_output *output, struct vela_snap snap)
{
    const struct wlr_box area = vela_output_physical_usable(output);
    int x0 = area.x + (int)lround(area.width * snap.x0 / 12.0);
    int y0 = area.y + (int)lround(area.height * snap.y0 / 12.0);
    int x1 = area.x + (int)lround(area.width * snap.x1 / 12.0);
    int y1 = area.y + (int)lround(area.height * snap.y1 / 12.0);
    return vela_output_from_physical(output, (struct wlr_box) { x0, y0, x1 - x0, y1 - y0 });
}

// "Tiled" tells the app to drop shadows and rounded corners on the sides
// touching the output edges.
static uint32_t tiled_edges(struct vela_snap snap)
{
    uint32_t edges = WLR_EDGE_NONE;
    edges |= snap.x0 == 0 ? WLR_EDGE_LEFT : 0;
    edges |= snap.x1 == 12 ? WLR_EDGE_RIGHT : 0;
    edges |= snap.y0 == 0 ? WLR_EDGE_TOP : 0;
    edges |= snap.y1 == 12 ? WLR_EDGE_BOTTOM : 0;
    return edges;
}

void vela_view_apply_snap(struct vela_view *view, struct vela_output *output)
{
    if (!output || vela_snap_is_none(view->snap)) {
        return;
    }
    struct vela_area area = vela_snap_area(output, view->snap);
    vela_view_send_tiled(view, tiled_edges(view->snap));
    struct vela_placement place = vela_output_place(output, area);
    vela_view_configure_size(view, place.width, place.height);
    struct wlr_box geometry = vela_view_geometry(view);
    vela_node_set_position(&view->tree->node, place.x - geometry.x, place.y - geometry.y);
}

void vela_view_set_snap(struct vela_view *view, struct vela_snap side, struct vela_output *output)
{
    if (!view->mapped || view->fullscreen || !vela_view_configurable(view)) {
        return;
    }
    vela_view_finish_open_animation(view);
    if (!vela_snap_equal(side, view->snap)) {
        vela_view_leave_snap_group(view); // moved elsewhere: it's no longer with its group
    }
    if (vela_snap_is_none(side)) {
        if (vela_snap_is_none(view->snap)) {
            return;
        }
        view->snap = vela_snap_none;
        vela_view_send_tiled(view, WLR_EDGE_NONE);
        struct wlr_box back = vela_view_restore_box(view);
        vela_view_configure_size(view, back.width, back.height);
        if (back.width > 0) {
            struct wlr_box geometry = vela_view_geometry(view);
            vela_node_set_position(&view->tree->node, back.x - geometry.x, back.y - geometry.y);
        }
        return;
    }
    if (!vela_snap_valid(side)) {
        return;
    }
    // The size to restore is the "free" one, not that of another snap or of
    // the maximized window.
    if (vela_snap_is_none(view->snap) && !view->maximized) {
        view->restore = vela_view_frame_box(view);
    }
    if (view->maximized) {
        view->maximized = false;
        vela_view_send_maximized(view, false);
        if (view->handle) {
            wlr_foreign_toplevel_handle_v1_set_maximized(view->handle, false);
        }
    }
    view->snap = side;
    vela_view_apply_snap(view, output ? output : vela_view_output(view));
}

void vela_view_leave_snap_group(struct vela_view *view)
{
    uint32_t group = view->snap_group;
    view->snap_group = 0;
    if (group == 0) {
        return;
    }
    // A group of one window is no longer a group.
    int rest = 0;
    struct vela_view *other;
    wl_list_for_each (other, &view->server->views, link) {
        rest += other->snap_group == group;
    }
    if (rest < 2) {
        wl_list_for_each (other, &view->server->views, link) {
            if (other->snap_group == group) {
                other->snap_group = 0;
            }
        }
    }
    vela_workspaces_announce(view->server);
}

// ------------------------------------------------------------ Win+arrows --

static bool is_half_column(struct vela_snap s)
{
    return vela_snap_equal(s, vela_snap_left) || vela_snap_equal(s, vela_snap_right);
}

static bool is_quarter(struct vela_snap s)
{
    return vela_snap_equal(s, vela_snap_top_left) || vela_snap_equal(s, vela_snap_top_right)
        || vela_snap_equal(s, vela_snap_bottom_left) || vela_snap_equal(s, vela_snap_bottom_right);
}

// Like Windows 11: Win+←/→ to the half (toward the opposite side it unsnaps);
// from a half, Win+↑/↓ to the top or bottom quarter; from the top quarter
// Win+↑ maximizes, from the bottom one Win+↓ minimizes.
void vela_snap_keyboard(struct vela_server *server, struct vela_view *view, uint32_t sym)
{
    struct vela_snap s = view->snap;
    bool left_column = s.x0 == 0 && s.x1 == 6;
    bool right_column = s.x0 == 6 && s.x1 == 12;
    if (sym == XKB_KEY_Left || sym == XKB_KEY_Right) {
        bool to_left = sym == XKB_KEY_Left;
        if ((to_left && right_column) || (!to_left && left_column)) {
            vela_view_set_snap(view, vela_snap_none, NULL);
        } else if (!left_column && !right_column) {
            vela_view_set_snap(view, to_left ? vela_snap_left : vela_snap_right, NULL);
            vela_snap_offer_assist(server, view);
        }
        return; // it's already on that side
    }
    if (sym == XKB_KEY_Up) {
        if (is_half_column(s)) {
            vela_view_set_snap(view, (struct vela_snap) { s.x0, 0, s.x1, 6 }, NULL);
            vela_snap_offer_assist(server, view);
        } else if (is_quarter(s) && s.y0 == 6) {
            vela_view_set_snap(view, (struct vela_snap) { s.x0, 0, s.x1, 12 }, NULL);
        } else {
            vela_view_set_maximized(view, true, true);
        }
        return;
    }
    // Down: first restore, then minimize.
    if (view->maximized) {
        vela_view_set_maximized(view, false, true);
    } else if (is_half_column(s)) {
        vela_view_set_snap(view, (struct vela_snap) { s.x0, 6, s.x1, 12 }, NULL);
        vela_snap_offer_assist(server, view);
    } else if (is_quarter(s) && s.y0 == 0) {
        vela_view_set_snap(view, (struct vela_snap) { s.x0, 0, s.x1, 12 }, NULL);
    } else if (!vela_snap_is_none(s) && !(is_quarter(s) && s.y0 == 6)) {
        vela_view_set_snap(view, vela_snap_none, NULL);
    } else {
        vela_view_set_minimized(view, true);
    }
}

// ----------------------------------------------------------------- preview --

// How close to the edge the cursor must get. In the real session the cursor
// stops at the edge; inside KDE it can leave Vela's window before touching it,
// which is why this isn't zero.
#define EDGE_THRESHOLD 6
// Along an edge, this close to a corner it snaps to the quarter.
#define CORNER_SIZE 96
// The mouse resting on the Maximize button: how long before the layouts open.
#define LAYOUTS_DELAY_MS 450
// The shell's accent (server->accent, sent with `theme`), translucent.
#define PREVIEW_ALPHA 0.28f

struct vela_snapping *vela_snapping_create(void)
{
    struct vela_snapping *snapping = calloc(1, sizeof(*snapping));
    snapping->tween = (struct vela_tween) { -1.0, 1.0, NULL };
    snapping->next_group = 1;
    snapping->drag_layout = snapping->drag_zone = -1;
    return snapping;
}

void vela_snapping_destroy(struct vela_snapping *snapping)
{
    if (!snapping) {
        return;
    }
    if (snapping->rect) {
        vela_node_destroy(&snapping->rect->node);
    }
    if (snapping->layouts_timer) {
        wl_event_source_remove(snapping->layouts_timer);
    }
    free(snapping);
}

bool vela_snap_preview_shown(const struct vela_server *server)
{
    return server->snapping->rect != NULL;
}

bool vela_snap_tick_preview(struct vela_server *server, double now_ms)
{
    struct vela_snapping *snapping = server->snapping;
    if (!snapping->rect) {
        return false;
    }
    // Grows from the center of the area and lights up.
    double p = now_ms > 0.0 ? vela_tween_progress(&snapping->tween, now_ms) : 0.0;
    double scale = 0.9 + 0.1 * p;
    double width = snapping->target_width * scale;
    double height = snapping->target_height * scale;
    vela_rect_node_set_size(snapping->rect, width, height);
    vela_node_set_position(&snapping->rect->node, snapping->target_x + (snapping->target_width - width) / 2,
        snapping->target_y + (snapping->target_height - height) / 2);
    float alpha = PREVIEW_ALPHA * (float)p;
    const struct wlr_render_color color = {
        server->accent[0] * alpha,
        server->accent[1] * alpha,
        server->accent[2] * alpha,
        alpha,
    };
    vela_rect_node_set_color(snapping->rect, &color);
    return !vela_tween_finished(&snapping->tween, now_ms);
}

static void remove_preview(struct vela_snapping *snapping)
{
    if (snapping->rect) {
        vela_node_destroy(&snapping->rect->node);
        snapping->rect = NULL;
    }
}

// ------------------------------------------- layouts while dragging --
// Dragging a window to the top of an output opens the snap layouts there,
// like Windows 11; released on a zone, it snaps there. The compositor has the
// pointer during the drag, so it finds the zone; the shell only draws
// (SnapDragLayouts.qml) what it's sent: "snap-drag show <output> <json>" (the
// panel's top left on the output, its scale, sizes and layouts),
// "snap-drag hover <layout> <zone>" (-1 -1: none), "snap-drag hide", and
// "snap-drag drop <window> <layout>" just before the snap, so that Snap
// Assist offers the layout's other zones.

#define DRAG_KEEP 32 // once open, this much farther they stay open
// The panel at 100%: one row of 64x40 cards, 8 apart, 16 from the ends and 8
// from the top and bottom of the pill, which is 8 below the top of the output;
// zones 3 inside the card.
#define DRAG_CARD_W 64
#define DRAG_CARD_H 40
#define DRAG_GAP 8
#define DRAG_PAD_X 16
#define DRAG_PAD_Y 8
#define DRAG_INSET 3
#define DRAG_COUNT 8
#define DRAG_PANEL_W (DRAG_COUNT * DRAG_CARD_W + (DRAG_COUNT - 1) * DRAG_GAP + 2 * DRAG_PAD_X)
#define DRAG_PANEL_H (DRAG_CARD_H + 2 * DRAG_PAD_Y)
#define DRAG_PANEL_TOP 8

// In twelfths (x1 == 0: no zone).
static const struct vela_snap drag_layouts[DRAG_COUNT][4] = {
    { { 0, 0, 6, 12 }, { 6, 0, 12, 12 } },
    { { 0, 0, 8, 12 }, { 8, 0, 12, 12 } },
    { { 0, 0, 4, 12 }, { 4, 0, 12, 12 } },
    { { 0, 0, 4, 12 }, { 4, 0, 8, 12 }, { 8, 0, 12, 12 } },
    { { 0, 0, 6, 12 }, { 6, 0, 12, 6 }, { 6, 6, 12, 12 } },
    { { 0, 0, 6, 6 }, { 6, 0, 12, 6 }, { 0, 6, 6, 12 }, { 6, 6, 12, 12 } },
    { { 0, 0, 3, 12 }, { 3, 0, 9, 12 }, { 9, 0, 12, 12 } },
    { { 0, 0, 6, 6 }, { 0, 6, 6, 12 }, { 6, 0, 12, 12 } },
};

// The panel on an output: its top left (layout coordinates) and scale
// ("snap-drag-scale", but at most 90% of the output's width).
struct drag_panel {
    double x, y, scale;
};

static struct drag_panel drag_panel_on(const struct vela_output *output)
{
    struct wlr_box box = vela_output_box(output);
    double scale = fmin(vela_snap_drag_config()->scale / 100.0, 0.9 * box.width / DRAG_PANEL_W);
    return (struct drag_panel) { box.x + (box.width - DRAG_PANEL_W * scale) / 2.0, box.y + DRAG_PANEL_TOP * scale,
        scale };
}

// The zone at (x, y) from the panel's top left, at 100%: anywhere on a card
// counts, its inset and the gaps between zones too.
static bool drag_zone_at(double x, double y, int *layout, int *zone)
{
    *layout = *zone = -1;
    int l = (int)floor((x - DRAG_PAD_X) / (DRAG_CARD_W + DRAG_GAP));
    double cx = DRAG_PAD_X + l * (DRAG_CARD_W + DRAG_GAP);
    if (l < 0 || l >= DRAG_COUNT || x >= cx + DRAG_CARD_W || y < DRAG_PAD_Y || y >= DRAG_PAD_Y + DRAG_CARD_H) {
        return false;
    }
    double u = fmin(fmax((x - cx - DRAG_INSET) * 12.0 / (DRAG_CARD_W - 2 * DRAG_INSET), 0.0), 11.999);
    double v = fmin(fmax((y - DRAG_PAD_Y - DRAG_INSET) * 12.0 / (DRAG_CARD_H - 2 * DRAG_INSET), 0.0), 11.999);
    for (int z = 0; z < 4 && drag_layouts[l][z].x1; ++z) {
        struct vela_snap s = drag_layouts[l][z];
        if (u >= s.x0 && u < s.x1 && v >= s.y0 && v < s.y1) {
            *layout = l;
            *zone = z;
            return true;
        }
    }
    return false;
}

// "snap-drag show": all the shell needs to draw what drag_zone_at hits.
static void drag_send_show(struct vela_server *server, const char *verb, struct vela_output *output, struct drag_panel panel)
{
    struct wlr_box box = vela_output_box(output);
    struct vela_buffer line = { 0 };
    vela_buffer_appendf(&line,
        "snap-drag %s %s {\"x\":%.2f,\"y\":%.2f,\"scale\":%.3f,\"card\":[%d,%d],\"gap\":%d,\"pad\":[%d,%d],"
        "\"inset\":%d,\"layouts\":[",
        verb, output->wlr->name, panel.x - box.x, panel.y - box.y, panel.scale, DRAG_CARD_W, DRAG_CARD_H, DRAG_GAP,
        DRAG_PAD_X, DRAG_PAD_Y, DRAG_INSET);
    for (int l = 0; l < DRAG_COUNT; ++l) {
        vela_buffer_appendf(&line, "%s[", l ? "," : "");
        for (int z = 0; z < 4 && drag_layouts[l][z].x1; ++z) {
            struct vela_snap s = drag_layouts[l][z];
            vela_buffer_appendf(&line, "%s[%d,%d,%d,%d]", z ? "," : "", s.x0, s.y0, s.x1, s.y1);
        }
        vela_buffer_appendf(&line, "]");
    }
    vela_buffer_appendf(&line, "]}");
    vela_shell_send(server, vela_buffer_text(&line));
    vela_buffer_finish(&line);
}

static const char *view_id(const struct vela_view *view);

// Opens, follows and closes the layouts; the zone under the cursor, or none.
static struct vela_snap drag_layouts_update(struct vela_server *server, struct vela_output *output)
{
    struct vela_snapping *snapping = server->snapping;
    struct wlr_cursor *cursor = server->cursor;
    struct vela_view *grabbed = server->grabbed;
    const struct vela_snap_drag_config *config = vela_snap_drag_config();
    // Only dragging with the mouse: a keyboard Move can't drop on a zone.
    bool allowed = output && grabbed && server->cursor_mode == VELA_CURSOR_MOVE && !server->interaction->keyboard.view
        && !grabbed->fullscreen && view_id(grabbed) && config->enabled;
    struct vela_output *open = NULL;
    struct vela_output *peek = NULL;
    struct drag_panel panel = { 0 };
    if (output) {
        struct wlr_box box = vela_output_box(output);
        panel = drag_panel_on(output);
        double width = DRAG_PANEL_W * panel.scale;
        double height = DRAG_PANEL_H * panel.scale;
        double x = cursor->x;
        double y = cursor->y;
        // Near the top, beside the panel too. Once open they stay open
        // DRAG_KEEP farther, below the panel too: no flicker at the border.
        bool at_top = x >= panel.x - config->width && x < panel.x + width + config->width
            && y < box.y + config->distance;
        bool around = x >= panel.x - config->width - DRAG_KEEP && x < panel.x + width + config->width + DRAG_KEEP
            && y < fmax(box.y + config->distance, panel.y + height) + DRAG_KEEP;
        if (!around) {
            snapping->drag_dismissed = false; // Esc lasts until the pointer goes away
        }
        if (allowed && !snapping->drag_dismissed && (at_top || (around && snapping->drag_output == output))) {
            open = output;
        }
        // Not yet near the top: the panel's lower edge shows, from the grab on.
        if (!open && allowed && !snapping->drag_dismissed && config->peek) {
            peek = output;
        }
    }
    if (open != snapping->drag_output || (!open && peek != snapping->drag_peek)) {
        if (open) {
            drag_send_show(server, "show", open, panel);
        } else if (peek) {
            drag_send_show(server, "peek", peek, panel);
        } else {
            vela_shell_send(server, "snap-drag hide");
        }
        snapping->drag_output = open;
        snapping->drag_peek = peek;
        snapping->drag_layout = snapping->drag_zone = -1;
    }
    int layout = -1;
    int zone = -1;
    if (open) {
        drag_zone_at((cursor->x - panel.x) / panel.scale, (cursor->y - panel.y) / panel.scale, &layout, &zone);
        if (layout != snapping->drag_layout || zone != snapping->drag_zone) {
            char line[64];
            snprintf(line, sizeof(line), "snap-drag hover %d %d", layout, zone);
            vela_shell_send(server, line);
        }
    }
    snapping->drag_layout = layout;
    snapping->drag_zone = zone;
    return layout >= 0 ? drag_layouts[layout][zone] : vela_snap_none;
}

bool vela_snap_drag_key(struct vela_server *server, uint32_t sym, bool pressed)
{
    struct vela_snapping *snapping = server->snapping;
    if (sym != XKB_KEY_Escape) {
        return false;
    }
    if (!pressed) {
        return snapping->drag_dismissed && server->cursor_mode == VELA_CURSOR_MOVE;
    }
    if (!snapping->drag_output) {
        return false;
    }
    snapping->drag_dismissed = true;
    vela_snap_update_zone(server);
    return true;
}

void vela_snap_update_zone(struct vela_server *server)
{
    struct vela_snapping *snapping = server->snapping;
    struct wlr_cursor *cursor = server->cursor;
    struct vela_view *grabbed = server->grabbed;
    struct vela_output *output = vela_output_at(server, cursor->x, cursor->y);
    enum vela_snap_zone zone = VELA_SNAP_ZONE_NONE;
    struct vela_snap tile = vela_snap_none;
    struct vela_snap chosen = drag_layouts_update(server, output);
    if (!vela_snap_is_none(chosen)) {
        zone = VELA_SNAP_ZONE_TILE;
        tile = chosen;
    } else if (output && grabbed && !grabbed->fullscreen) {
        struct wlr_box box = vela_output_box(output);
        bool left = cursor->x <= box.x + EDGE_THRESHOLD;
        bool right = cursor->x >= box.x + box.width - 1 - EDGE_THRESHOLD;
        bool top = cursor->y <= box.y + EDGE_THRESHOLD;
        bool bottom = cursor->y >= box.y + box.height - 1 - EDGE_THRESHOLD;
        bool near_top = cursor->y <= box.y + CORNER_SIZE;
        bool near_bottom = cursor->y >= box.y + box.height - CORNER_SIZE;
        bool near_left = cursor->x <= box.x + CORNER_SIZE;
        bool near_right = cursor->x >= box.x + box.width - CORNER_SIZE;
        // Quarters in the corners, halves on the sides, the top maximizes.
        zone = VELA_SNAP_ZONE_TILE;
        if ((left && near_top) || (top && near_left)) {
            tile = vela_snap_top_left;
        } else if ((right && near_top) || (top && near_right)) {
            tile = vela_snap_top_right;
        } else if ((left && near_bottom) || (bottom && near_left)) {
            tile = vela_snap_bottom_left;
        } else if ((right && near_bottom) || (bottom && near_right)) {
            tile = vela_snap_bottom_right;
        } else if (left || right) {
            tile = left ? vela_snap_left : vela_snap_right;
        } else {
            zone = top ? VELA_SNAP_ZONE_MAXIMIZE : VELA_SNAP_ZONE_NONE;
        }
    }
    if (zone == snapping->zone && vela_snap_equal(tile, snapping->tile) && output == snapping->output) {
        return;
    }
    snapping->zone = zone;
    snapping->tile = tile;
    snapping->output = output;
    if (zone == VELA_SNAP_ZONE_NONE) {
        remove_preview(snapping);
        return;
    }
    struct vela_area target
        = zone == VELA_SNAP_ZONE_MAXIMIZE ? vela_output_usable_area(output) : vela_snap_area(output, tile);
    snapping->target_x = target.x;
    snapping->target_y = target.y;
    snapping->target_width = target.width;
    snapping->target_height = target.height;
    if (!snapping->rect) {
        const struct wlr_render_color none = { 0 };
        snapping->rect = vela_rect_node_create(grabbed->tree->node.parent, 0, 0, &none);
    }
    // Below the dragged window, like Windows.
    vela_node_place_below(&snapping->rect->node, &grabbed->tree->node);
    vela_tween_start(&snapping->tween, VELA_SNAP_PREVIEW_MS, &vela_decelerate);
    vela_snap_tick_preview(server, 0.0);
    vela_server_schedule_frames(server);
}

void vela_snap_end_zone(struct vela_server *server, bool apply)
{
    struct vela_snapping *snapping = server->snapping;
    enum vela_snap_zone zone = snapping->zone;
    struct vela_snap tile = snapping->tile;
    struct vela_output *output = snapping->output;
    struct vela_view *grabbed = server->grabbed;
    remove_preview(snapping);
    snapping->zone = VELA_SNAP_ZONE_NONE;
    snapping->tile = vela_snap_none;
    snapping->output = NULL;
    if (snapping->drag_output || snapping->drag_peek) {
        // Dropped on a zone: the shell learns the layout before Snap Assist.
        char line[256];
        bool drop = apply && grabbed && snapping->drag_layout >= 0 && zone == VELA_SNAP_ZONE_TILE;
        if (drop) {
            snprintf(line, sizeof(line), "snap-drag drop %s %d", view_id(grabbed), snapping->drag_layout);
        }
        vela_shell_send(server, drop ? line : "snap-drag hide");
        snapping->drag_output = NULL;
        snapping->drag_peek = NULL;
        snapping->drag_layout = snapping->drag_zone = -1;
    }
    snapping->drag_dismissed = false;
    if (!apply || !grabbed || zone == VELA_SNAP_ZONE_NONE) {
        return;
    }
    if (zone == VELA_SNAP_ZONE_MAXIMIZE) {
        vela_view_set_maximized(grabbed, true, true);
    } else {
        vela_view_set_snap(grabbed, tile, output);
        vela_snap_offer_assist(server, grabbed);
    }
}

// ------------------------------------------------------- shell: assist --

static const char *view_id(const struct vela_view *view)
{
    return view->ext_handle ? view->ext_handle->identifier : NULL;
}

void vela_snap_offer_assist(struct vela_server *server, struct vela_view *view)
{
    struct vela_output *output = view ? vela_view_output(view) : NULL;
    if (!output || vela_snap_is_none(view->snap) || !view_id(view)) {
        return;
    }
    // {"window":id,"output":name,"tile":[x0,y0,x1,y1],"occupied":[[...]],"candidates":[id...]}
    struct vela_buffer occupied = { 0 };
    struct vela_buffer candidates = { 0 };
    struct vela_view *other;
    wl_list_for_each (other, &server->views, link) {
        if (other == view || !other->mapped || other->minimized || !vela_view_on_current_workspace(other)
            || !view_id(other)) {
            continue;
        }
        if (!vela_snap_is_none(other->snap) && vela_view_output(other) == output) {
            vela_buffer_appendf(&occupied, "%s[%d,%d,%d,%d]", occupied.length ? "," : "", other->snap.x0,
                other->snap.y0, other->snap.x1, other->snap.y1);
        } else if (vela_snap_is_none(other->snap) && !other->fullscreen && vela_view_configurable(other)) {
            vela_buffer_appendf(&candidates, "%s\"%s\"", candidates.length ? "," : "", view_id(other));
        }
    }
    if (candidates.length) { // otherwise no window to offer
        struct vela_buffer line = { 0 };
        struct vela_snap s = view->snap;
        vela_buffer_appendf(&line,
            "snap-assist {\"window\":\"%s\",\"output\":\"%s\",\"tile\":[%d,%d,%d,%d],\"occupied\":[%s],"
            "\"candidates\":[%s]}",
            view_id(view), output->wlr->name, s.x0, s.y0, s.x1, s.y1, vela_buffer_text(&occupied),
            vela_buffer_text(&candidates));
        vela_shell_send(server, vela_buffer_text(&line));
        vela_buffer_finish(&line);
    }
    vela_buffer_finish(&occupied);
    vela_buffer_finish(&candidates);
}

void vela_snap_join_group(struct vela_server *server, struct vela_view *view, struct vela_view *origin)
{
    if (!view || !origin || origin == view || !origin->mapped || vela_snap_is_none(origin->snap)
        || vela_snap_is_none(view->snap) || vela_view_output(origin) != vela_view_output(view)) {
        return;
    }
    if (origin->snap_group == 0) {
        origin->snap_group = server->snapping->next_group++;
    }
    view->snap_group = origin->snap_group;
    vela_workspaces_announce(server);
}

void vela_snap_activate_group(struct vela_server *server, struct vela_view *view)
{
    // The others first, then the chosen one: it stays on top and focused. The
    // list changes while they're brought forward: collect them first.
    uint32_t group = view->snap_group;
    int count = wl_list_length(&server->views);
    struct vela_view **members = calloc((size_t)count + 1, sizeof(*members));
    int n = 0;
    struct vela_view *other;
    wl_list_for_each (other, &server->views, link) {
        if (group != 0 && other->snap_group == group && other != view) {
            members[n++] = other;
        }
    }
    members[n++] = view;
    for (int i = 0; i < n; ++i) {
        if (members[i]->minimized) {
            vela_view_set_minimized(members[i], false);
        }
        vela_focus_view(server, members[i]);
    }
    free(members);
}

// ------------------------------------------------------ shell: layouts --

void vela_snap_show_layouts(struct vela_server *server, struct vela_view *view, bool keyboard)
{
    struct vela_output *output = view ? vela_view_output(view) : NULL;
    if (!output || !view_id(view) || view->fullscreen || server->locked) {
        return;
    }
    // Below the Maximize button; without Vela's bar, at the top center of the
    // window.
    struct wlr_box frame = vela_view_frame_box(view);
    struct wlr_box anchor = { frame.x + frame.width / 2, frame.y, 0, 0 };
    if (view->decoration) {
        anchor = vela_decoration_maximize_box(view->decoration);
    }
    struct wlr_box screen = vela_output_box(output);
    char line[512];
    snprintf(line, sizeof(line), "snap-layouts %s %s %d %d %s", view_id(view), output->wlr->name,
        anchor.x + anchor.width / 2 - screen.x, anchor.y + anchor.height - screen.y, keyboard ? "1" : "0");
    vela_shell_send(server, line);
}

static int handle_layouts_timer(void *data)
{
    struct vela_server *server = data;
    // Only once: the panel covers the button, and hovering it again starts
    // over.
    struct vela_view *hovered = server->snapping->layouts_hover;
    server->snapping->layouts_hover = NULL;
    if (hovered && hovered->decoration && server->cursor_mode == VELA_CURSOR_PASSTHROUGH
        && server->seat->pointer_state.button_count == 0
        && vela_decoration_part_at(hovered->decoration, server->cursor->x, server->cursor->y)
            == VELA_DECORATION_MAXIMIZE) {
        vela_snap_show_layouts(server, hovered, false);
    }
    return 0;
}

void vela_snap_hover_maximize(struct vela_server *server, struct vela_view *view)
{
    struct vela_snapping *snapping = server->snapping;
    if (view == snapping->layouts_hover) {
        return;
    }
    snapping->layouts_hover = view;
    if (!snapping->layouts_timer) {
        snapping->layouts_timer = wl_event_loop_add_timer(server->loop, handle_layouts_timer, server);
    }
    wl_event_source_timer_update(snapping->layouts_timer, view ? LAYOUTS_DELAY_MS : 0);
}

void vela_snap_forget(struct vela_server *server, struct vela_view *view)
{
    if (server->snapping->layouts_hover == view) {
        vela_snap_hover_maximize(server, NULL);
    }
}
