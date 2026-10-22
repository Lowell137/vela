#!/bin/sh
# SPDX-FileCopyrightText: 2026 Brando Giuffrida
# SPDX-License-Identifier: GPL-3.0-or-later

# Installs this fork of Vela (Lowell137/vela, with the themes) on Arch,
# CachyOS or EndeavourOS in one command: downloads the PKGBUILD, builds the
# package from the fork's branch and installs it with pacman.
#
#   sh -c "$(curl -fsSL https://raw.githubusercontent.com/Lowell137/vela/main/scripts/install-fork.sh)"
#
# Run it as your user, not root: makepkg refuses root and asks for sudo itself
# (build dependencies, then the install). VELA_BRANCH picks another branch.

set -eu

REPO="https://github.com/Lowell137/vela"
BRANCH="${VELA_BRANCH:-main}"

if ! command -v pacman >/dev/null 2>&1; then
    echo "This script is for Arch, CachyOS and EndeavourOS (pacman)." >&2
    exit 1
fi
if [ "$(id -u)" -eq 0 ]; then
    echo "Run it as your user, not root: it asks for sudo when it needs it." >&2
    exit 1
fi

echo ":: Build tools (base-devel, git)"
sudo pacman -S --needed base-devel git curl

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
echo ":: PKGBUILD from $REPO ($BRANCH)"
curl -fsSL "https://raw.githubusercontent.com/Lowell137/vela/$BRANCH/packaging/arch/PKGBUILD" -o "$work/PKGBUILD"

cd "$work"
echo ":: Building (a few minutes), then installing"
# -s: the dependencies from the repositories; -i: pacman -U at the end
# (it offers to replace an installed vela or vela-git).
VELA_GIT_URL="$REPO.git#branch=$BRANCH" makepkg -si

echo ":: Done. Log out and choose Vela on the login screen (or log in again)."

# Settings > Updates (vela-update) follows this fork, not the official project.
config="${XDG_CONFIG_HOME:-$HOME/.config}/vela/updates.conf"
if ! grep -qs '^source=' "$config"; then
    mkdir -p "$(dirname "$config")"
    printf 'source=%s.git\n' "$REPO" >> "$config"
fi
