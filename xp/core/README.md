# XP-TUBE native core

This directory contains XP-owned C++ foundations. The migrated application's
file operations, path validation, directory creation, and diagnostic secret
redaction, and log sink management now delegate to XP-native components through compatibility adapters.
The remaining application services are still being migrated in stages.

## Current components

- `xp::core::Error` and `xp::core::Result<T>`: explicit success/failure
  values for module boundaries.
- `xp::core::resolve_inside`: lexical validation and joining of an untrusted
  relative path beneath a trusted root.
- `xp::core::StoragePaths` and `StorageContext`: explicit data/package roots
  and guarded path resolution. The PS4 adapter preserves `/data/XP-TUBE`
  for app data and `/data/pkg` for package output.
- `read_file`, `write_file_atomic`, and `FileSink`: bounded file reads,
  atomic writes, and resumable temporary download output.
- `redact_secrets`: case-insensitive masking of credentials in diagnostic
  strings before they reach log sinks. Its matching rules were adapted from
  the GPL-3.0-licensed migration reference; see `docs/SOURCE-PROVENANCE.md`.
- `Logger`: thread-safe log-level filtering, timestamped output, stderr and
  file sinks, bounded rotation, and redaction before writing. The legacy
  printf-style API remains as a formatting/boot-trace compatibility adapter.

## Filesystem security boundary

The path resolver is a lexical guard, not a filesystem sandbox. It does not
resolve symbolic links and cannot prevent a symlink in a trusted directory
from pointing outside it. File operations reject symlinks at the final file
component where `O_NOFOLLOW` is available, but this is not a complete defense
against hostile parent directories, hard links, or concurrent replacement.
Callers must use trusted application-owned directories.

## Build and test

From the repository root:

```sh
cmake -S xp -B build/xp-core -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build/xp-core --parallel 2
ctest --test-dir build/xp-core --output-on-failure
```

The PS4 build workflow currently packages the compatibility application from
the vendored migration source. Passing these native-core tests does not imply
that every service has been independently rewritten.
