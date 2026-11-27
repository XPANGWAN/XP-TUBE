# XP-TUBE native core

This directory contains XP-owned C++ foundations. It is currently built and
tested as a standalone CMake subproject; it is **not yet linked into the PS4
application runtime**.

## Current components

- `xp::core::Error` and `xp::core::Result<T>`: explicit success/failure
  values for module boundaries. These types do not replace error handling in
  the migrated application until adapters are implemented.
- `xp::core::resolve_inside`: lexical validation and joining of an untrusted
  relative path beneath a trusted root. It rejects absolute paths, traversal
  segments, empty segments, backslashes and embedded NUL bytes.

## Path resolver security boundary

The resolver is a lexical guard, not a filesystem sandbox. It does not resolve
symbolic links and cannot prevent a symlink inside the root from pointing
outside it. Callers must use a trusted application data root and must not treat
this helper alone as protection against hostile filesystem layouts. A future
platform storage adapter must enforce filesystem-level constraints where the
platform supports them.

## Build and test

From the repository root:

```sh
cmake -S xp -B build/xp-core -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build/xp-core --parallel 2
ctest --test-dir build/xp-core --output-on-failure
```

The PS4 build workflow currently packages the compatibility application from
the vendored migration source. Passing these native-core tests does not imply
that the PS4 package uses these components yet.
