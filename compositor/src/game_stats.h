// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VELA_GAME_STATS_H
#define VELA_GAME_STATS_H

// The lines the Game Bar of the shell reads on the shell socket (docs/shortcuts.md):
// "perf <fps> <frame_ms> <missed>" and "perfapp <app_id> <pid>". Pure
// formatting, so a line never has a newline or a space inside a field.

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

// fps one decimal, frame_ms the average time between shown frames (0 without
// frames), missed vblanks since the last line.
bool vela_game_perf_line(char *out, size_t size, double fps, int missed);

// The id with control characters and spaces replaced by '_', "-" if empty
// or NULL; pid 0 if unknown.
bool vela_game_app_line(char *out, size_t size, const char *app_id, pid_t pid);

#endif
