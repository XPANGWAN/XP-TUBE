# XP-TUBE source inventory — migration baseline

## Snapshot

- XP-TUBE main commit when inventory was prepared: `bbdeeb646e24f253a75e8076a4234efebf28a2f1`
- Imported source revision: `7372c068704f568bf04e1f8b41a04a2149b2e048`
- Source root: `vendor/playtube-migration/`
- Source files under `src/`: 206
- Status: inventory only; **no directory below is represented as independently rewritten**.

## Source areas and file counts

| Area | Files | Responsibility and planned XP-TUBE boundary |
|---|---:|---|
| `src/app` | 53 | Application lifecycle, settings, profiles, screens and feature orchestration → xp/app and xp/ui |
| `src/core` | 24 | Version, data paths, storage primitives, JSON, logging, tasks and errors → xp/core |
| `src/main.cpp` | 1 | Supporting source area; classify individual files before migration |
| `src/net` | 8 | HTTP, URL, TLS and transport behavior → xp/net |
| `src/platform` | 27 | SDL, controller input, PS4 backend, system integration and language → xp/platform |
| `src/player` | 10 | libmpv adapter, playback state, seeking and media helpers → xp/playback |
| `src/render` | 8 | Software renderer, rasterization, compositing and canvas → xp/ui/rendering |
| `src/testing` | 6 | Supporting source area; classify individual files before migration |
| `src/ui` | 50 | Layout, focus, text, widgets, animation, images and translations → xp/ui |
| `src/youtube` | 19 | YouTube client, parsers, auth, search, channels and stream resolution → xp/youtube |

## Build-system facts observed

- The imported CMake project still declares `project(Playtube ...)`.
- CMake targets and C++ namespaces remain Playtube-derived.
- PS4 package metadata in the vendored source starts with legacy PLTB identifiers; the current CI patch transforms these for the XP-TUBE package.
- `scripts/build-ps4.sh` uses the pacbrew/OpenOrbis build environment and downloads dependency packages when local `build-deps` files are absent.
- External libraries such as SDL2, curl, Mbed TLS, libmpv, FFmpeg, FriBidi and the OpenOrbis toolchain are third-party dependencies, not XP-TUBE-authored modules. Their versions and licenses need their own manifest.

## Migration order and acceptance gates

1. **Build boundary:** keep the current working PS4 package reproducible while removing patch-on-source as a release architecture.
2. **XP-owned build and core:** create native XP-TUBE CMake targets and port core primitives with tests.
3. **Platform and rendering:** isolate PS4 services, controller mapping, renderer and resource loading behind XP interfaces.
4. **UI foundation:** create the XP-TUBE design tokens, navigation/focus model, components and screen framework.
5. **Application features:** rebuild home, search, video details, channels, library, account and settings using XP-owned screens.
6. **Services and playback:** maintain YouTube integration and internal playback behind XP-owned interfaces; document private API limitations.
7. **Removal gate:** delete the migration reference only after all required features have replacement implementations, license review is complete, and CI builds from XP-owned paths.

## Non-negotiable provenance rules

- Preserve GPL-3.0 and all required upstream/third-party copyright and license notices.
- Do not mechanically rename source files or namespaces and call that a rewrite.
- Record source origin for every copied/adapted implementation.
- Keep old tested functionality until a replacement has regression coverage.
- Do not claim hardware validation until the resulting PKG is tested on a real PS4.
