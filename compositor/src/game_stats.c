// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "game_stats.h"

#include "util.h"

bool vela_game_perf_line(char *out, size_t size, double fps, int missed)
{
    if (!(fps > 0.0)) { // also NaN
        fps = 0.0;
    }
    return vela_format(out, size, "perf %.1f %.1f %d", fps, fps > 0.0 ? 1000.0 / fps : 0.0, missed > 0 ? missed : 0);
}

bool vela_game_app_line(char *out, size_t size, const char *app_id, pid_t pid)
{
    char id[128];
    size_t length = 0;
    for (const char *c = app_id; c && *c && length < sizeof(id) - 1; ++c) {
        unsigned char byte = (unsigned char)*c;
        id[length++] = byte <= ' ' || byte == 0x7f ? '_' : (char)byte;
    }
    if (length == 0) {
        id[length++] = '-';
    }
    id[length] = '\0';
    return vela_format(out, size, "perfapp %s %d", id, pid > 0 ? (int)pid : 0);
}
