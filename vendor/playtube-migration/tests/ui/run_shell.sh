#!/usr/bin/env bash
# End to end: the fake YouTube over TLS (test CA), the shell pointed at it, then every reference screenshot compared.
# Usage: run_shell.sh <playtube> <playtube_imgdiff> <fake_youtube> <script> <references-dir> <output-dir>
#            ["fake_youtube options"] ["playtube options"]
# tests/ui/<script>.account starts it signed in; tests/ui/<script>.fake holds fake_youtube options
set -euo pipefail
playtube=$1 imgdiff=$2 fake=$3 script=$4 refs=$5 out=$6
fakeOptions=${7:-}
appOptions=${8:-}
here=$(cd "$(dirname "$0")" && pwd)
account="$here/$(basename "$script" .script).account"
fakeFile="$here/$(basename "$script" .script).fake"
if [ -z "$fakeOptions" ] && [ -f "$fakeFile" ]; then fakeOptions=$(cat "$fakeFile"); fi
rm -rf "$out"
mkdir -p "$out/data"
if [ -f "$account" ]; then cp "$account" "$out/data/account.json"; fi
# shellcheck disable=SC2086
"$fake" --url-file "$out/url" $fakeOptions > /dev/null &
server=$!
trap 'kill $server 2>/dev/null || true; wait $server 2>/dev/null || true' EXIT
for _ in $(seq 1 50); do
    [ -s "$out/url" ] && break
    sleep 0.1
done
# shellcheck disable=SC2086
LC_ALL=fr_FR.UTF-8 SDL_VIDEODRIVER=offscreen MESA_LOADER_DRIVER_OVERRIDE=llvmpipe timeout 120 \
    "$playtube" --hidden --screen shell --data-dir "$out/data" --test-script "$script" --test-output "$out" \
    --youtube "$(cat "$out/url")" --ca-file "$here/../fixtures/tls/ca.crt" $appOptions
for secret in demo-access-token demo-refresh-token fresh-access; do
    if grep -q "$secret" "$out/data/playtube.log"; then
        echo "a token reached the log" >&2
        exit 1
    fi
done
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
