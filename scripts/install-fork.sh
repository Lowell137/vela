#!/bin/sh
# SPDX-FileCopyrightText: 2026 Brando Giuffrida
# SPDX-License-Identifier: GPL-3.0-or-later

# Installs this fork of Vela (Lowell137/vela: the themes and everything else) in one command.
# On Arch, CachyOS or EndeavourOS: downloads the PKGBUILD, builds the package from the
# fork's branch and installs it with pacman.
# On Ubuntu 26.10 or newer (and Debian with wlroots 0.20): installs the packages with apt,
# builds the fork from source and installs it under /usr. Run the script again to update.
# The Ubuntu path is only built and unit-tested, not run on real hardware yet.
#
#   sh -c "$(curl -fsSL https://raw.githubusercontent.com/Lowell137/vela/main/scripts/install-fork.sh)"
#
# Run it as your user, not root: makepkg refuses root and asks for sudo itself
# (build dependencies, then the install). VELA_BRANCH picks another branch.

set -eu

REPO="https://github.com/Lowell137/vela"
BRANCH="${VELA_BRANCH:-main}"

if [ "$(id -u)" -eq 0 ]; then
    echo "Run it as your user, not root: it asks for sudo when it needs it." >&2
    exit 1
fi

install_ubuntu() {
    if ! apt-cache policy libwlroots-0.20-dev 2>/dev/null | grep -q 'Candidate: [0-9]'; then
        echo "libwlroots-0.20-dev is not in this system's repositories (Vela needs wlroots 0.20)." >&2
        echo "Ubuntu 26.10 or newer has it. Enable 'universe' and run: sudo apt update" >&2
        exit 1
    fi
    echo ":: Build dependencies (apt)"
    sudo apt-get update
    # The upstream CI list plus what this fork adds (archives, video, terminal, multimedia).
    sudo env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates git curl build-essential cmake ninja-build pkgconf \
        libwlroots-0.20-dev libwayland-dev wayland-protocols libxkbcommon-dev \
        libpixman-1-dev libinput-dev libgbm-dev libdrm-dev \
        libvulkan-dev glslc libfreetype-dev libharfbuzz-dev libfontconfig-dev \
        libcairo2-dev librsvg2-dev libpam0g-dev libpolkit-agent-1-dev \
        libpolkit-gobject-1-dev zlib1g-dev libgtest-dev libsystemd-dev \
        qt6-base-dev qt6-base-private-dev qt6-declarative-dev \
        qt6-svg-dev qt6-wayland-dev qt6-tools-dev qt6-tools-dev-tools \
        qt6-l10n-tools liblayershellqtinterface-dev \
        libpipewire-0.3-dev libwireplumber-0.5-dev \
        python3 python3-numpy python3-pil dbus \
        libarchive-dev libmpv-dev libvterm-dev qt6-multimedia-dev

    # What Vela needs to run (the QML modules are separate packages on Debian) and the
    # optional extras. A missing name is reported and skipped, not fatal: the names
    # are not checked against every release.
    missing=""
    for pkg in xwayland polkitd pipewire wireplumber \
        qml6-module-qtcore qml6-module-qtqml qml6-module-qtqml-workerscript \
        qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtquick-layouts \
        qml6-module-qtquick-controls qml6-module-qtquick-templates qml6-module-qtquick-dialogs \
        qml6-module-qtquick-shapes qml6-module-qtquick-effects qml6-module-qtquick-pdf \
        qml6-module-qtmultimedia qt6-wayland xdg-desktop-portal xdg-desktop-portal-wlr \
        xdg-desktop-portal-gtk xdg-utils sound-theme-freedesktop breeze-icon-theme \
        network-manager bluez power-profiles-daemon udisks2 ffmpeg imagemagick \
        qt6-image-formats-plugins pulseaudio-utils tesseract-ocr; do
        if apt-cache show "$pkg" >/dev/null 2>&1; then
            sudo env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "$pkg" \
                || missing="$missing $pkg"
        else
            missing="$missing $pkg"
        fi
    done
    [ -z "$missing" ] || echo ":: Not installed (missing or failed):$missing"

    src="${XDG_CACHE_HOME:-$HOME/.cache}/vela-src"
    echo ":: Source from $REPO ($BRANCH)"
    if [ -d "$src/.git" ]; then
        git -C "$src" fetch --depth 1 origin "$BRANCH"
        git -C "$src" checkout -q --detach FETCH_HEAD
    else
        git clone --depth 1 --branch "$BRANCH" "${VELA_GIT_URL:-$REPO.git}" "$src"
    fi

    # Same options as the Arch package. libexec in lib/vela, because the compositor
    # looks for its helpers there.
    cmake -S "$src" -B "$src/build" -G Ninja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_INSTALL_LIBEXECDIR=lib/vela \
        -DVELA_BUILD_TOOLS=OFF \
        -DVELA_BUILD_STEAM_PROGRESS=ON \
        -Wno-dev
    # The compiler needs about 2 GB per job: fewer jobs on a small machine.
    jobs=$(nproc)
    mem_jobs=$(awk '/MemTotal/ { print int($2 / 2000000) }' /proc/meminfo)
    [ "$mem_jobs" -ge 1 ] || mem_jobs=1
    [ "$mem_jobs" -lt "$jobs" ] && jobs=$mem_jobs
    echo ":: Building with $jobs jobs (a few minutes)"
    cmake --build "$src/build" --parallel "$jobs"

    echo ":: Installing under /usr"
    sudo cmake --install "$src/build"
    # To uninstall: xargs -a ~/.local/share/vela/install_manifest.txt sudo rm -f
    mkdir -p "$HOME/.local/share/vela"
    cp "$src/build/install_manifest.txt" "$HOME/.local/share/vela/install_manifest.txt"

    echo ":: Done. Log out and choose Vela with the gear on the login screen."
    echo "   To update, run this script again. Settings > Updates does not support Ubuntu yet."
}

if command -v apt-get >/dev/null 2>&1; then
    install_ubuntu
    exit 0
fi
if ! command -v pacman >/dev/null 2>&1; then
    echo "This script is for Arch, CachyOS and EndeavourOS (pacman), and Ubuntu 26.10 (apt)." >&2
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
