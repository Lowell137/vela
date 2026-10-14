// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "window_rules.h"

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static bool yes(const char *value)
{
    return strcmp(value, "yes") == 0 || strcmp(value, "1") == 0 || strcmp(value, "true") == 0;
}

static void free_rule(struct vela_window_rule *rule)
{
    free(rule->app);
    free(rule->title);
    free(rule->output);
}

static void apply_pair(struct vela_window_rule *rule, const char *key, const char *value)
{
    int a, b;
    char tail;
    if (strcmp(key, "title~") == 0) {
        free(rule->title);
        rule->title = *value ? strdup(value) : NULL;
    } else if (strcmp(key, "output") == 0) {
        free(rule->output);
        rule->output = *value ? strdup(value) : NULL;
    } else if (strcmp(key, "desktop") == 0 && sscanf(value, "%d%c", &a, &tail) == 1 && a > 0) {
        rule->desktop = a;
    } else if (strcmp(key, "position") == 0 && sscanf(value, "%d,%d%c", &a, &b, &tail) == 2) {
        rule->has_position = true;
        rule->x = a;
        rule->y = b;
    } else if (strcmp(key, "size") == 0 && sscanf(value, "%dx%d%c", &a, &b, &tail) == 2 && a > 0 && b > 0) {
        rule->has_size = true;
        rule->width = a;
        rule->height = b;
    } else if (strcmp(key, "center") == 0) {
        rule->center = yes(value);
    } else if (strcmp(key, "maximized") == 0) {
        rule->maximized = yes(value);
    } else if (strcmp(key, "fullscreen") == 0) {
        rule->fullscreen = yes(value);
    } else if (strcmp(key, "always-on-top") == 0) {
        rule->always_on_top = yes(value);
    } else if (strcmp(key, "capture") == 0) {
        rule->capture = yes(value);
    } else if (strcmp(key, "autoscroll") == 0) {
        rule->autoscroll = yes(value);
    } else {
        fprintf(stderr, "vela: window-rule: ignoring \"%s=%s\"\n", key, value);
    }
}

bool vela_window_rules_add(struct vela_window_rules *rules, const char *value)
{
    const char *bar = strchr(value, '|');
    size_t app_length = bar ? (size_t)(bar - value) : strlen(value);
    while (app_length > 0 && value[app_length - 1] == ' ') {
        --app_length;
    }
    while (app_length > 0 && *value == ' ') {
        ++value;
        --app_length;
    }
    if (app_length == 0) {
        fprintf(stderr, "vela: window-rule: no app id in \"%s\"\n", value);
        return false;
    }
    struct vela_window_rule rule
        = { .center = -1, .maximized = -1, .fullscreen = -1, .always_on_top = -1, .capture = -1, .autoscroll = -1 };
    rule.app = strndup(value, app_length);

    // key=value pairs split on commas; a piece without '=' continues the
    // previous value (position=X,Y and titles with commas).
    char *list = strdup(bar ? bar + 1 : "");
    char *key = NULL;
    char *current_value = NULL;
    char *save = NULL;
    for (char *piece = strtok_r(list, ",", &save);; piece = strtok_r(NULL, ",", &save)) {
        char *eq = piece ? strchr(piece, '=') : NULL;
        if (key && (!piece || eq)) {
            apply_pair(&rule, key, current_value);
            free(key);
            free(current_value);
            key = current_value = NULL;
        }
        if (!piece) {
            break;
        }
        if (eq) {
            *eq = '\0';
            char *k = piece;
            while (*k == ' ') {
                ++k;
            }
            char *end = k + strlen(k);
            while (end > k && end[-1] == ' ') {
                *--end = '\0';
            }
            key = strdup(k);
            current_value = strdup(eq + 1);
        } else if (key) {
            char *joined = NULL;
            if (asprintf(&joined, "%s,%s", current_value, piece) >= 0) {
                free(current_value);
                current_value = joined;
            }
        } else if (*piece) {
            fprintf(stderr, "vela: window-rule: ignoring \"%s\"\n", piece);
        }
    }
    free(list);

    struct vela_window_rule *items = realloc(rules->items, (size_t)(rules->count + 1) * sizeof(*items));
    if (!items) {
        free_rule(&rule);
        return false;
    }
    rules->items = items;
    rules->items[rules->count++] = rule;
    return true;
}

static void add_line(const char *value, void *data)
{
    vela_window_rules_add(data, value);
}

void vela_window_rules_load(struct vela_window_rules *rules)
{
    vela_window_rules_free(rules);
    vela_config_each("window-rule", add_line, rules);
}

void vela_window_rules_free(struct vela_window_rules *rules)
{
    for (int i = 0; i < rules->count; ++i) {
        free_rule(&rules->items[i]);
    }
    free(rules->items);
    rules->items = NULL;
    rules->count = 0;
}

const struct vela_window_rule *vela_window_rules_find(const struct vela_window_rules *rules, const char *app_id,
    const char *title)
{
    for (int i = 0; i < rules->count; ++i) {
        const struct vela_window_rule *rule = &rules->items[i];
        if (!app_id || strcasecmp(rule->app, app_id) != 0) {
            continue;
        }
        if (rule->title && !(title && strcasestr(title, rule->title))) {
            continue;
        }
        return rule;
    }
    return NULL;
}
