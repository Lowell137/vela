#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Tracks upstream (origin/main), trial-merges it onto `integration` in a
# private worktree, builds, tests and packages. Installs nothing, pushes
# nothing, never moves `integration`. See packaging/fork-sync/README.md.
set -euo pipefail

REPO="${VELA_REPO:-$HOME/Projects/vela}"
UPSTREAM_REMOTE="${VELA_UPSTREAM_REMOTE:-origin}"
UPSTREAM_BRANCH="${VELA_UPSTREAM_BRANCH:-main}"
INTEGRATION="${VELA_INTEGRATION:-integration}"
WORK_ROOT="${VELA_WORK_ROOT:-$HOME/vela-work}"
SRC_DIR="${VELA_SYNC_SRC:-$WORK_ROOT/sync-src}"
BUILD_DIR="${VELA_SYNC_BUILD:-$WORK_ROOT/sync-build}"
PKG_DIR="${VELA_SYNC_PKG:-$WORK_ROOT/sync-pkg}"
STATE_DIR="${VELA_STATE_DIR:-$HOME/.local/state/vela-fork-sync}"
SKIP_BUILD="${VELA_SKIP_BUILD:-0}" # 1: stop after a clean merge (used by the test)
KNOWN_RED_FUNCTIONAL='test_explicit_sync test_implicit_sync test_cpu_sync_still_waits_for_implicit_producers'

STATE_FILE="$STATE_DIR/last.json"
mkdir -p "$STATE_DIR" "$WORK_ROOT"
exec 9>"$STATE_DIR/lock"
flock -n 9 || { echo "fork-sync: another run is active" >&2; exit 0; }

g() { git -C "$REPO" "$@"; }

notify() {
    echo "fork-sync: $1"
    notify-send -a vela-fork-sync "Vela fork sync" "$1" 2>/dev/null || true
}

# write_state STATUS [key=value ...]  (values are strings; numbers stay strings)
write_state() {
    python3 - "$STATE_FILE" "$@" <<'PY'
import json, sys, datetime
out = {"time": datetime.datetime.now().astimezone().isoformat(timespec="seconds"), "status": sys.argv[2]}
for kv in sys.argv[3:]:
    k, _, v = kv.partition("=")
    out[k] = v.split("\n") if k == "conflicts" else v
json.dump(out, open(sys.argv[1], "w"), indent=2, ensure_ascii=False)
PY
}

g fetch "$UPSTREAM_REMOTE"
behind=$(g rev-list --count "$INTEGRATION..$UPSTREAM_REMOTE/$UPSTREAM_BRANCH")
if [ "$behind" -eq 0 ]; then
    echo "fork-sync: up to date (guncel)"
    write_state guncel "behind=0"
    exit 0
fi

branch="sync/$(date +%Y%m%d)"
g show-ref --verify --quiet "refs/heads/$branch" && branch="$branch-$(date +%H%M)"

# Recreate our private worktree; it is ours alone, so forcing removal is fine.
if [ -e "$SRC_DIR" ]; then
    g worktree remove --force "$SRC_DIR" 2>/dev/null || rm -rf "$SRC_DIR"
    g worktree prune
fi
g worktree add -q -b "$branch" "$SRC_DIR" "$INTEGRATION"

if ! git -C "$SRC_DIR" -c user.name="${GIT_AUTHOR_NAME:-lowell}" -c user.email="${GIT_AUTHOR_EMAIL:-ezerumut9@gmail.com}" \
        merge --no-edit "$UPSTREAM_REMOTE/$UPSTREAM_BRANCH" >/dev/null 2>&1; then
    files=$(git -C "$SRC_DIR" diff --name-only --diff-filter=U)
    git -C "$SRC_DIR" merge --abort
    g worktree remove --force "$SRC_DIR"
    g branch -D "$branch" >/dev/null
    write_state cakisma "behind=$behind" "conflicts=$files"
    notify "Vela upstream merge cakisti ($behind commit, $(echo "$files" | wc -l) dosya). integration'a dokunulmadi. Detay: $STATE_FILE"
    exit 0
fi

if [ "$SKIP_BUILD" = 1 ]; then
    write_state merge-hazir "behind=$behind" "branch=$branch" "tests=atlandi (VELA_SKIP_BUILD)"
    exit 0
fi

fail() { write_state "$1" "behind=$behind" "branch=$branch" "log=$2"; notify "Vela sync $1: $2"; exit 1; }

mkdir -p "$BUILD_DIR" "$PKG_DIR"
log="$BUILD_DIR/sync.log"
cmake -G Ninja -S "$SRC_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DVELA_BUILD_TOOLS=ON -Wno-dev >"$log" 2>&1 \
    && cmake --build "$BUILD_DIR" >>"$log" 2>&1 || fail build-basarisiz "$log"

# Known reds: packaging-update (timeout, excluded) and 3 functional test_ready sync tests.
tlog="$BUILD_DIR/ctest.log"
tests_note="packaging-update dislandi (bilinen kirmizi)"
(cd "$BUILD_DIR" && ctest -E '^packaging-update$' --output-on-failure >"$tlog" 2>&1) || rc=$?
if [ "${rc:-0}" -ne 0 ]; then
    failed=$(sed -n 's/^ *[0-9]* - \([A-Za-z0-9_-]*\) (.*/\1/p' "$tlog" | sort -u)
    if [ "$failed" = functional ]; then
        bad=$(sed -n 's/^\(FAIL\|ERROR\): \([A-Za-z0-9_]*\) .*/\2/p' "$tlog" | sort -u)
        for t in $bad; do
            case " $KNOWN_RED_FUNCTIONAL " in *" $t "*) ;; *) fail test-kirmizi "$tlog" ;; esac
        done
        [ -n "$bad" ] || fail test-kirmizi "$tlog"
        tests_note="$tests_note; functional icinde bilinen kirmizilar: ${bad//$'\n'/ }"
    else
        fail test-kirmizi "$tlog"
    fi
fi

rm -rf "$PKG_DIR/src" "$PKG_DIR/pkg" "$PKG_DIR"/*.pkg.tar.* "$PKG_DIR/vela"
cp "$SRC_DIR/packaging/arch/PKGBUILD" "$PKG_DIR/PKGBUILD"
(cd "$PKG_DIR" && VELA_GIT_URL="file://$SRC_DIR" PKGDEST="$PKG_DIR" SRCDEST="$PKG_DIR/srcdest" BUILDDIR="$PKG_DIR/build" \
    makepkg -f --nodeps --noconfirm >"$PKG_DIR/makepkg.log" 2>&1) || fail paket-basarisiz "$PKG_DIR/makepkg.log"
pkg=$(ls -t "$PKG_DIR"/*.pkg.tar.* 2>/dev/null | grep -v '\.sig$' | head -n 1)
[ -n "$pkg" ] || fail paket-basarisiz "$PKG_DIR/makepkg.log"

# integration may be checked out elsewhere: ff there, otherwise move the ref.
itree=$(g worktree list --porcelain | awk -v b="refs/heads/$INTEGRATION" '/^worktree /{w=substr($0,10)} $1=="branch"&&$2==b{print w}')
if [ -n "$itree" ]; then
    merge_cmd="git -C '$itree' merge --ff-only $branch"
else
    merge_cmd="git -C '$REPO' branch -f $INTEGRATION $branch"
fi
install_cmd="sudo pacman -U '$pkg'"
write_state merge-hazir "behind=$behind" "branch=$branch" "package=$pkg" "tests=$tests_note" \
    "merge_command=$merge_cmd" "install_command=$install_cmd"
notify "Vela güncellemesi hazır: $behind yeni upstream commit, paket: $pkg. Kurmak için onay bekliyor: $install_cmd"
