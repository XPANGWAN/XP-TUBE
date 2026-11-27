# XP-TUBE source architecture review

## Scope and evidence

This review is based on the pinned source revision:

- Repository: https://github.com/LowganDev/PlaytubePS4
- Revision: `7372c068704f568bf04e1f8b41a04a2149b2e048`
- License file: `LICENSE` (GNU GPL v3)
- Build definition: `CMakeLists.txt`
- Dependency definition: `cmake/dependencies.cmake`
- PS4 packaging: `cmake/ps4_pkg.cmake`
- PS4 build entry point: `scripts/build-ps4.sh`

This is a source-level architecture map. The pinned source has now been imported into `vendor/playtube-migration/` with subtree ancestry, a provenance record, and a SHA-256 inventory. The import is complete; the implementation remains migration-reference code and has **not** been independently rewritten.

## Existing architecture discovered

The CMake project defines these major static libraries and executable:

| Existing target | Main responsibility | Main source areas |
|---|---|---|
| `playtube_base` | version, data paths, file I/O, JSON, logging, tasks, signals | `src/core` |
| `playtube_net` | URL, HTTP, crypto and TLS helpers | `src/net` |
| `playtube_render` | software rasterizer and compositing | `src/render` |
| `playtube_platform` | SDL, input, events, language, PS4 backend | `src/platform` |
| `playtube_ui` | widgets, layout, focus, text, images, animation, i18n | `src/ui` |
| `playtube_player` | libmpv engine, seeking, repeats, trickplay, WebP | `src/player` |
| `playtube_youtube` | YouTube client, parsing, auth, streams, playback, search | `src/youtube` |
| `playtube_core` | app lifecycle, settings, update, screens and session | `src/app` |
| `playtube` | application entry point and final executable | `src/main.cpp` |

The target names and C++ namespace are still inherited as `playtube` in the inspected source. Renaming identifiers alone is not a rewrite and is not an independence milestone.

## Important dependency findings

### PS4 platform and packaging

- PS4-specific platform code lives under `src/platform/ps4`.
- Packaging is defined in `cmake/ps4_pkg.cmake`.
- The build script runs inside the `xfangfang/pacbrew:250221` container.
- The script downloads PS4 dependency packages from the upstream release endpoint unless local `build-deps` packages are supplied.
- The build relies on the OpenOrbis toolchain and libraries such as SDL2, curl, Mbed TLS, mpv, FFmpeg, FriBidi, and system PS4 services.

These toolchain and library dependencies are not inherently Playtube dependencies. They must be documented and retained only with license and compatibility checks.

### YouTube integration

The inspected `src/youtube/client.cpp` implements multiple InnerTube client profiles (WEB, TVHTML5, VISIONOS, ANDROID_VR, IOS, ANDROID). This is an undocumented/private service interface and can change independently of XP-TUBE. Treat it as a reliability and maintenance risk; do not claim permanent compatibility.

### Playback

The project uses libmpv for video playback and FFmpeg components for decoding/support. The player implementation is in `src/player`. A replacement must preserve internal playback and PS4 controller behavior, and must be tested on hardware.

### UI and settings

The UI toolkit is in `src/ui`; application screens are in `src/app/shell`. The settings screen is a single large screen implementation with rows for display, playback, subtitles, network/service behavior, and other preferences. A genuinely original XP-TUBE interface requires redesigning the screen hierarchy and interaction model, not merely changing the heading or color tokens.

## Rewrite boundaries

These are the recommended XP-TUBE-owned boundaries. They are architectural targets, not claims of completed work:

1. `xp/core`: lifecycle, configuration, data paths, logging, async tasks, error/result types.
2. `xp/platform`: PS4 lifecycle, controller input, system dialogs, display and audio integration.
3. `xp/net`: HTTP transport, TLS policy, retry/cancellation, request diagnostics.
4. `xp/youtube`: service client, parsing, search, channels, metadata, stream resolution, account capabilities.
5. `xp/playback`: player abstraction, libmpv adapter, stream lifecycle, controller mapping.
6. `xp/ui`: XP-TUBE design system, navigation, focus, components, screens and settings.
7. `xp/app`: application state, feature orchestration, persistence and update management.

Do not mechanically rename the current directories and claim independence. New modules must have clear XP-TUBE ownership, tests, and documented origins for any code retained or adapted.

## Migration sequence

1. Complete the provenance-preserving import and verify the exact upstream commit and retained license files.
2. Inventory every source file and classify it as upstream-derived, XP-TUBE-authored, or third-party.
3. Extract the minimum buildable PS4 platform and toolchain layer into the XP-TUBE-owned repository layout.
4. Establish XP-TUBE-native CMake targets and CI.
5. Port or rewrite core, network, platform, UI, YouTube, playback and app modules in dependency order.
6. Add regression tests before replacing working behavior.
7. Remove the temporary migration reference only after the independent implementations build and pass tests.

## Explicit non-goals

- Do not delete required GPL notices, upstream copyright notices, or third-party licenses.
- Do not modify or import data from an installed Playtube application.
- Do not bypass DRM, account security, or access controls.
- Do not promise permanent compatibility with undocumented YouTube endpoints.
- Do not label the application fully independent until the release build and runtime pass the final dependency audit.
