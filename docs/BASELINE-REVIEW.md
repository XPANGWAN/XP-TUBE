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
| Application / screens | C++17, application and shell modules | Proven navigation and controller-first interaction model. |
| UI / rendering | SDL2 platform layer, NanoVG software rendering, custom UI toolkit | PS4 rendering and input paths differ from Linux development paths. |
| Playback | libmpv 0.36.0 with FFmpeg 7.1.5 | Test codecs, resolution, frame pacing, audio, subtitles, and long sessions on real hardware. |
| YouTube access | Internal, undocumented InnerTube client | Fragile dependency; response changes, throttling, and anti-bot blocks are expected risks. |
| Networking | libcurl 8.22.0, Mbed TLS 3.6.7 on PS4 | Certificate and resolver behavior must be verified on console. |
| Profiles / auth | Optional device-code sign-in and per-console-account profiles | Refresh token is stored as plain JSON in the reviewed code; security hardening is required before distribution. |
| Quality / diagnostics | Unit tests, Linux test harness, sanitizers, fuzz targets, PS4 import/EBOOT/license checks | A green build is not equivalent to a successful console test. |

## Important reliability and security findings

1. The 1.2.0 changelog records fixes for stream interruption/resume, missing video-page retries, watch-history reporting, and repeated log warnings.
2. The upstream issue tracker documents YouTube anti-bot checks and temporary IPv4 blocking. Signing in does not automatically lift such a block.
3. SponsorBlock skips community-submitted sponsor/self-promotion segments. It does not establish that every YouTube-inserted advertisement is removed.
4. The reviewed PS4 player uses software decoding. Advertised maximum resolution is a target, not a guarantee for every codec, frame rate, or stream.
5. `src/youtube/account.cpp` persists `refresh_token` in a regular JSON file without encryption in this revision.
6. **Critical first-run migration behavior:** `src/app/app.cpp` calls `session.importPrevious("/data/Playtube/config.json")` on PS4 when `onlyProfile(consoleUser)` is true. `src/app/shell/session.cpp` and `src/youtube/account.cpp` read the old Playtube config and can copy its YouTube refresh token into the new app's profile. The old config is read-only, but the token is duplicated into the development app's data. The baseline development package is therefore a build-validation artifact only; do not install it on the user's console as the XP-TUBE test build.
7. The upstream device-flow OAuth client values are embedded in source. Before shipping an independently branded app, review the authentication flow and provider requirements rather than assuming the existing client registration is transferable.

## Hardening work started

### Playback recovery

The upstream player already checks stream URLs before handing them to mpv, refreshes URLs after long pauses, and restarts streams that end materially before the reported duration. A remaining failure path is an HTTP/network error after playback has begun: the previous behavior immediately opened the error dialog even when the signed stream URL had simply expired or become invalid.

XP-TUBE now makes one guarded automatic fresh-stream request after an mpv playback error, preserving the last known playback position. The guard prevents an endless retry loop; it is released after the next first frame or when the refreshed stream request fails. If recovery cannot start playback, the normal Retry/Back dialog remains available. This is a source-level improvement and still requires real PS4 testing against actual network interruptions.

### Account credential storage

The refresh token is stored in an atomically written JSON file with mode 0600; the access token remains in memory, and sign-out clears the persisted refresh token. This is filesystem permission hardening, **not encryption at rest**. Do not claim the token is encrypted or protected from a process running as the same console user. A future secure-storage change must use a supported per-user platform key/secure-storage facility; embedding a static encryption key in the application would only disguise the plaintext and is not an acceptable fix.

### Network and YouTube compatibility

InnerTube is undocumented and its client profiles, visitor data, stream reachability probes, and response parsers can change independently of XP-TUBE. Keep captured fixtures and fake-server regression tests, classify HTTP 401/403 separately from network timeouts and server errors, and never treat a successful Linux fixture test as proof that YouTube will serve every real PS4 connection. Do not bypass DRM or access controls.

## Identity and data isolation requirements

The upstream release app uses title ID `PLTB00000` and data folder `/data/Playtube`. The development build uses title ID `PLTB00001` and data folder `/data/PlaytubeDev`. The development build is distinguishable from the release app, but its first-run migration deliberately reads `/data/Playtube/config.json`.

For XP-TUBE, the importer must be disabled or replaced with an explicit, user-consented, documented migration. Default behavior must be **no read, copy, or write** of Playtube's data, account token, settings, or history. XP-TUBE must have a new unique title/content ID and its own data root.

Before an XP-TUBE-branded package is produced, inspect and change every identity surface together:

- PKG title ID / content ID and application metadata
- application name, icon, param.sfo and package artwork
- persistent data directory and all profile/config/log/update paths
- update manifest/channel and downloaded package destination
- any migration code that references Playtube paths
- network user-agent or client identity only where technically and legally appropriate

Add static CI assertions that reject `PLTB00000`, `PLTB00001`, `/data/Playtube`, and `/data/PlaytubeDev` in XP-TUBE runtime sources and package metadata, except in clearly documented migration tests or attribution docs.

## License and attribution gate

The upstream project is GPL-3.0 and includes components under separate licenses. Any derivative distribution must comply with GPL-3.0, provide the required corresponding source and notices, and retain third-party license texts. Do not remove the upstream author's attribution or relicense third-party components as XP-TUBE code. Keep a component manifest and automated license audit in CI.

## Staged implementation plan

### Batch 1 — Establish a known-good baseline
1. Pin the upstream source revision.
2. Build the upstream development PKG in GitHub Actions for toolchain/packaging validation only.
3. Inspect build logs, package output, imports, EBOOT, and license checks.
4. Do not install the upstream development package: its first-run migration can copy the Playtube sign-in token. Console testing will use the isolated XP-TUBE package after the migration code is removed.

### Batch 2 — Create independent XP-TUBE identity
1. Import the reviewed source with complete history/attribution where practical.
2. Set a unique XP-TUBE title/content ID.
3. Disable the old Playtube importer and isolate all data, profile, cache, log, and update paths.
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
- Do not install the upstream development baseline package.
- Do not merge this research branch into `main` without explicit approval.
- Do not promise universal ad removal or immunity to YouTube anti-bot restrictions.
- Do not bypass DRM or other access controls.
