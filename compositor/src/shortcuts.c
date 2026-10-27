// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "shortcuts.h"

#include "config.h"
#include "lock.h"
#include "motion.h"
#include "process.h"
#include "server.h"
#include "shell.h"
#include "util.h"
#include "view.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#define MAX_SHORTCUTS 128
#define ACTION_APP (-2) // a shortcut an app asked for through the portal
#define MODIFIER_MASK (WLR_MODIFIER_ALT | WLR_MODIFIER_CTRL | WLR_MODIFIER_SHIFT | WLR_MODIFIER_LOGO)

enum kind { KIND_NATIVE, KIND_SHELL };

static const struct action {
    const char *id;
    enum kind kind;
    const char *shell_line; // what the shell is told (KIND_SHELL)
} actions[] = {
    { "close", KIND_NATIVE, NULL },
    { "terminal", KIND_NATIVE, NULL },
    { "lock", KIND_NATIVE, NULL },
    { "always-on-top", KIND_NATIVE, NULL },
    { "start", KIND_SHELL, "toggle-start" },
    { "winx", KIND_SHELL, "winx" },
    { "run", KIND_SHELL, "run" },
    { "spotlight", KIND_SHELL, "spotlight" },
    { "show-desktop", KIND_SHELL, "show-desktop" },
    { "quick-settings", KIND_SHELL, "quick-settings" },
    { "notification-center", KIND_SHELL, "notification-center" },
    { "settings", KIND_SHELL, "settings" },
    { "files", KIND_SHELL, "files" },
    { "task-view", KIND_SHELL, "task-view" },
    { "snip", KIND_SHELL, "snip" },
    { "snip-text", KIND_SHELL, "snip-text" },
    { "snip-lens", KIND_SHELL, "snip-lens" },
    { "clipboard", KIND_SHELL, "clipboard" },
    { "emoji", KIND_SHELL, "emoji" },
    { "sound-output", KIND_SHELL, "sound-output" },
};
#define ACTION_COUNT (sizeof(actions) / sizeof(actions[0]))

struct shortcut {
    uint32_t modifiers;
    uint32_t sym; // lowercase
    int action; // index in actions[], -1: a command, ACTION_APP: an app's
    char *command;
    int number; // ACTION_APP: the shell's number for it
};

static struct shortcut shortcuts[MAX_SHORTCUTS];
static int shortcut_count;
static uint32_t active_app_sym; // the key of the app shortcut being held, 0: none
static int active_app_number;
static bool overridden[ACTION_COUNT]; // the user has a "shortcut-<action>" line
static bool hot_corner = true; // "hot-corner=no" in vela.conf turns it off
static bool hot_corner_glow = true; // "hot-corner-glow=no": no glow while near
static bool hot_corner_animation = true; // "hot-corner-animation=no": no ripples
static struct vela_snap_drag_config snap_drag = { true, 64, 160, 100, true }; // "snap-drag-*" in vela.conf

bool vela_shortcut_parse(const char *text, uint32_t *modifiers, uint32_t *sym)
{
    if (!text) {
        return false;
    }
    char copy[128];
    if (snprintf(copy, sizeof(copy), "%s", text) >= (int)sizeof(copy)) {
        return false;
    }
    uint32_t mods = 0;
    uint32_t key = 0;
    char *save = NULL;
    for (char *part = strtok_r(copy, "+", &save); part; part = strtok_r(NULL, "+", &save)) {
        while (*part == ' ') {
            ++part;
        }
        size_t length = strlen(part);
        while (length > 0 && part[length - 1] == ' ') {
            part[--length] = '\0';
        }
        if (length == 0 || key != 0) {
            return false; // empty part, or something after the key
        }
        if (strcasecmp(part, "super") == 0 || strcasecmp(part, "win") == 0 || strcasecmp(part, "meta") == 0
            || strcasecmp(part, "logo") == 0) {
            mods |= WLR_MODIFIER_LOGO;
        } else if (strcasecmp(part, "alt") == 0) {
            mods |= WLR_MODIFIER_ALT;
        } else if (strcasecmp(part, "ctrl") == 0 || strcasecmp(part, "control") == 0) {
            mods |= WLR_MODIFIER_CTRL;
        } else if (strcasecmp(part, "shift") == 0) {
            mods |= WLR_MODIFIER_SHIFT;
        } else {
            xkb_keysym_t found = xkb_keysym_from_name(part, XKB_KEYSYM_CASE_INSENSITIVE);
            if (found == XKB_KEY_NoSymbol) {
                return false;
            }
            key = xkb_keysym_to_lower(found);
        }
    }
    if (key == 0) {
        return false;
    }
    *modifiers = mods;
    *sym = key;
    return true;
}

static int find_action(const char *id)
{
    for (size_t i = 0; i < ACTION_COUNT; ++i) {
        if (strcmp(actions[i].id, id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static void clear(void)
{
    for (int i = 0; i < shortcut_count; ++i) {
        free(shortcuts[i].command);
    }
    shortcut_count = 0;
    memset(overridden, 0, sizeof(overridden));
}

static void add(uint32_t modifiers, uint32_t sym, int action, const char *command)
{
    if (shortcut_count >= MAX_SHORTCUTS) {
        return;
    }
    struct shortcut *slot = &shortcuts[shortcut_count++];
    slot->number = 0;
    slot->modifiers = modifiers;
    slot->sym = sym;
    slot->action = action;
    slot->command = command ? strdup(command) : NULL;
}

// ~/.config/vela/app-shortcuts.conf, "number|Super+Q|app id|shortcut id|text"
// (shell/src/appshortcuts.h): the keys apps asked for in the portal.
static void load_app_shortcuts(void)
{
    char path[512];
    if (!vela_config_path("app-shortcuts.conf", path, sizeof(path))) {
        return;
    }
    FILE *file = fopen(path, "r");
    if (!file) {
        return;
    }
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        char *bar = strchr(line, '|');
        if (!bar) {
            continue;
        }
        *bar = '\0';
        int number = atoi(line);
        char *combo = bar + 1;
        bar = strchr(combo, '|');
        if (number <= 0 || !bar || bar == combo) {
            continue; // no keys yet
        }
        *bar = '\0';
        uint32_t mods, sym;
        if (vela_shortcut_parse(combo, &mods, &sym)) {
            add(mods, sym, ACTION_APP, NULL);
            if (shortcut_count > 0 && shortcuts[shortcut_count - 1].action == ACTION_APP) {
                shortcuts[shortcut_count - 1].number = number;
            }
        }
    }
    fclose(file);
}

void vela_shortcuts_load(void)
{
    clear();
    struct vela_config config;
    vela_config_read(&config);
    for (int i = 0; i < config.count; ++i) {
        const char *key = config.entries[i].key;
        const char *value = config.entries[i].value;
        if (strncmp(key, "shortcut-", 9) != 0) {
            continue;
        }
        const char *name = key + 9;
        if (strncmp(name, "custom-", 7) == 0) {
            // "Super+T|command"
            const char *bar = strchr(value, '|');
            if (!bar || bar[1] == '\0') {
                continue;
            }
            char combo[128];
            size_t length = (size_t)(bar - value);
            if (length >= sizeof(combo)) {
                continue;
            }
            memcpy(combo, value, length);
            combo[length] = '\0';
            uint32_t mods, sym;
            if (vela_shortcut_parse(combo, &mods, &sym)) {
                add(mods, sym, -1, bar + 1);
            }
            continue;
        }
        int action = find_action(name);
        if (action < 0) {
            continue;
        }
        overridden[action] = true; // also when "none" or invalid: the built-in key is off
        uint32_t mods, sym;
        if (vela_shortcut_parse(value, &mods, &sym)) {
            add(mods, sym, action, NULL);
        }
    }
    hot_corner = vela_config_flag(&config, "hot-corner", true);
    hot_corner_glow = vela_config_flag(&config, "hot-corner-glow", true);
    hot_corner_animation = vela_config_flag(&config, "hot-corner-animation", true);
    snap_drag.enabled = vela_config_flag(&config, "snap-drag-layouts", true);
    snap_drag.distance = vela_clamp(atoi(vela_config_get(&config, "snap-drag-trigger-distance", "64")), 4, 200);
    snap_drag.width = vela_clamp(atoi(vela_config_get(&config, "snap-drag-trigger-width", "160")), 0, 960);
    snap_drag.scale = vela_clamp(atoi(vela_config_get(&config, "snap-drag-scale", "100")), 60, 200);
    snap_drag.peek = vela_config_flag(&config, "snap-drag-peek", true);
    // animations=no: no animation at all; animation-speed= in percent (50-200).
    const int animation_speed = vela_clamp(atoi(vela_config_get(&config, "animation-speed", "100")), 50, 200);
    vela_motion_set_scale(vela_config_flag(&config, "animations", true) ? 100.0 / animation_speed : 0.0);
    vela_config_finish(&config);
    load_app_shortcuts();
    wlr_log(WLR_INFO, "Shortcuts: %d from vela.conf", shortcut_count);
}

bool vela_hot_corner_enabled(void)
{
    return hot_corner;
}

bool vela_hot_corner_glow_enabled(void)
{
    return hot_corner_glow;
}

bool vela_hot_corner_animation_enabled(void)
{
    return hot_corner_animation;
}

const struct vela_snap_drag_config *vela_snap_drag_config(void)
{
    return &snap_drag;
}

bool vela_shortcuts_default_enabled(const char *action)
{
    int index = find_action(action);
    return index < 0 || !overridden[index];
}

static void run_native(struct vela_server *server, const char *id)
{
    if (strcmp(id, "close") == 0) {
        struct vela_view *active = vela_views_focused(server);
        if (active) {
            vela_view_close(active, "close shortcut");
        }
    } else if (strcmp(id, "terminal") == 0) {
        // The Settings choice (Default apps) first, then the environment,
        // then the first installed of the usual terminals.
        const char *choice = NULL;
        struct vela_config conf;
        vela_config_read(&conf);
        const char *key = vela_config_get(&conf, "terminal", "");
        char chosen[256] = "";
        if (*key) {
            snprintf(chosen, sizeof(chosen), "%s", key);
            choice = chosen;
        }
        vela_config_finish(&conf);
        const char *terminal = choice && *choice ? choice : getenv("VELA_TERMINAL");
        vela_spawn(terminal && *terminal ? terminal : "konsole || foot || kitty || alacritty || xterm");
    } else if (strcmp(id, "lock") == 0) {
        vela_lock_screen(server->lock);
    } else if (strcmp(id, "always-on-top") == 0) {
        struct vela_view *active = vela_views_focused(server);
        if (active) {
            vela_view_set_always_on_top(active, !active->always_on_top);
        }
    }
}

bool vela_shortcuts_dispatch(struct vela_server *server, uint32_t modifiers, uint32_t sym)
{
    uint32_t mods = modifiers & MODIFIER_MASK;
    uint32_t lower = xkb_keysym_to_lower(sym);
    if (mods != 0 && shortcut_count > 0) {
        wlr_log(WLR_DEBUG, "Shortcut check: mods 0x%x key 0x%x", mods, lower);
    }
    for (int i = 0; i < shortcut_count; ++i) {
        const struct shortcut *shortcut = &shortcuts[i];
        if (shortcut->modifiers != mods || shortcut->sym != lower) {
            continue;
        }
        if (shortcut->action == ACTION_APP) {
            // Held down until the key goes up (vela_shortcuts_key_released).
            char line[64];
            snprintf(line, sizeof(line), "app-shortcut press %d", shortcut->number);
            vela_shell_send(server, line);
            active_app_sym = lower;
            active_app_number = shortcut->number;
        } else if (shortcut->action < 0) {
            vela_spawn(shortcut->command);
        } else if (actions[shortcut->action].kind == KIND_NATIVE) {
            run_native(server, actions[shortcut->action].id);
        } else {
            const char *line = actions[shortcut->action].shell_line;
            // The clipboard and emoji panel opens next to the pointer.
            if (strcmp(line, "clipboard") == 0 || strcmp(line, "emoji") == 0) {
                vela_shell_send_at_pointer(server, line);
            } else {
                vela_shell_send(server, line);
            }
        }
        return true;
    }
    return false;
}

void vela_shortcuts_key_released(struct vela_server *server, uint32_t sym)
{
    if (active_app_sym != 0 && xkb_keysym_to_lower(sym) == active_app_sym) {
        char line[64];
        snprintf(line, sizeof(line), "app-shortcut release %d", active_app_number);
        vela_shell_send(server, line);
        active_app_sym = 0;
    }
}
