<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:43:33.630Z","dependsOn":[]} -->
# Make the game DebugRender dispatch and its AGENTS.md invariant agree

## Context

Found by the architecture phase of a deep analysis of
`Projects/BrokenEngineSandbox/Source/Frame` (direct files only), and verified
independently. Pre-existing at commit `022915b6`; line numbers are from that
commit, except the `Frame.cpp` and collection-header ones, which are from the
commit that last edited this Plan, so locate by symbol if they moved. `BES/`
abbreviates `Projects/BrokenEngineSandbox/Source/`.

`BES/Frame/AGENTS.md` `## Invariants` (lines 21-22) says: "Every game
collection pair registers in the tuples in `FrameCollections.h`; no collection
has explicit per-collection dispatch."

`FrameInterpolate::DebugRender` (`BES/Frame/Frame.cpp:637-649`) dispatches by
name: inside its loop over the active coordinates it calls
`FightersInterpolate::DebugRender`, `OmnisInterpolate::DebugRender`, and
`BattleshipsInterpolate::DebugRender` (`:644-646`). Every other render entry
point in the same file walks the tuple through an engine helper
(`ForEachBeginRender`, `ForEachInterpolateRender`, `ForEachEndRender`,
`ForEachRequestTextures`; `Engine/Source/Frame/FrameUtils.h:174-220`).
`FrameUtils.h` holds no walker for `DebugRender`.

Only those three collections declare `DebugRender`
(`BES/Frame/Collections/Fighters/Fighters.h:171`, `Omnis/Omnis.h:203`,
`Battleships/Battleships.h:163`); Blasters, Missiles, and Wrecks do not. The
one caller of `FrameInterpolate::DebugRender` is
`Engine/Source/Graphics/Render/MainUniforms.cpp:299`, under `kbDebugRender`.

Unmet criterion: the code and the scoped AGENTS.md invariant agree. Today a
new collection with debug geometry is not drawn until someone adds a line by
hand, which the sentence says cannot happen.

## Design

The code changes so that `FrameInterpolate::DebugRender` walks the collection
tuple; the sentence in `BES/Frame/AGENTS.md` stays as written, with no
exception.

This is the authoring agent's record of the user's choice. The authoring
session asked the user to choose among three options, and the user's reply was
"Option 2". Option 2 was: change the code — add an engine helper that walks the
collection list and calls `DebugRender` on each collection that has one, so the
`Frame/AGENTS.md` sentence stays true with no exception. The other two options
were to narrow the `AGENTS.md` sentence to exempt debug render, and to drop
this Plan.

- Add one `ForEachDebugRender` to `Engine/Source/Frame/FrameUtils.h`, beside
  `ForEachInterpolateRender` (`:174-184`) and in its form: a fold over
  `TypeList<TS...>` that calls `TS::DebugRender` for a type only when a
  `requires` test finds that call, and does nothing for a type without one.
- The helper takes one cell's `const game::FrameInterpolate&` and that cell's
  `engine::GridCoord`, and passes both to each `TS::DebugRender`. Those are the
  two parameters all three existing declarations take:
  `static void DebugRender(const FrameInterpolate& __restrict rFrameInterpolate, engine::GridCoord coordinate)`.
- In `FrameInterpolate::DebugRender`, keep the loop over the active
  coordinates and its `rRenderInterpolates` lookup. Replace the three named
  calls (`Frame.cpp:644-646`) with one `engine::ForEachDebugRender` call over
  `GameInterpolateTypes {}`, passing `it->second` and `rCoordinate`.
- The drawn result does not change. `FrameInterpolate::DebugRender` compiles
  under `BT_CLIENT` only, and the client tuple order is Blasters, Missiles,
  Fighters, Omnis, Battleships, Wrecks (`BES/Frame/FrameCollections.h:21`), so
  the walk skips the three types without `DebugRender` and calls the same three
  in the same order as today.

## Critical files

- `Engine/Source/Frame/FrameUtils.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`

## In scope

- A new `ForEachDebugRender` in `FrameUtils.h`.
- The three named `DebugRender` calls inside `FrameInterpolate::DebugRender`
  (`Frame.cpp`).

## Out of scope

- `BES/Frame/AGENTS.md`: the sentence stays as written.
- The three collections' own `DebugRender` bodies and declarations.
- The signature of `FrameInterpolate::DebugRender`, the engine's own debug
  rendering, and the caller in `MainUniforms.cpp`.
- Any other `ForEach*` helper in `FrameUtils.h`.

## Risk tier

Tier 3 — invariant/integration. Trigger: "a change spanning independently
owned subsystems" (`.agents/references/risk-tiers.md`). The change adds a
helper to engine Frame (`Engine/Source/Frame/`) and changes its one caller in
game Frame (`BES/Frame/`); each has its own owning `AGENTS.md`, and the helper
is a new engine name every game project compiles. Tier 2 does not fit: it
covers "one subsystem's runtime or tool behavior" and excludes "independently
owned cross-subsystem integration".

No other Tier 3 surface is reached: no determinism/CRC, wire/protocol,
serialization or data layout, save/replay, threading, trust boundary, or
build/bootstrap coordination. The changed call path is client-only and
`kbDebugRender`-only rendering, with no server reach. `FrameUtils.h` reaches
the game precompiled headers through `Frame/FrameBase.h`, so client and server
both rebuild.

## Acceptance criteria

- `FrameInterpolate::DebugRender` names no collection type: its body holds no
  `FightersInterpolate::DebugRender`, `OmnisInterpolate::DebugRender`, or
  `BattleshipsInterpolate::DebugRender` call, and dispatches only through
  `engine::ForEachDebugRender`.
- The BrokenEngineSandbox client and server build through `/compile`.
- Through `/agent-harness`, a client with debug rendering on still draws the
  debug geometry of Fighters, Omnis, and Battleships. `kbDebugRender` is true
  only in the Debug configuration (`Engine/Source/EngineSwitches.h`). The
  supplied evidence records no harness setup.
- `BES/Frame/AGENTS.md` is unchanged.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- The private game's source was not available to the authoring session. The
  executing session confirms, where that source is available, that the new
  helper compiles against it, and reports it as unconfirmed otherwise.
