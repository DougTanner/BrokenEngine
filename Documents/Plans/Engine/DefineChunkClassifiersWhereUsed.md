<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:41:27.142Z","dependsOn":[]} -->
# Define the constexpr chunk classifiers where they are used

## Context

Found by the architecture phase of an analysis of `Engine/Source/File` (the
directory, not its subdirectories), verified against commit `7dae9a27`. It is
pre-existing debt, not an unmet acceptance criterion of any change.

`Engine/Source/File/FileManager.h:137,140` declare `IsEagerChunk` and
`IsServerChunk` as `constexpr` with no body. Their only definitions are in
`Engine/Source/File/PackChunks.cpp:104-113`.

- `Engine/Source/Profile/ProfileScreens.cpp:118` calls `IsEagerChunk` from
  another translation unit, which includes `File/PackChunks.h` (line 3) and
  so sees only the bodiless declaration.
- `IsServerChunk` is called only at `PackChunks.cpp:302,446`, both under
  `BT_SERVER`.

A `constexpr` function is implicitly inline and must be defined in every
translation unit that uses it; using it without a definition there is
ill-formed, no diagnostic required (C++23 `[basic.def.odr]`, externally
verified during the analysis). The build works today only because the
compiler happens to emit the definition from `PackChunks.cpp`; both projects
build with `/GL` outside Debug. Whether MSVC Release emits that definition in
every case is unresolved and is not needed for this correction.

Unmet criterion: each `constexpr` function is defined in every translation
unit that uses it.

## Design

Recommended:

1. Move the `IsEagerChunk` body to its declaration at `FileManager.h:137` and
   delete the definition at `PackChunks.cpp:104-107`. `FileManager.h:3`
   already includes `Data/DataTypes.h`, so the body compiles there.
2. Make `IsServerChunk` a file-local `static constexpr` function in
   `PackChunks.cpp` inside `#if defined(BT_SERVER)`, placed before its first
   use, keeping its comment. Delete the declaration and its comment at
   `FileManager.h:138-140`. Rationale: it has no caller outside
   `PackChunks.cpp` and none on the client.
3. Correct the include comment at `Engine/Source/File/PackChunks.h:3`, which
   lists "IsEagerChunk/IsServerChunk decls".

## Critical files

- `Engine/Source/File/FileManager.h`
- `Engine/Source/File/PackChunks.cpp`
- `Engine/Source/File/PackChunks.h`

## In scope

- The `IsEagerChunk` and `IsServerChunk` declarations (`FileManager.h`) and
  definitions (`PackChunks.cpp`).
- The comment at `PackChunks.h:3`.

## Out of scope

- The classification either function returns.
- Any other declaration in `FileManager.h`, including the client-only texture
  members of `LazyChunk`.
- `Engine/Source/Profile/ProfileScreens.cpp`: its call stays as written.

## Risk tier

Tier 2 — scoped behavior. Trigger: it changes declarations in `FileManager.h`,
a header `Engine.h` aggregates, and removes `IsServerChunk` from that public
surface, more than Tier 1's "no public signature exposure" allows. No runtime
behavior, pack format, or affinity of a caller changes. The header edit
rebuilds both precompiled headers once.

## Acceptance criteria

- `IsEagerChunk` has its body in `FileManager.h` and no other definition;
  `IsServerChunk` is named only inside `PackChunks.cpp` under `BT_SERVER`.
- The BrokenEngineSandbox client and server and ModelViewer build without
  errors or new warnings, in Debug and Release.

## Coordination

No mandatory nondirectional coordination.
