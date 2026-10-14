// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "process.h"

#include "util.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/pidfd.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/util/log.h>

void vela_exec_shell(const char *command)
{
    setsid();
    // wlroots blocks some signals to handle them in its loop: the processes we
    // launch must start with a clean mask.
    sigset_t set;
    sigemptyset(&set);
    sigprocmask(SIG_SETMASK, &set, NULL);
    execl("/bin/sh", "/bin/sh", "-c", command, (char *)NULL);
    _exit(127);
}

void vela_spawn(const char *command)
{
    pid_t child = fork();
    if (child == 0) {
        if (fork() == 0) {
            vela_exec_shell(command);
        }
        _exit(0);
    }
    if (child > 0) {
        waitpid(child, NULL, 0);
    }
}

// ---------------------------------------------------------- supervision --

void vela_child_init(struct vela_child *child, const char *name, int death_signal, int final_exit_code)
{
    memset(child, 0, sizeof(*child));
    child->name = name;
    child->death_signal = death_signal;
    child->final_exit_code = final_exit_code;
    child->pid = -1;
    child->pidfd = -1;
}

static void stop_watching(struct vela_child *child)
{
    if (child->source) {
        wl_event_source_remove(child->source);
        child->source = NULL;
    }
    if (child->pidfd >= 0) {
        close(child->pidfd);
    }
    child->pidfd = -1;
    child->pid = -1;
}

static int handle_exit(int fd, uint32_t mask, void *data)
{
    struct vela_child *child = data;
    int status = 0;
    waitpid(child->pid, &status, 0);
    double uptime = (vela_now_ns() - child->started_ns) / 1e9;
    stop_watching(child);
    char *command = child->command;
    child->command = NULL;

    // Asked for by vela_child_restart(): not a crash, whatever the status.
    if (child->restart_requested) {
        child->restart_requested = false;
        child->quick_crashes = 0;
        wlr_log(WLR_INFO, "%s \"%s\" restarting on request", child->name, command);
        vela_child_start(child, child->loop, command);
        free(command);
        return 0;
    }

    // A clean exit (such as "already running") was intended.
    if (WIFEXITED(status) && (WEXITSTATUS(status) == 0 || WEXITSTATUS(status) == child->final_exit_code)) {
        wlr_log(WLR_INFO, "%s \"%s\" exited (code %d)", child->name, command, WEXITSTATUS(status));
        free(command);
        return 0;
    }
    char reason[64];
    if (WIFSIGNALED(status)) {
        snprintf(reason, sizeof(reason), "signal %s", strsignal(WTERMSIG(status)));
    } else {
        snprintf(reason, sizeof(reason), "code %d", WEXITSTATUS(status));
    }
    // When it keeps exiting right after starting, restarting it is pointless.
    child->quick_crashes = uptime < 5.0 ? child->quick_crashes + 1 : 0;
    if (child->quick_crashes >= 3) {
        wlr_log(WLR_ERROR, "%s \"%s\" keeps exiting (%s): not restarting it", child->name, command, reason);
    } else {
        wlr_log(WLR_ERROR, "%s \"%s\" exited (%s): restarting it", child->name, command, reason);
        vela_child_start(child, child->loop, command);
    }
    free(command);
    return 0;
}

static void handle_client_destroy(struct wl_listener *listener, void *data)
{
    struct vela_child *child = wl_container_of(listener, child, client_destroy);
    wl_list_remove(&child->client_destroy.link);
    child->client = NULL;
}

static void set_client(struct vela_child *child, struct wl_client *client)
{
    if (child->client) {
        wl_list_remove(&child->client_destroy.link); // the old one stays connected, untrusted
    }
    child->client = client;
    if (client) {
        child->client_destroy.notify = handle_client_destroy;
        wl_client_add_destroy_listener(client, &child->client_destroy);
    }
}

// The environment with WAYLAND_SOCKET=<fd> (`entry`) instead of any inherited
// one, built before fork: the child only swaps a pointer.
static char **environment_with(char *entry)
{
    extern char **environ;
    size_t count = 0;
    while (environ[count]) {
        ++count;
    }
    char **env = calloc(count + 2, sizeof(*env));
    if (!env) {
        return NULL;
    }
    size_t kept = 0;
    for (size_t i = 0; i < count; ++i) {
        if (strncmp(environ[i], "WAYLAND_SOCKET=", 15) != 0) {
            env[kept++] = environ[i];
        }
    }
    env[kept] = entry;
    return env;
}

void vela_child_start(struct vela_child *child, struct wl_event_loop *loop, const char *command)
{
    extern char **environ;
    // The shell's own connection (§15.3): fds[0] ours, fds[1] the child's.
    int fds[2] = { -1, -1 };
    char entry[32] = "";
    char **env = NULL;
    if (child->display) {
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds) == 0) {
            snprintf(entry, sizeof(entry), "WAYLAND_SOCKET=%d", fds[1]);
            env = environment_with(entry);
        } else {
            wlr_log_errno(WLR_ERROR, "socketpair: %s connects as an untrusted client", child->name);
        }
        if (!env && fds[0] >= 0) {
            close(fds[0]);
            close(fds[1]);
            fds[0] = fds[1] = -1;
        }
    }

    // A single fork, so the process stays our child and a pidfd in the Wayland
    // loop tells us when it ends.
    char *copy = strdup(command);
    free(child->command);
    child->command = copy;
    child->loop = loop;
    pid_t pid = fork();
    if (pid < 0) {
        wlr_log_errno(WLR_ERROR, "Can't start \"%s\"", command);
        if (fds[0] >= 0) {
            close(fds[0]);
            close(fds[1]);
        }
        free(env);
        return;
    }
    if (pid == 0) {
        // The child dies with the compositor: if the compositor crashes, the
        // supervisor starts another one with new children, and the old ones
        // must not reconnect (QT_WAYLAND_RECONNECT) and make duplicates.
        prctl(PR_SET_PDEATHSIG, child->death_signal);
        if (env) {
            fcntl(fds[1], F_SETFD, 0); // inherited by exec; libwayland sets CLOEXEC again
            environ = env;
        }
        vela_exec_shell(command);
    }
    free(env);
    if (fds[0] >= 0) {
        close(fds[1]);
        struct wl_client *client = wl_client_create(child->display, fds[0]);
        if (!client) {
            close(fds[0]);
        }
        set_client(child, client);
    }
    int pidfd = pidfd_open(pid, 0);
    if (pidfd < 0) {
        wlr_log_errno(WLR_ERROR, "pidfd_open: \"%s\" won't be restarted", command);
        return;
    }
    child->pid = pid;
    child->pidfd = pidfd;
    child->started_ns = vela_now_ns();
    child->source = wl_event_loop_add_fd(loop, pidfd, WL_EVENT_READABLE, handle_exit, child);
}

void vela_child_restart(struct vela_child *child)
{
    if (child->pid <= 0) {
        return;
    }
    child->restart_requested = true;
    kill(child->pid, SIGTERM);
}

void vela_child_stop(struct vela_child *child)
{
    child->restart_requested = false;
    set_client(child, NULL);
    stop_watching(child);
    free(child->command);
    child->command = NULL;
}
