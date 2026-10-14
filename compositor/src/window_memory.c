// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "window_memory.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

enum { MAX_ENTRIES = 512 };

static bool state_path(char *directory, size_t size, char *path, size_t path_size)
{
    const char *state = getenv("XDG_STATE_HOME");
    const char *home = getenv("HOME");
    int n;
    if (state && *state == '/') {
        n = snprintf(directory, size, "%s/vela", state);
    } else if (home && *home == '/') {
        n = snprintf(directory, size, "%s/.local/state/vela", home);
    } else {
        return false;
    }
    if (n <= 0 || (size_t)n >= size) {
        return false;
    }
    n = snprintf(path, path_size, "%s/window-positions.json", directory);
    return n > 0 && (size_t)n < path_size;
}

static bool field_int(const char *line, const char *key, int *out)
{
    char needle[32];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    const char *at = strstr(line, needle);
    if (!at) {
        return false;
    }
    char *end;
    long value = strtol(at + strlen(needle), &end, 10);
    if (end == at + strlen(needle) || value < -100000 || value > 100000) {
        return false;
    }
    *out = (int)value;
    return true;
}

static bool field_string(const char *line, const char *key, char *out, size_t size)
{
    char needle[32];
    snprintf(needle, sizeof(needle), "\"%s\":\"", key);
    const char *at = strstr(line, needle);
    if (!at) {
        return false;
    }
    at += strlen(needle);
    const char *end = strchr(at, '"');
    if (!end || (size_t)(end - at) >= size) {
        return false;
    }
    memcpy(out, at, (size_t)(end - at));
    out[end - at] = '\0';
    return true;
}

// A safe app id (or output name) is written as it is: no escaping needed.
static bool plain(const char *text)
{
    for (; *text; ++text) {
        if (*text == '"' || *text == '\\' || (unsigned char)*text < 0x20) {
            return false;
        }
    }
    return true;
}

static void add(struct vela_window_memory *memory, const char *app, const struct vela_window_memory_entry *entry)
{
    for (int i = 0; i < memory->count; ++i) {
        if (strcasecmp(memory->items[i].app, app) == 0) {
            char *keep = memory->items[i].app;
            memory->items[i] = *entry;
            memory->items[i].app = keep;
            return;
        }
    }
    if (memory->count >= MAX_ENTRIES) {
        return;
    }
    struct vela_window_memory_entry *items
        = realloc(memory->items, (size_t)(memory->count + 1) * sizeof(*items));
    char *name = strdup(app);
    if (!items || !name) {
        free(name);
        if (items) {
            memory->items = items;
        }
        return;
    }
    memory->items = items;
    items[memory->count] = *entry;
    items[memory->count].app = name;
    memory->count++;
}

static void load(struct vela_window_memory *memory)
{
    memory->loaded = true;
    char directory[PATH_MAX], path[PATH_MAX];
    if (!state_path(directory, sizeof(directory), path, sizeof(path))) {
        return;
    }
    FILE *in = fopen(path, "r");
    if (!in) {
        return;
    }
    char line[1024];
    while (fgets(line, sizeof(line), in)) { // one entry per line; anything else is ignored
        struct vela_window_memory_entry entry = { 0 };
        char app[256];
        int maximized = 0;
        if (!field_string(line, "app", app, sizeof(app)) || !*app || !field_string(line, "output", entry.output, sizeof(entry.output))
            || !field_int(line, "x", &entry.x) || !field_int(line, "y", &entry.y)
            || !field_int(line, "w", &entry.width) || !field_int(line, "h", &entry.height)
            || entry.width < 100 || entry.height < 100 || !field_int(line, "desktop", &entry.desktop)
            || entry.desktop < 0) {
            continue;
        }
        field_int(line, "maximized", &maximized);
        entry.maximized = maximized != 0;
        add(memory, app, &entry);
    }
    fclose(in);
}

static void save(const struct vela_window_memory *memory)
{
    char directory[PATH_MAX], path[PATH_MAX], temporary[PATH_MAX + 8];
    if (!state_path(directory, sizeof(directory), path, sizeof(path))
        || snprintf(temporary, sizeof(temporary), "%s.XXXXXX", path) >= (int)sizeof(temporary)) {
        return;
    }
    char parent[PATH_MAX]; // mkdir -p of ~/.local/state/vela
    snprintf(parent, sizeof(parent), "%s", directory);
    for (char *p = parent + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(parent, 0755);
            *p = '/';
        }
    }
    mkdir(parent, 0755);
    int fd = mkstemp(temporary);
    if (fd < 0) {
        return;
    }
    FILE *out = fdopen(fd, "w");
    if (!out) {
        close(fd);
        unlink(temporary);
        return;
    }
    bool ok = fputs("{\"version\":1,\"windows\":[\n", out) >= 0;
    for (int i = 0; i < memory->count; ++i) {
        const struct vela_window_memory_entry *e = &memory->items[i];
        ok = fprintf(out, "%s{\"app\":\"%s\",\"output\":\"%s\",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"maximized\":%d,\"desktop\":%d}\n",
                 i ? "," : "", e->app, e->output, e->x, e->y, e->width, e->height, e->maximized ? 1 : 0, e->desktop)
                >= 0
            && ok;
    }
    ok = fputs("]}\n", out) >= 0 && ok;
    ok = fclose(out) == 0 && ok;
    if (!ok || rename(temporary, path) != 0) {
        unlink(temporary);
    }
}

const struct vela_window_memory_entry *vela_window_memory_find(struct vela_window_memory *memory, const char *app_id)
{
    if (!app_id || !*app_id) {
        return NULL;
    }
    if (!memory->loaded) {
        load(memory);
    }
    for (int i = 0; i < memory->count; ++i) {
        if (strcasecmp(memory->items[i].app, app_id) == 0) {
            return &memory->items[i];
        }
    }
    return NULL;
}

void vela_window_memory_set(struct vela_window_memory *memory, const char *app_id,
    const struct vela_window_memory_entry *entry)
{
    if (!app_id || !*app_id || strlen(app_id) >= 256 || !plain(app_id) || !plain(entry->output) || entry->width < 100
        || entry->height < 100) {
        return;
    }
    if (!memory->loaded) {
        load(memory);
    }
    add(memory, app_id, entry);
    save(memory);
}

void vela_window_memory_free(struct vela_window_memory *memory)
{
    for (int i = 0; i < memory->count; ++i) {
        free(memory->items[i].app);
    }
    free(memory->items);
    memset(memory, 0, sizeof(*memory));
}
