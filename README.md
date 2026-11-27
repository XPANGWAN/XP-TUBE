# XP-TUBE

**XP-TUBE** is an independently developed YouTube client for PS4 running GoldHEN.

## Project objective

The final product must not depend on Playtube source code, its repository, its build scripts, or its runtime implementation. The current codebase is still a Playtube-derived migration baseline; complete independence has **not** yet been achieved.

Third-party libraries may be used under their respective licenses. GPL-3.0 obligations, original copyright notices, and third-party notices must be preserved wherever legally required. Removing Playtube branding from the product UI does not authorize removing required attribution or license material.

## Repository and branch policy

- Active development branch: `main`.
- Backup branch: `feature/xp-tube-independence` (kept at the pre-main migration baseline).
- Research branch: `research/playtube-baseline` (preserved as historical reference).
- Do not delete backup/research branches.
- Do not force-push or rewrite branch history.
- Changes are made on `main`; use small, reviewable commits for infrastructure changes and coherent batches for product work.
- Keep a known-good PKG and all build evidence until a replacement is tested on the real PS4.

## Non-negotiable product requirements

- XP-TUBE owns its application identity, package metadata, data directory, logs, updater, UI, settings, navigation, and application architecture.
- Never overwrite, uninstall, import data from, or modify the user's Playtube installation.
- Preserve working search, channels, videos, library, account capabilities, and internal playback while replacing implementation.
- No Playtube repository fetch or Playtube source dependency in the final build.
- No Playtube branding in the user-facing product.
- Keep legally required source attribution, GPL notices, and third-party licenses.
- Do not claim complete YouTube ad removal. SponsorBlock is not a universal ad blocker.
- Treat YouTube's undocumented InnerTube interface and anti-bot/IP restrictions as reliability risks.
- Do not claim a package works until build evidence is verified and it is tested on real hardware.

## Current engineering state

- [x] XP-TUBE package identity: `XPTB00000`.
- [x] XP-TUBE data directory: `/data/XP-TUBE`.
- [x] XP-TUBE-specific updater, runtime log, and boot trace.
- [x] Playtube settings/account migration disabled.
- [x] Source-level identity and regression audit exists.
- [x] Existing baseline package and corresponding-source archive were built and checksum-verified on the backup branch.
- [x] `main` advanced safely to the latest pre-migration baseline commit `e279d0405124b7cdab367547254e217a7023ec70`.
- [x] Package-build and host-unit-test workflows now trigger on `main`.
- [ ] Remove all build-time fetching of the Playtube repository.
- [ ] Bring the reviewed, license-compliant source into XP-TUBE's own repository and establish source provenance.
- [ ] Replace the patch-on-upstream build with a native XP-TUBE build.
- [ ] Inventory and rewrite all Playtube-derived application components required for complete independence.
- [ ] Rebuild the UI and settings as original XP-TUBE experiences.
- [ ] Verify no Playtube source/runtime/build dependency remains.
- [ ] Build and test the independent PKG on a real PS4.

## Temporary migration boundary

The current workflows still clone the pinned Playtube repository and apply `scripts/prepare-xp-tube.py`. This is a temporary migration mechanism only. It is not the final architecture and must not be described as full independence.

The previous verified package is historical evidence only. It does not prove that the current `main` build is independent or that the package was tested on the current PS4 installation.

## Design direction

- Cinematic dark interface, black and metallic silver with violet as the supporting accent.
- Original XP-TUBE visual language, not a Playtube reskin.
- 16:9 TV-first layouts and readable typography.
- Controller-first navigation for DualShock 4.
- Vector icons, no emoji, smooth low-overhead transitions.
- Preserve all working core functions while replacing their implementation.

## Build and audit

The workflows under `.github/workflows/` currently provide a temporary pinned-source build and host unit-test path. The next independence milestone is to vendor/import the reviewed source with required license history and notices, then change both workflows to build only from XP-TUBE repository contents.

See `docs/INDEPENDENCE-AUDIT.md` and `docs/ROADMAP.md`.
