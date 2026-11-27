# XP-TUBE

**XP-TUBE** is an independent PS4 YouTube-client project for GoldHEN.

The engineering baseline is [PlaytubePS4](https://github.com/LowganDev/PlaytubePS4), currently pinned for baseline verification to upstream commit `7372c068704f568bf04e1f8b41a04a2149b2e048` (Playtube 1.2.0). The upstream project remains the work of its original author; its GPL-3.0 license and all third-party notices must be preserved in any distributed derivative.

## Project principles

- Keep XP-TUBE independently identifiable and installable; never overwrite Playtube or its data.
- Verify an unchanged upstream development build before making product changes.
- Preserve upstream copyright, license texts, attribution, and corresponding-source obligations.
- Do not claim that all YouTube ads can be removed. SponsorBlock handles community-submitted segments; it is not proof that every server-served ad is blocked.
- Treat YouTube's undocumented InnerTube interface and anti-bot/IP restrictions as reliability risks, not problems that can be promised away.
- Never claim a PKG works until GitHub Actions produces it and it is tested on the user's PS4.

## Current status

- [x] Create an isolated research branch.
- [x] Pin the Playtube 1.2.0 baseline commit.
- [x] Add a reproducible GitHub Actions baseline-build workflow.
- [ ] Complete the first PS4 PKG build.
- [ ] Test the development PKG on a PS4 without replacing Playtube.
- [ ] Record compatibility, playback, account, controller, and stability results.
- [ ] Design XP-TUBE's independent title ID, storage paths, branding, and update channel.

## Build baseline

The workflow `.github/workflows/playtube-baseline.yml` clones the pinned upstream revision and invokes its own PS4 build script with the upstream development-package option. The expected output is a **Playtube development baseline PKG**, not an XP-TUBE release. This deliberate first step checks the toolchain and packaging path before we fork or modify application code.

See [the baseline review](docs/BASELINE-REVIEW.md) for architecture, risks, and the staged implementation plan.
