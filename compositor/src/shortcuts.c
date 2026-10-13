// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "shortcuts.h"

#include "config.h"
#include "lock.h"
#include "process.h"
#include "server.h"
#include "shell.h"
#include "view.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#define MAX_SHORTCUTS 64
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
    { "start", KIND_SHELL, "toggle-start" },
    { "winx", KIND_SHELL, "winx" },
    { "run", KIND_SHELL, "run" },
    { "show-desktop", KIND_SHELL, "show-desktop" },
    { "quick-settings", KIND_SHELL, "quick-settings" },
    { "notification-center", KIND_SHELL, "notification-center" },
    { "settings", KIND_SHELL, "settings" },
    { "files", KIND_SHELL, "files" },
    { "task-view", KIND_SHELL, "task-view" },
    { "snip", KIND_SHELL, "snip" },
    { "snip-text", KIND_SHELL, "snip-text" },
    { "clipboard", KIND_SHELL, "clipboard" },
    { "sound-output", KIND_SHELL, "sound-output" },
};
#define ACTION_COUNT (sizeof(actions) / sizeof(actions[0]))

struct shortcut {
    uint32_t modifiers;
    uint32_t sym; // lowercase
    int action; // index in actions[], or -1: a command
    char *command;
};

static struct shortcut shortcuts[MAX_SHORTCUTS];
static int shortcut_count;
static bool overridden[ACTION_COUNT]; // the user has a "shortcut-<action>" line

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
    slot->modifiers = modifiers;
    slot->sym = sym;
    slot->action = action;
    slot->command = command ? strdup(command) : NULL;
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
    vela_config_finish(&config);
    wlr_log(WLR_INFO, "Shortcuts: %d from vela.conf", shortcut_count);
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
            vela_view_close(active);
        }
    } else if (strcmp(id, "terminal") == 0) {
        const char *terminal = getenv("VELA_TERMINAL");
        vela_spawn(terminal ? terminal : "konsole || foot || kitty || alacritty || xterm");
    } else if (strcmp(id, "lock") == 0) {
        vela_lock_screen(server->lock);
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
        if (shortcut->action < 0) {
            vela_spawn(shortcut->command);
        } else if (actions[shortcut->action].kind == KIND_NATIVE) {
            run_native(server, actions[shortcut->action].id);
        } else {
            vela_shell_send(server, actions[shortcut->action].shell_line);
        }
        return true;
    }
    return false;
}
