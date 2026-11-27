# Playtube 1.2.0 baseline review

## Source of truth

- Upstream: https://github.com/LowganDev/PlaytubePS4
- Pinned commit: `7372c068704f568bf04e1f8b41a04a2149b2e048`
- Upstream release line: Playtube 1.2.0 (2026-11-21)
- Upstream license: GPL-3.0; third-party components retain their own licenses and notices.

Do not silently track a moving upstream branch for a reproducible baseline. Update the pinned commit only after reviewing the upstream diff and release notes.

## Architecture observed

| Layer | Upstream implementation | Review notes |
|---|---|---|
| Application / screens | C++17, application and shell modules | Keep the proven navigation and controller-first interaction model during baseline validation. |
| UI / rendering | SDL2 platform layer, NanoVG software rendering, custom UI toolkit | PS4 rendering and input paths are distinct from Linux development paths. |
| Playback | libmpv 0.36.0 with FFmpeg 7.1.5 | Test codecs, resolution, frame pacing, audio, subtitles, and long sessions on real hardware. |
| YouTube access | Internal, undocumented InnerTube client | Fragile dependency; response changes, throttling, and anti-bot blocks are expected risks. |
| Networking | libcurl 8.22.0, Mbed TLS 3.6.7 on PS4 | Certificate and resolver behavior must be verified on console. |
| Profiles / auth | Optional device-code sign-in and per-console-account profiles | Review token storage and logout/data deletion behavior before XP-TUBE distribution. |
| Quality / diagnostics | Unit tests, Linux test harness, sanitizers, fuzz targets, PS4 import/EBOOT/license checks | Keep checks in CI; a green build is not equivalent to a successful console test. |

## Important reliability findings

1. The 1.2.0 changelog records fixes for stream interruption/resume, missing video-page retries, watch-history reporting, and repeated log warnings.
2. The upstream issue tracker documents YouTube anti-bot checks and temporary IPv4 blocking. Upstream behavior reports the block and reduces requests; signing in does not lift it.
3. SponsorBlock skips community-submitted sponsor/self-promotion segments. It does not establish that every YouTube-inserted advertisement is removed.
4. The upstream player uses software decoding in the reviewed implementation. Advertised maximum resolution must be treated as a target, not a guarantee for every codec, frame rate, or stream.
5. Authentication tokens and profile data require a dedicated security review before changing storage locations or shipping an XP-branded derivative.

## Identity and data isolation requirements

XP-TUBE must not reuse Playtube's release identity or storage directory. The baseline workflow deliberately builds the upstream **development** package (the upstream `PLAYTUBE_DEV` option) so that the first test package is distinguishable from Playtube. Before an XP-TUBE-branded package is produced, inspect and change every identity surface together:

- PKG title ID / content ID and application metadata
- application name, icon, param.sfo and package artwork
- persistent data directory and all profile/config/log/update paths
- update manifest/channel and downloaded package destination
- network user-agent or client identity only where technically and legally appropriate

No identity change is considered complete until static checks confirm there are no unintended Playtube release IDs or shared data paths.

## License and attribution gate

The upstream project is GPL-3.0 and includes components under separate licenses. Any derivative distribution must comply with GPL-3.0, provide the required corresponding source and notices, and retain third-party license texts. Do not remove the upstream author's attribution or relicense third-party components as XP-TUBE code. Keep a component manifest and automated license audit in CI.

## Staged implementation plan

### Batch 1 — Establish a known-good baseline
1. Pin the upstream source revision.
2. Build the upstream development PKG in GitHub Actions.
3. Inspect build logs, package output, imports, EBOOT, and license checks.
4. Install and test the development PKG alongside Playtube; preserve the original app and its data.

### Batch 2 — Create independent XP-TUBE identity
1. Import the reviewed source with complete history/attribution where practical.
2. Set a unique XP-TUBE title/content ID.
3. Isolate all data, profile, cache, log, and update paths.
4. Apply XP-TUBE branding without disturbing proven playback behavior.

### Batch 3 — Reliability and security
1. Add startup, shutdown, network, playback, and crash diagnostics.
2. Test reconnect, stream interruption, long playback, memory pressure, and unsupported codecs.
3. Review token handling, profile isolation, TLS, and deletion behavior.
4. Add regression tests and license/import checks.

### Batch 4 — Product validation
1. Validate search, recommendations, channels, Shorts, live streams, comments, and account features.
2. Validate DualShock 4 navigation and all player controls.
3. Measure playback quality and stability on the actual PS4.
4. Produce a release candidate only after CI and console test evidence.

## Current non-goals

- Do not replace or uninstall the user's working Playtube installation.
- Do not merge this research branch into `main` without explicit approval.
- Do not promise universal ad removal or immunity from YouTube anti-bot restrictions.
- Do not bypass DRM or other access controls.
