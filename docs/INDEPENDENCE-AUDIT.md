# XP-TUBE independence audit

This audit is run against the pinned upstream source after `scripts/prepare-xp-tube.py`
has applied the XP-TUBE patch and before the PS4 package is built.

## Isolation guarantees checked by CI

- Release title, package title ID, and application data folder are XP-TUBE-specific.
- Settings and account files are resolved through the XP-TUBE data root; account files remain scoped per PS4 user.
- The startup path does not call Playtube's legacy settings/account importer and does not read `/data/Playtube`.
- The updater checks XPANGWAN/XP-TUBE releases and uses XP-TUBE package naming.
- The runtime log is named `xp-tube.log` under `/data/XP-TUBE`; the earliest boot trace derives its directory from `appFolder()` and writes `/data/XP-TUBE/boot.txt` with XP-TUBE-labelled messages.
- Every shipped translation JSON file is parsed during CI and checked for leftover visible `Playtube` branding; upstream legal notices and attribution outside translations are not rewritten by this check.

## Important boundary

The upstream importer implementation and upstream copyright/license material are retained in the corresponding source archive where required, but XP-TUBE does not invoke the importer. The original Playtube installation and its data are not modified or migrated.

This is a source-level regression guard, not a substitute for testing installation and upgrades on a real PS4. The user must keep the known-good XP-TUBE PKG and Playtube installation until a newly built PKG is tested.
