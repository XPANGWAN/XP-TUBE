# XP-TUBE native migration status

This file records implementation ownership separately from package branding and
from the temporary migration build. It must be updated only when source and CI
evidence support a status change.

## Verified repository state

| Area | Current state | Evidence / remaining boundary |
|---|---|---|
| XP Core | XP-owned native library exists | `xp/core` contains result/error handling, guarded paths, storage, file operations, logging, text and URL utilities with unit tests. |
| XP Network | XP-owned native library exists | `xp/net` contains HTTP contracts, libcurl transport and certificate inspection. The host test suite includes contract and local HTTPS streaming coverage. |
| PS4 package build | Not yet confirmed on latest commit | Run 324 is queued; the previous package run is still compiling dependencies. No PKG artifact is confirmed here. |
| Application runtime | Partially migrated | Native core/network components are used through compatibility adapters, but the complete application still builds from the vendored migration tree. |
| XP Platform foundation | Initial native event contract added | `xp/platform` now defines backend-neutral lifecycle, controller, display and audio-focus events plus a bounded single-thread event queue and host tests. No PS4 SDK adapter, controller polling, display or audio backend is implemented yet. |\n| PS4 platform adapter | Not independently replaced | Existing PS4 platform implementation remains in the migration source; it must be bridged to the XP event contract and validated on hardware. |
| UI and application screens | Not independently replaced | Existing UI/screens remain migration-derived; branding changes are not a rewrite. |
| YouTube/service layer | Not independently replaced | Existing service behavior remains migration-derived. Undocumented service endpoints remain a maintenance risk. |
| Playback engine | Not independently replaced | libmpv/FFmpeg are third-party dependencies; the application-level playback integration remains migration-derived. |
| Build source | Not independent yet | `scripts/prepare-xp-tube.py` still patches `vendor/playtube-migration`; removing upstream network downloads alone does not remove this source dependency. |
| Licensing | Preserved | GPL-3.0 and third-party notices must remain in source and corresponding release source archives. |

## Recent verification

- Host unit-test workflow run `46120147153` completed successfully on
  2026-11-28, including the native core test build and test execution.
- PS4 package workflow run `46120147271` was still building locally pinned
  dependencies at the last status check.
- Newer package workflow run `46120291626` was queued and had not started jobs
  at the last status check.
- A successful host test run is not evidence of a PS4 PKG or real-console
  runtime success.

## Next migration sequence

1. Keep the current native core and network contracts stable; add regression
   coverage before changing their public APIs.
2. Audit the current PS4 platform adapter and define XP-owned interfaces for
   controller input, application lifecycle, display, audio and system dialogs.
3. Implement the XP platform adapter behind those interfaces while preserving
   the working PS4 behavior.
4. Migrate the application/service layer in small testable slices.
5. Remove the patch-on-vendor build only after the replacement source tree
   builds the same release package and passes feature regression tests.
6. Remove the migration reference only after the independent build, license
   audit and real PS4 validation are complete.

## Independence gate

Do not mark XP-TUBE fully independent until the release build uses only
XP-TUBE-maintained application source, no Playtube application modules or
services are required at runtime, the provenance/license audit passes, and the
resulting PKG is confirmed on PS4 hardware.
