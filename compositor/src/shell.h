// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VELA_SHELL_H
#define VELA_SHELL_H

// The Qt shell: the startup command (the shell) that the compositor launches
// and relaunches when it fails, and the messages it sends it.

#include <stdbool.h>

struct vela_server;

// Launches `command` and relaunches it when it fails (not when it exits
// cleanly, nor when it keeps failing right after starting). The shell lives
// in vela_server.shell (a vela_child, process.h).
void vela_shell_start(struct vela_server *server, const char *command);
// The shell ends (SIGTERM) and is launched again; the session and the
// compositor stay up. Not counted as a crash.
void vela_shell_restart(struct vela_server *server);
// We are shutting down: the shell going away must not be relaunched.
void vela_shell_stop(struct vela_server *server);

// One line of text on the shell's Unix socket
// ($XDG_RUNTIME_DIR/vela-shell-<WAYLAND_DISPLAY>.sock), such as "toggle-start"
// or "accessibility {...}". Never blocks: without a shell the message is
// simply lost.
void vela_shell_send(struct vela_server *server, const char *line);
// The same line followed by where the pointer is, in layout coordinates
// ("clipboard 812 430"): for the panels that open next to it.
void vela_shell_send_at_pointer(struct vela_server *server, const char *line);
// "perfapp <app_id> <pid>" for the focused window (docs/shortcuts.md, Game
// Bar), sent only when it differs from the last one, or always with `force`.
void vela_shell_send_perfapp(struct vela_server *server, bool force);
// The Game Bar shortcut: the focused app, then "gamebar".
void vela_shell_gamebar(struct vela_server *server);

#endif
