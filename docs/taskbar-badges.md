# Taskbar badges from notifications

Builds on the taskbar badges (Unity LauncherEntry): a count, progress and an
urgent state on the buttons of apps that send them. This document describes
what a button shows for apps that send nothing of their own.

> **Status:** implemented (the notification count and the optional Steam
> helper); tested on one machine, see §6.

## 1. Goals

- A taskbar button shows how many notifications an app sent since its window
  was last in front, for apps that never send a LauncherEntry count (Discord,
  Firefox, Brave, Telegram and most Electron apps).
- The count follows what the user sees: bringing the window forward, or
  leaving it, resets it. Nothing is dropped from the notification center.
- An app that does send its own LauncherEntry count or progress is never
  overridden.

Not goals: see §5.

## 2. Decisions taken

| Decision | Why |
|---|---|
| Count in the shell, from `NotificationServer`, not a new service | It already sees every notification and the center (popups plus history). |
| App matched by the `desktop-entry` hint, else the app name; both spellings of a desktop id (`discord` and `com.discordapp.Discord`) | Real Discord sends `desktop-entry=discord`, the taskbar knows `com.discordapp.Discord.desktop`. |
| The app's own LauncherEntry count wins | It is more exact than counting notifications. |
| A replaced notification counts as a new message | Chat apps update their one notification per message; counting items only would stay at 1. |
| Reset when the window becomes active or stops being active (`markSeen`) | The user saw it, or is about to stop looking. The center keeps the notifications. |
| Not persisted | A shell restart starts from zero, like the badges themselves. |

## 3. Overview

```
Notify (D-Bus) ---> NotificationServer ---+--> popups / center (unchanged)
                          |               |
                          |  unreadFor(desktopId), markSeen(desktopId)
                          v
                 Taskbar.qml: badge = Badges.entry(id), count from unreadFor
                              when the app sent none and the window is not active
```

`unreadFor()` counts popups and center items of the app newer than the last
`markSeen()`, plus replacements since then. `unreadRevision` changes with
every popup, center and reset change, so the taskbar binding re-evaluates.

## 4. Steam download progress (optional)

Steam on Linux sends no progress to the taskbar. It writes progress to
`steamapps/appmanifest_*.acf`, but that file is only flushed now and then (it
showed 6% while Steam's own window showed 37%). The files Steam has staged
are current: the size of `steamapps/downloading/<appid>/`.

`vela-steam-progress` (`steam-progress/`) is a small standalone program (Qt Core and DBus, no Vela
dependency) that reads both, takes the larger staged figure, and sends a
LauncherEntry update for `steam.desktop`. The taskbar draws it like any other
progress. It is off by default (a CMake option, not started by the session)
and depends on Steam's file layout, so it may need updates when Steam changes.
Build it with `-DVELA_BUILD_STEAM_PROGRESS=ON` and run it next to the session
(it sends one update every 2 s while a download runs, and hides the bar after).

Chromium browsers (Brave, Chrome 155) were tried and send no LauncherEntry
signal, with or without libunity and `XDG_CURRENT_DESKTOP=Unity`; there is no
progress for them.

## 5. Not in scope

- A count in the notification center itself, per-app rules.
- Apps without a taskbar button (tray-only): the tray has its own indicators.
- Progress for Chromium browsers (see §4).

## 6. Tests

- Unit test (`shell/tests/notification_unread.cpp`): count by hint or by name
  in either desktop id spelling; `markSeen` resets only that app and a later
  notification counts again; replacements count as messages; cleared center
  items do not count.
- Steam helper (`steam-progress/test.cpp`, ctest `steam-progress`): a fake
  library with manifests and staged files: nothing running, a running
  download from the manifest, the live folder beating a stale manifest,
  several downloads as one fraction, a second library from
  `libraryfolders.vdf`, and the fraction never above 1.
- Not covered: the Taskbar.qml binding (no QML test target); checked by hand
  with Discord and a real Steam download on one machine (NVIDIA RTX 4060 only).
