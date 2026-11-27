# XP-TUBE

**XP-TUBE** is an independent YouTube client for PS4 running GoldHEN.

The technical baseline is [PlaytubePS4](https://github.com/LowganDev/PlaytubePS4), pinned to upstream commit `7372c068704f568bf04e1f8b41a04a2149b2e048` (Playtube 1.2.0). The upstream project remains the work of its original author. GPL-3.0 terms, copyright notices, attribution, and corresponding-source obligations are preserved.

## Non-negotiable project rules

- XP-TUBE has its own package identity, title ID, data directory, logs, and update repository.
- Never overwrite, uninstall, import data from, or modify the user's Playtube installation.
- Keep the known-good XP-TUBE test package until a replacement has been installed and tested on the PS4.
- Do not merge into `main` without explicit user approval.
- Do not claim complete YouTube ad removal. SponsorBlock is not a universal ad blocker.
- Treat YouTube's undocumented InnerTube interface and anti-bot/IP restrictions as reliability risks.
- Do not claim a package works until its build evidence is verified and it is tested on real hardware.

## Current engineering state

- [x] Pin the upstream baseline to a reproducible commit.
- [x] Build and test the initial Playtube-derived XP-TUBE baseline on PS4.
- [x] Create the isolated `feature/xp-tube-independence` branch.
- [x] Assign XP-TUBE title ID `XPTB00000` and package metadata.
- [x] Isolate application data under `/data/XP-TUBE`.
- [x] Keep account files scoped per PS4 user.
- [x] Point the updater to `XPANGWAN/XP-TUBE`.
- [x] Disable the legacy Playtube settings/account migration path.
- [x] Rename runtime and early-boot logs to XP-TUBE-specific paths.
- [x] Add CI regression checks for identity, storage isolation, updater, logs, and visible translation branding.
- [x] Produce a verified PKG and matching GPL corresponding-source archive on the independence branch.
- [ ] Install and test the latest independence-branch PKG on PS4 while retaining the known-good package and Playtube.
- [ ] Complete the remaining source/UI identity review.
- [ ] Begin the independent XP-TUBE visual redesign after isolation and stability checks.

## Latest verified package

The latest package was produced by GitHub Actions run [45991023580](https://github.com/XPANGWAN/XP-TUBE/actions/runs/45991023580) from commit `5ef5983805af270a166f89d0c1385e95ed4e5db5` on `feature/xp-tube-independence`.

- Package: `IV0001-XPTB00000_00-XPTB000000100000.pkg`
- SHA-256: `124336d0b5c42a10569f10c614ca671671d95c03b9bf55e40f2f2184ca4c3564`
- Data directory: `/data/XP-TUBE`
- Package size: `53,215,232` bytes
- Runtime log: `/data/XP-TUBE/xp-tube.log`
- Early boot trace: `/data/XP-TUBE/boot.txt`

This package passed build and checksum verification. Its installation and upgrade behavior still require a real PS4 test.

## Build and audit

The workflow `.github/workflows/xp-tube-build.yml` fetches the pinned upstream source, applies the documented XP-TUBE patch, runs `scripts/audit-xp-tube.py`, builds the PS4 PKG, and uploads the package together with its manifest, checksum, and corresponding source archive.

See [the independence audit](docs/INDEPENDENCE-AUDIT.md) for the checks and their limits.
