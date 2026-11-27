#!/usr/bin/env bash
# Runs a UI script headless (CPU rendering) and compares every reference screenshot.
# Usage: run.sh <playtube> <playtube_imgdiff> <script> <references-dir> <output-dir> [playtube options...]
set -euo pipefail
playtube=$1 imgdiff=$2 script=$3 refs=$4 out=$5
shift 5
rm -rf "$out"
mkdir -p "$out"
LC_ALL=fr_FR.UTF-8 SDL_VIDEODRIVER=offscreen MESA_LOADER_DRIVER_OVERRIDE=llvmpipe timeout 120 \
    "$playtube" --hidden "$@" --data-dir "$out/data" --test-script "$script" --test-output "$out"
shopt -s nullglob
references=("$refs"/*.png)
if [ ${#references[@]} -eq 0 ]; then
    echo "no reference images in $refs (scripts/update-ui-refs.sh)" >&2
    exit 1
fi
status=0
for ref in "${references[@]}"; do
    "$imgdiff" "$ref" "$out/$(basename "$ref")" || status=1
done
exit $status
