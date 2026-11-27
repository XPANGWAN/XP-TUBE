#!/usr/bin/env bash
# The README's screenshots (images/screens/), made by the Linux build in English against the real YouTube,
# signed out (nothing of an account shows): V2F's channel and the settings; the home is signed in on the fake
# YouTube. YouTube's replies change: look at every picture before keeping it.
set -euo pipefail
cd "$(dirname "$0")/.."
cmake --build --preset linux-debug
build=build/linux-debug
out=$build/readme-out
rm -rf "$out"
mkdir -p "$out" images/screens
for script in scripts/readme/channel.script scripts/readme/settings.script; do
    data=$out/data-$(basename "$script" .script)
    mkdir -p "$data"
    LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8 SDL_VIDEODRIVER=offscreen MESA_LOADER_DRIVER_OVERRIDE=llvmpipe timeout 400 \
        "$build/playtube" --hidden --screen shell --data-dir "$data" --test-script "$script" --test-output "$out"
done
# The first screenshot, signed in: the home's For you and Subscriptions rows from the fake YouTube, filled with the
# latest videos of a few creators (scripts/readme/make-showcase.py); no topic rows
showcase=$out/showcase
python3 scripts/readme/make-showcase.py "$showcase"
data=$out/data-signed
mkdir -p "$data"
cp tests/ui/signed.account "$data/account.json"
echo '{"homeTopics":[]}' > "$data/settings.json"
"$build/tests/fake_youtube" --url-file "$out/url" --showcase "$showcase" > /dev/null &
server=$!
trap 'kill $server 2>/dev/null || true; wait $server 2>/dev/null || true' EXIT
for _ in $(seq 1 50); do
    [ -s "$out/url" ] && break
    sleep 0.1
done
LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8 SDL_VIDEODRIVER=offscreen MESA_LOADER_DRIVER_OVERRIDE=llvmpipe timeout 400 \
    "$build/playtube" --hidden --screen shell --data-dir "$data" --test-script scripts/readme/signed-home.script \
    --test-output "$out" --youtube "$(cat "$out/url")" --ca-file tests/fixtures/tls/ca.crt
# JPEG for the README (a fifth of the PNG's size)
python3 - "$out" images/screens <<'PY'
import glob, os, sys
from PIL import Image
for path in glob.glob(os.path.join(sys.argv[1], "*.png")):
    name = os.path.splitext(os.path.basename(path))[0] + ".jpg"
    Image.open(path).convert("RGB").save(os.path.join(sys.argv[2], name), quality=85, optimize=True)
PY
ls -l images/screens/
