# XP-TUBE native migration status

This file records implementation ownership separately from package branding
and from the temporary migration build. Status changes require source and CI
evidence.

## Verified repository state

| Area | Current state | Evidence / remaining boundary |
|---|---|---|
| XP Core | XP-owned native library integrated through transitional adapters | `xp/core` contains result/error handling, guarded paths, storage, file operations, logging, text and URL utilities with unit tests. Storage paths, file operations, and logging are already consumed by the application; application settings now use XP-owned bounded configuration reads, atomic writes, and damaged-file quarantine. PS4 platform selection is passed through an XP-owned capability macro. |
| XP Network | XP-owned implementation with decoupled build inputs | `xp/net` contains HTTP contracts, libcurl transport and certificate inspection. Platform library targets and PS4 selection enter through generic XP-TUBE CMake inputs; the migration project's legacy target names are confined to its transitional adapter. Host contract and local HTTPS streaming coverage remain in CI. |
| XP Platform runtime | Integrated transitional event path | The app routes the existing SDL event stream through XP Runtime/EventQueue; SDL remains initialized and polled by one existing owner. Screen events are converted back during migration. |
| PS4 platform backend | Still inherited | OpenOrbis module loading, remote/system services, display/presentation, and SDL integration remain in the migration source; no independent XP PS4 backend yet. |
| UI and application screens | Not independently replaced | Existing UI/screens remain migration-derived; branding changes are not a rewrite. |
| YouTube/service layer | Not independently replaced | Existing service behavior remains migration-derived. Undocumented service endpoints remain a maintenance risk. |
| Playback engine | Not independently replaced | libmpv/FFmpeg are third-party dependencies; application-level playback integration remains migration-derived. |
| Build source | Not independent yet | `scripts/prepare-xp-tube.py` still patches the repository-vendored migration tree. |
| Licensing | Preserved | GPL-3.0 and third-party notices remain required in source and corresponding release archives. |

## Current migration boundary

The XP runtime now receives controller, quit, text-input, and text-delete
events from the single existing SDL polling stream. It does not initialize SDL
again and does not perform a second controller poll. The app temporarily maps
XP events back into the inherited UI event type so that working screen
interactions remain intact while screens are migrated.

The XP Network module no longer references migration-project target names or its legacy PS4 compile macro; the temporary parent build supplies curl, crypto, and platform selection through generic XP-TUBE inputs. This is build-boundary decoupling, not a claim that the application source is independent.

The PS4 build script consumes only locally built dependency packages and
validates the XP-TUBE package filename. This removes the fallback download of
the old Playtube dependency release; it does not remove the inherited
application source or the source patching layer.

## Next sequence

1. Implement and validate an XP-owned OpenOrbis platform backend behind the
   current XP event contract.
2. Continue migrating settings/configuration and application services onto XP-owned interfaces; then migrate lifecycle, UI, and playback in testable slices.
3. Replace the source patching workflow with repository-owned maintained
   source and remove the upstream build dependency.
4. Run full feature regression, licensing, security, and PS4 hardware checks.
