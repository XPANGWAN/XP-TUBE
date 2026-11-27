# XP-TUBE native core

This directory contains XP-owned C++ foundations. The migrated application's
file operations, path validation, and directory creation now delegate to XP
native storage components through compatibility adapters. The remaining
application services are still being migrated in stages.

## Current components

- \`xp::core::Error\` and \`xp::core::Result<T>\`: explicit success/failure
  values for module boundaries. These types do not replace error handling in
  the migrated application until adapters are implemented.
- \`xp::core::resolve_inside\`: lexical validation and joining of an untrusted
  relative path beneath a trusted root. It rejects absolute paths, traversal
  segments, empty segments, backslashes and embedded NUL bytes.
- \`xp::core::StoragePaths\`: keeps the application data root and package-output
  root explicit, resolves data files beneath the former, and limits package
  output to a single filename beneath the latter. The platform selects the
  package root (for example, \`/data/pkg\` on PS4); this class does not create
  directories or perform filesystem I/O.
- \`read_file\`: bounded reads of regular files, with explicit missing-file and
  I/O errors. Symbolic links are not followed where \`O_NOFOLLOW\` is supported.
- \`write_file_atomic\`: writes to a unique same-directory temporary file,
  flushes it, then renames it over the destination. Temporary files use mode
  0600. Parent-directory fsync is best-effort because platform filesystems may
  not support it.
- \`FileSink\`: streams download data into a private \`.part\` file, supports
  resuming, restarting, keeping a paused partial file, and atomically committing
  the completed file. An uncommitted part is removed when the sink is destroyed
  unless \`keep()\` was called.

## Filesystem security boundary

The path resolver is a lexical guard, not a filesystem sandbox. It does not
resolve symbolic links and cannot prevent a symlink in a trusted directory
from pointing outside it. File operations reject symlinks at the final file
component where \`O_NOFOLLOW\` is available, but this is not a complete defense
against hostile parent directories, hard links, or concurrent replacement.
Callers must use trusted application-owned directories. Platform-specific
directory-handle enforcement remains a separate integration task.

## Build and test

From the repository root:

\`\`\`sh
cmake -S xp -B build/xp-core -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build/xp-core --parallel 2
ctest --test-dir build/xp-core --output-on-failure
\`\`\`

The PS4 build workflow currently packages the compatibility application from
the vendored migration source. Passing these native-core tests does not imply
that the PS4 package uses these components yet.
