<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:42:27.264Z","dependsOn":[]} -->
# Drop WriteRow's iCount parameter

## Context

Found by a deep analysis of `Engine/Source/Agent` (in-function phase),
verified by code reading against commit `4fa6d2d7`; nothing was built. Line
numbers are from that commit.

In `Engine/Source/Agent/Commands/FrameEdit.h`, `WriteRow` (`:278`) takes
`int64_t iCount` beside `POST_RENDER& rPostRender`, while its mirror `ReadRow`
reads `rPostRender.iCount` itself (`:420`). The one `WriteRow` caller,
`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp:109`,
passes `std::get<INDICES>(postRenders).iCount` and the same
`std::get<INDICES>(postRenders)` as `rPostRender`, so the parameter restates
an argument. The `ReadRow` caller (`:254`) passes elements of the same tuple,
so every `POST_RENDER` that reaches `WriteRow` has `iCount`.

Unmet criterion: `WriteRow` takes no count parameter that its `rPostRender`
argument already supplies.

## Design

1. `FrameEdit.h`: remove `int64_t iCount` from `WriteRow`'s parameter list
   (`:278`) and read the count from `rPostRender.iCount` as `ReadRow` does,
   for the uses at `:291`, `:294`, and `:296`.
2. `ServerFrameEdit.cpp:109`: drop the `.iCount` argument.

Risk tier: Tier 2 (the author's classification). Trigger: the signature of an
engine template that game code calls changes, which Tier 1's "no public
signature exposure" excludes. No runtime behavior changes: the same count is
read from the same object. `edit_frame` is a server-only developer command,
so there is no determinism, wire, or serialization exposure.

## Critical files

- `Engine/Source/Agent/Commands/FrameEdit.h`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`

## In scope

- `WriteRow`: its parameter list and its three uses of `iCount`.
- The `engine::WriteRow` call in `ServerFrameEdit.cpp`.

## Out of scope

- A shared helper for the index read that `WriteRow` (`:289-297`) and
  `ReadRow` (`:421-429`) both hold: the pair is a deliberate mirror and the
  analysis did not accept the helper.
- `ReadRow`, `WriteKeys`, `ReadKeys`, and the `ReadElement`/`WriteElement`
  names.
- Error text.

## Acceptance criteria

1. Code read: `WriteRow` has no `iCount` parameter and its one caller passes
   none.
2. `/compile` builds the BrokenEngineSandbox server and client.
