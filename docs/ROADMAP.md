# XP-TUBE development roadmap

This roadmap is the working sequence for the independent XP-TUBE client. Keep the Playtube-derived feature set and controller-first experience intact while replacing XP-TUBE identity and improving reliability. Work stays on `feature/xp-tube-independence`; do not merge to `main` without explicit approval.

## Stage 1 — Independence and stability

- Give XP-TUBE its own title ID, application title, data folder, updater endpoint, package naming, runtime log, and boot trace.
- Keep settings and account data scoped to XP-TUBE and the active console profile.
- Disable legacy Playtube settings, search-history, and account-token import. Never read or modify the original Playtube data directory.
- Preserve upstream GPL-3.0 terms, third-party notices, attribution, and a corresponding source archive.
- Run the source-level independence audit and verify the PKG, manifest, SHA-256, and source archive in CI.

**Exit gate:** a successful CI build for the current branch head, with the expected artifact and all integrity/license checks passing.

## Stage 2 — XP-TUBE identity and interface consistency

- Keep the familiar Playtube layout and navigation that the user already tested and likes.
- Use XP-TUBE branding consistently in the app header, settings, updater text, package artwork, and shipped translations.
- Keep the optional YouTube logo choice available; XP-TUBE remains the default.
- Apply the selected theme accent consistently without replacing the user's theme choices or hard-coding YouTube red.
- Keep third-party acknowledgements and legal attribution intact.

**Exit gate:** source audit passes and a clean package builds; no layout or feature is removed just for rebranding.

## Stage 3 — Main, search, channel, and video-page experience

- Review the home feed/rows, loading and retry states, empty states, and focus restoration.
- Review search suggestions, recent searches, filters, pasted video links, pagination, and stale-response handling.
- Review channel identity, videos/Shorts/live/playlists tabs, subscription state, and pagination.
- Review video detail/navigation flows and ensure playlists and channel links continue to open correctly.
- Make only targeted changes where source review demonstrates a clear usability or reliability benefit.

**Exit gate:** regression checks pass for all existing flows; no removal of working functionality.

## Stage 4 — Player and PS4 controller experience

- Review play/pause, seek, previous/next shortcuts, options/menu overlays, focus order, and return navigation.
- Preserve internal playback and the currently working YouTube playback path.
- Improve responsiveness and clarity without introducing heavy animations or breaking controller navigation.
- Keep player progress and focus styling tied to the selected XP-TUBE theme accent.

**Exit gate:** static checks and package build pass; real-device validation remains a separate user test.

## Stage 5 — Release-candidate package and user test

- Build from the pinned upstream commit and exact XP-TUBE branch commit.
- Verify package title ID and filename, package checksum, manifest, source archive, GPL license, and third-party notices.
- Publish the PKG only after all CI checks pass.
- Keep the known-good XP-TUBE package and original Playtube installation/data untouched.
- Ask the user to test the new PKG on PS4 when they are available; do not claim device validation before that test.

**Exit gate:** user confirms the new package works on the PS4. Only then consider the next release milestone or a merge request.

## Working rules

- Complete related work in coherent batches rather than scattering tiny cosmetic commits.
- Do not make speculative UI rewrites; inspect the existing implementation first.
- Do not change or remove tested features without a concrete reason and a regression check.
- Never merge to `main` or overwrite the original Playtube installation/data without explicit approval.
- A successful CI build proves build/package integrity, not real-console runtime compatibility.
