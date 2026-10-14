# Vela fork sync

`scripts/fork-sync.sh` watches upstream (`origin/main`) and, when it is ahead of
`integration`, prepares an update candidate. It installs nothing, pushes nothing
and never moves `integration`.

Flow: `git fetch origin` -> if `integration..origin/main` is empty, write
`guncel` and exit -> otherwise merge `origin/main` into `sync/<date>` (private
worktree `~/vela-work/sync-src`) -> on conflict: abort, record the files,
`notify-send`, exit -> otherwise build in `~/vela-work/sync-build`, run ctest
(`packaging-update` excluded; the 3 known-red `test_ready` sync tests inside
`functional` are tolerated and reported), then `makepkg --nodeps` in
`~/vela-work/sync-pkg`.

Result: `~/.local/state/vela-fork-sync/last.json` with `status`
(`guncel`, `cakisma`, `build-basarisiz`, `test-kirmizi`, `paket-basarisiz`,
`merge-hazir`), commit count, branch, package path, test summary and the two
commands below. All paths can be overridden with `VELA_*` env vars (see the
top of the script).

## Enable (manual, nothing is enabled for you)

    mkdir -p ~/.config/systemd/user
    cp packaging/fork-sync/vela-fork-sync.{service,timer} ~/.config/systemd/user/
    # edit ExecStart in the .service if the script lives elsewhere
    systemctl --user daemon-reload
    systemctl --user enable --now vela-fork-sync.timer

One-off run: `scripts/fork-sync.sh`. Logs: `journalctl --user -u vela-fork-sync`.

## After a `merge-hazir` result (needs your approval)

The exact commands are in `last.json` (`merge_command`, `install_command`):

    # fast-forward integration to the tested branch
    git -C <worktree-with-integration> merge --ff-only sync/<date>
    # install the package
    sudo pacman -U ~/vela-work/sync-pkg/vela-git-<ver>.pkg.tar.zst

## Test

`scripts/tests/fork-sync-test.sh` runs the script against a temporary bare
upstream (clean merge and conflict paths, build skipped via `VELA_SKIP_BUILD=1`).
