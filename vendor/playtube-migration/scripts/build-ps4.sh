#!/usr/bin/env bash
# Builds XP-TUBE using dependency packages produced locally from pinned recipes.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO_ROOT="$(cd "$ROOT/../.." && pwd)"
IMAGE="xfangfang/pacbrew:250221"
PLAYTUBE_DEV="${PLAYTUBE_DEV:-0}"
PLAYTUBE_START_LOOP="${PLAYTUBE_START_LOOP:-0}"

# Pass one literal container script; the quoted heredoc prevents host expansion.
docker run --rm \
    -v "$ROOT":/src \
    -v "$REPO_ROOT/xp":/xp-native:ro \
    -e OPENORBIS=/opt/pacbrew/ps4/openorbis \
    -e PLAYTUBE_DEV="$PLAYTUBE_DEV" \
    -e PLAYTUBE_START_LOOP="$PLAYTUBE_START_LOOP" \
    "$IMAGE" "$(cat <<'CONTAINER_SCRIPT'
set -euo pipefail
while IFS= read -r dependency; do
    dependency="${dependency%%#*}"
    dependency="${dependency//[[:space:]]/}"
    [[ -z "$dependency" ]] && continue
    package="ps4-openorbis-${dependency}-any.pkg.tar.gz"
    if [[ -s "build-deps/$package" ]]; then
        pacman --noconfirm -U "build-deps/$package"
    else
        echo "Missing locally built dependency package: build-deps/$package" >&2
        exit 1
    fi
done < scripts/ps4/deps.txt
if [[ "$PLAYTUBE_DEV" == "1" ]]; then dev_option=ON; else dev_option=OFF; fi
if [[ "$PLAYTUBE_START_LOOP" == "1" ]]; then loop_option=ON; else loop_option=OFF; fi
trap 'chown -R "$(id -u):$(id -g)" build/ps4' EXIT
cmake --preset ps4-release \
    -DPLAYTUBE_DEV="$dev_option" \
    -DPLAYTUBE_START_LOOP="$loop_option" \
    -DXP_TUBE_USE_NATIVE_STORAGE=ON \
    -DXP_TUBE_NATIVE_ROOT=/xp-native \
    -DBUILD_TESTING=OFF
cmake --build --preset ps4-release -j"$(nproc)"
CONTAINER_SCRIPT
)"
"$ROOT/scripts/check-ps4-imports.sh" "$ROOT/build/ps4/playtube" "$ROOT/build/ps4/pkg/eboot.bin"
python3 "$ROOT/scripts/check-ps4-eboot.py" "$ROOT/build/ps4/playtube" "$ROOT/build/ps4/pkg/eboot.bin"
python3 "$ROOT/scripts/check-ps4-licenses.py" \
    "$ROOT/build/ps4/CMakeFiles/playtube.dir/link.txt" \
    "$ROOT/resources/licenses/manifest.txt"
mkdir -p "$ROOT/dist"
pkg="$(ls -t "$ROOT"/build/ps4/pkg/IV0001-XPTB*.pkg | head -1)"
test -s "$pkg"
cp "$pkg" "$ROOT/dist/"
ls -la "$ROOT/dist/$(basename "$pkg")"
