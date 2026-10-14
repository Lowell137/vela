#!/usr/bin/env bash
# Exercises fork-sync.sh against a throw-away bare "upstream" (no network).
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d "${TMPDIR:-$HOME/vela-work}/fork-sync-test.XXXXXX")
trap 'rm -rf "$t"' EXIT
export GIT_AUTHOR_NAME=t GIT_AUTHOR_EMAIL=t@t GIT_COMMITTER_NAME=t GIT_COMMITTER_EMAIL=t@t
export VELA_REPO="$t/repo" VELA_WORK_ROOT="$t/work" VELA_STATE_DIR="$t/state" VELA_SKIP_BUILD=1
export PATH="$t/bin:$PATH"; mkdir "$t/bin"; printf '#!/bin/sh\necho "NOTIFY: $*" >> %s/notify.log\n' "$t" >"$t/bin/notify-send"; chmod +x "$t/bin/notify-send"
git init -q --bare -b main "$t/up.git"
git clone -q "$t/up.git" "$t/seed" 2>/dev/null
(cd "$t/seed" && echo a >f && git add f && git commit -qm base && git push -q origin HEAD:main)
git clone -q "$t/up.git" "$VELA_REPO"
git -C "$VELA_REPO" branch integration origin/main
state() { python3 -c "import json;print(json.load(open('$t/state/last.json'))['status'])"; }

"$here/fork-sync.sh" >/dev/null; [ "$(state)" = guncel ] || { echo FAIL guncel; exit 1; }

# 1) clean upstream commit -> merge-hazir, integration untouched
(cd "$t/seed" && echo b >g && git add g && git commit -qm up1 && git push -q origin HEAD:main)
before=$(git -C "$VELA_REPO" rev-parse integration)
"$here/fork-sync.sh" >/dev/null
[ "$(state)" = merge-hazir ] && [ "$(git -C "$VELA_REPO" rev-parse integration)" = "$before" ] || { echo FAIL clean; exit 1; }

# 2) conflicting change -> cakisma, no leftover branch
git -C "$VELA_REPO" worktree remove --force "$t/work/sync-src"
git -C "$VELA_REPO" branch -D $(git -C "$VELA_REPO" branch --list 'sync/*' --format '%(refname:short)') >/dev/null
git -C "$VELA_REPO" checkout -q integration && echo mine >"$VELA_REPO/f" && git -C "$VELA_REPO" commit -qam fork
(cd "$t/seed" && echo theirs >f && git commit -qam up2 && git push -q origin HEAD:main)
"$here/fork-sync.sh" >/dev/null
[ "$(state)" = cakisma ] && grep -q '"f"' "$t/state/last.json" || { echo FAIL conflict; exit 1; }
[ -z "$(git -C "$VELA_REPO" branch --list 'sync/*')" ] || { echo FAIL leftover; exit 1; }
grep -c NOTIFY "$t/notify.log" >/dev/null
echo "fork-sync test: OK"
