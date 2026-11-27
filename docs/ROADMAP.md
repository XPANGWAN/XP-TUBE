# XP-TUBE — full independence roadmap

## Final definition of done

XP-TUBE is independent only when all of the following are true:

1. GitHub Actions does not clone, download, or otherwise fetch Playtube source to build XP-TUBE.
2. The build consumes source owned and maintained in the XP-TUBE repository.
3. Runtime code does not link to, load, or require Playtube components or services.
4. The Playtube patching/generation layer has been removed from the release build.
5. All Playtube-derived components selected for replacement have been rewritten or replaced with documented, independently maintained implementations.
6. The user-facing UI contains no Playtube branding.
7. Required GPL and third-party notices remain intact.
8. Search, channels, videos, account features where supported, library, and internal playback pass regression tests.
9. A clean PKG builds from the XP-TUBE repository alone and is tested on real PS4 hardware.

A new name, title ID, isolated data directory, or redesigned UI alone does not satisfy this definition.

## Repository policy

- Active branch: `main`.
- Backup branch: `feature/xp-tube-independence`, preserved at the pre-main migration baseline.
- Historical research branch: `research/playtube-baseline`, preserved.
- Never delete these backups or rewrite their history.
- All normal development is committed directly to `main`.
- Do not use force-push.
- Keep the last known-good PKG and build evidence until the replacement passes PS4 testing.

## Stage 0 — Establish the working baseline

- [x] Advance `main` safely to the last known development baseline.
- [x] Preserve the former development branch as a backup.
- [x] Enable package build and host test workflows on `main`.
- [x] Identify the current dependency: both workflows clone `LowganDev/PlaytubePS4` and apply an XP-TUBE patch.
- [ ] Verify the first main-branch CI runs and retain their logs/artifacts.

**Exit gate:** main builds reproducibly and its CI status is known.

## Stage 1 — Source provenance and migration into XP-TUBE

- Review the pinned upstream revision and complete file inventory.
- Determine which files are original upstream, XP-TUBE modifications, and third-party components.
- Preserve required copyright, GPL-3.0, and third-party license notices.
- Import the reviewed source into XP-TUBE's own repository, with clear provenance and license history.
- Do not remove or misrepresent original authorship.
- Establish an XP-TUBE-owned source tree and build entry point.

**Exit gate:** the required source is present in the XP-TUBE repository, provenance is documented, and a build can be prepared without cloning Playtube.

## Stage 2 — Remove upstream build dependency

- Replace `scripts/prepare-xp-tube.py` patch-on-upstream behavior with native XP-TUBE source.
- Remove Playtube clone/checkout steps from both workflows.
- Build the PS4 package from repository-owned source only.
- Build host unit tests from the same repository-owned source.
- Keep a source manifest, dependency manifest, checksums, and license audit.
- Add CI checks that fail if the workflow fetches Playtube or if release build scripts reference its repository.

**Exit gate:** clean build and unit tests succeed with network access to Playtube blocked.

## Stage 3 — Independent architecture and core systems

- Inventory every inherited application module and dependency.
- Define XP-TUBE-owned interfaces for app lifecycle, state, navigation, settings, networking, content models, storage, and playback.
- Replace inherited implementation modules incrementally with independently maintained XP-TUBE modules.
- Keep third-party libraries only where their licenses and technical role are documented.
- Preserve tested features with regression tests before removing old implementations.

**Exit gate:** the XP-TUBE runtime no longer relies on Playtube application implementation.

## Stage 4 — Original XP-TUBE interface

- Build the complete XP-TUBE visual system and screen layouts.
- Rebuild home, search, video details, channels, library, account, and settings.
- Give settings a completely new information architecture and visual design.
- Use black, silver, and violet; do not copy Playtube layout or interaction patterns.
- Keep 16:9 TV readability and DualShock 4 navigation.
- Use vector icons, no emoji, and lightweight animations.

**Exit gate:** all main screens are XP-TUBE designs and all controls work with the PS4 controller.

## Stage 5 — YouTube services and playback

- Implement and maintain XP-TUBE-owned YouTube integration modules.
- Keep search, suggestions, pagination, channel content, video metadata, and supported account operations working.
- Maintain the internal video playback path.
- Handle network timeouts, stream expiry, retries, and service changes safely.
- Do not bypass DRM or access controls.
- Do not promise universal advertisement removal or guaranteed immunity from anti-bot restrictions.

**Exit gate:** real search and playback regression tests pass, with documented limitations.

## Stage 6 — Security, quality, and performance

- Review account/session storage and credential handling.
- Review TLS, certificate validation, profile isolation, and data deletion.
- Test memory use, long sessions, network recovery, unsupported codecs, and crash recovery.
- Run unit, integration, static, and license checks.
- Verify package metadata, title ID, updater, logs, and data paths.

**Exit gate:** no known critical defects in CI and a reproducible release candidate is available.

## Stage 7 — PS4 validation and release

- Produce a clean PKG from `main`.
- Verify PKG metadata, checksum, source archive, manifest, and licenses.
- Keep the existing working PKG and Playtube installation untouched.
- Provide a clear PS4 test checklist.
- Wait for actual user test feedback before calling the build hardware-validated.

**Exit gate:** the user confirms the release candidate works on PS4 and the independence audit passes.

## Working rules

- Work in coherent, substantial batches; avoid cosmetic-only cycles.
- Inspect current source before changing it.
- Never remove a working feature without a tested replacement.
- Never claim success without checking the actual commit and CI result.
- Do not change project scope or add unrelated features.
- If a technical or licensing blocker appears, explain it and stop that specific unsafe change rather than disguising it.
