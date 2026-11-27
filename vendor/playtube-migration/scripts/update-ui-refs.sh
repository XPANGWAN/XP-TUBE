#!/usr/bin/env bash
# Regenerates the UI reference screenshots of every tests/ui/*.script (French UI, software renderer). Review the
# images before committing. Every script but the gallery runs the shell against the fake YouTube; a
# tests/ui/<name>.account file starts it signed in, a tests/ui/<name>.fake file holds fake_youtube options.
set -euo pipefail
cd "$(dirname "$0")/.."
cmake --build --preset linux-debug
build=build/linux-debug
for script in tests/ui/*.script; do
    name=$(basename "$script" .script)
    out=$build/ui-refs/$name
    rm -rf "$out"
    mkdir -p "$out/data" "tests/ui/references/$name"
    options=(--screen "$name")
    server=
    if [ "$name" != gallery ]; then
        [ -f "tests/ui/$name.account" ] && cp "tests/ui/$name.account" "$out/data/account.json"
        fakeOptions=()
        [ -f "tests/ui/$name.fake" ] && read -r -a fakeOptions < "tests/ui/$name.fake"
        "$build/tests/fake_youtube" --url-file "$out/url" "${fakeOptions[@]}" > /dev/null &
        server=$!
        for _ in $(seq 1 50); do [ -s "$out/url" ] && break; sleep 0.1; done
        options=(--screen shell --youtube "$(cat "$out/url")" --ca-file tests/fixtures/tls/ca.crt)
    fi
    LC_ALL=fr_FR.UTF-8 SDL_VIDEODRIVER=offscreen MESA_LOADER_DRIVER_OVERRIDE=llvmpipe timeout 120 \
        "$build/playtube" --hidden "${options[@]}" --data-dir "$out/data" \
        --test-script "$script" --test-output "$out" || { [ -n "$server" ] && kill "$server"; exit 1; }
    if [ -n "$server" ]; then
        kill "$server"
        wait "$server" 2>/dev/null || true
    fi
    rm -f "tests/ui/references/$name"/*.png
    cp "$out"/*.png "tests/ui/references/$name/"
done
ls -la tests/ui/references/*
