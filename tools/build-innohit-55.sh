#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build the shared INNO-Hit IH55UWB/IH55UWB4 webOS Hub profile.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KIT="$ROOT/external/webos-native-app-kit"
BUILD_DIR="${CSWEB_BUILD_DIR:-$KIT/build/webos-innohit55}"
VERSION="${CSWEB_VERSION:-0.1.0}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"

if [[ -z "${WEBOS_NDK:-}" ]]; then
    echo "Set WEBOS_NDK to the installed webOS NDK (see docs/legacy-build.md)." >&2
    exit 2
fi

"$ROOT/tools/fetch-kit.sh"
if [[ ! -x "$ROOT/runtime/dist/runtime-armv7/lib/ld-musl-armhf.so.1" ]]; then
    echo "Building the armv7 runtime; this requires Docker and can take a long time."
    "$ROOT/tools/build-runtime.sh" --arch armv7
fi

cmake --preset webos-release -B "$BUILD_DIR" \
    -DWNK_EXTRA_APP_DIRS="$ROOT/apps/csweb-tv" \
    -DCSWEB_INNOHIT_55=ON \
    -DCSWEB_VERSION="$VERSION"
cmake --build "$BUILD_DIR" -j"$JOBS" --target csweb-tv-ipk

printf '\nIPK output:\n%s\n' "$BUILD_DIR/dist/org.csweb.tv.legacy_${VERSION}_arm.ipk"
