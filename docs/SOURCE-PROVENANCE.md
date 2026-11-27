# Source provenance and migration boundary

## Imported baseline

- Original repository: https://github.com/LowganDev/PlaytubePS4
- Imported revision: 7372c068704f568bf04e1f8b41a04a2149b2e048
- Imported path: vendor/playtube-migration/
- License at imported revision: GNU General Public License v3.0
- Import method: Git subtree with upstream commit ancestry preserved.
- Import date: 2026-11-27
- Imported regular files: 554
- SHA-256 inventory: docs/SOURCE-FILE-CHECKSUMS.txt

The directory vendor/playtube-migration/ is a temporary migration reference. It is
not an XP-TUBE-authored implementation. Original copyright, GPL,
and third-party notices must remain intact wherever required.

## Required next steps

1. Review source provenance and the complete third-party dependency list.
2. Move or rewrite required build and runtime components into
   XP-TUBE-owned paths while preserving source history and notices.
3. Replace the patch-on-upstream build with a native XP-TUBE build.
4. Remove this migration directory only after all required components
   have been independently replaced and license review confirms
   removal is appropriate.
