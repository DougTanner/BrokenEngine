<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:53:31.830Z","dependsOn":[]} -->
# Share One Path-to-UTF-8 Helper in Common

## Context

Found by a deep analysis of `Engine/Source/Graphics` (direct files only) and
verified independently. Pre-existing at commit `401e4b75`; line numbers are
from that commit, so locate by symbol if they moved.

Three file-local functions convert a `std::filesystem::path` to a UTF-8
`std::string` with the same body: take `u8string()` and build a `std::string`
from its pointer and size.

- `PathToString`, `Engine/Source/Graphics/Screenshot.cpp:20-24`, called at
  `:188`, `:437`, `:464`, and `:494`.
- `PathToUtf8`, `Engine/Source/Agent/AgentCommandsServerGeneric.cpp:19-23`,
  called at `:138`, `:140`, and `:164`.
- `PathToUtf8`, `DataPacker/Source/DiagnosticReporter.cpp:9-13`, called at
  `:65`.

They differ only in the local's name and `c_str()` versus `data()`. `Common`
has no shared owner: a search of `Common/**/*.h` for
`Utf8|WideTo|ToNarrow|u8string` finds nothing. The copies sit in the client,
the server, and DataPacker, and all three programs include `Common.h`. The
encoding the agent harness reads back from capture results and from server
save results is therefore defined in two engine files under two names.

Unmet criterion: one path-to-UTF-8 definition under one name. Authority: root
`AGENTS.md` `## Directives` (DRY; one term per concept; extract helpers only
for current duplication — three current sites);
`.agents/references/cpp-conventions.md` code placement (Common owns code more
than one program needs).

Cost recorded by the verification: the edit touches a header `Common.h`
aggregates, so every precompiled header rebuilds, and the change spans three
programs for a three-line body.

## Design

Recommended: add one `common::PathToUtf8` beside the other string helpers
(`Common/StringUtils.h`, defined where that header's functions are defined),
with the existing body unchanged; remove the three file-local definitions;
rename the four `PathToString` call sites in `Screenshot.cpp` to the shared
name. Behavior is moved, not changed.

## Critical files

- `Common/StringUtils.h` and its implementation file
- `Engine/Source/Graphics/Screenshot.cpp`
- `Engine/Source/Agent/AgentCommandsServerGeneric.cpp`
- `DataPacker/Source/DiagnosticReporter.cpp`

## In scope

- One new `common::PathToUtf8` declaration and definition.
- The file-local `PathToString` in `Screenshot.cpp` and its four call sites.
- The file-local `PathToUtf8` in `AgentCommandsServerGeneric.cpp` and in
  `DiagnosticReporter.cpp`, and their call sites as far as the qualified name
  requires.

## Out of scope

- Any change to the produced string.
- Other path or string conversions, including `PathToCppVariable`.
- The rest of `Screenshot.cpp`.

## Risk tier

Tier 3 — invariant/integration. Trigger: "a change spanning independently
owned subsystems" (`.agents/references/risk-tiers.md`): it adds a `Common`
function and changes callers in engine Graphics, engine Agent, and DataPacker,
and the `Common.h`-aggregated header edit rebuilds every precompiled header.
No determinism/CRC, wire, serialization, `.pack`, or save/replay byte
changes.

## Acceptance criteria

- The tracked tree holds one definition that converts a path to a UTF-8
  `std::string`, named `PathToUtf8` in `common::`, and no file-local
  `PathToString` or `PathToUtf8`.
- Every project that compiles `Common` builds, including the
  BrokenEngineSandbox client and server, ModelViewer, and DataPacker.

## Coordination

No Plan below depends on this one, and it depends on none. Whichever lands
second re-derives its regions by symbol from the landed file.

- `Documents/Plans/Engine/ShareScreenshotAndDumpHelpers.md` edits
  `Screenshot.cpp` too, in the encoders and catch blocks that hold the
  `PathToString` call sites.

## Notes

- `Projects/Coretide` was not searched: the private game's source was not
  available to the authoring session. A session that can read the primary
  checkout reports a further copy there as a residual instead of editing it.
