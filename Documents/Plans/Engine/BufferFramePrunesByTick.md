<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:39:30.059Z","dependsOn":[]} -->
# BufferFrame prunes rings by tick instead of a per-tick set

## Context

`SBF` is `Engine/Source/Network/Server/ServerBufferedFrames.cpp`. Verified
against commit `4cf9d607`.

`ServerBufferedFrames::BufferFrame` (`SBF:30-82`) builds a
`std::unordered_set<GridCoord>` of the supplied coordinates every tick
(`SBF:35-36`, `SBF:41`) only to erase, at the end (`SBF:78-81`), the delta
rings of coordinates not supplied this tick.

The set is not needed to tell the two apart:

- The loop pushes one entry with `iTick` onto every supplied coordinate's
  ring (`SBF:69-74`), so each supplied ring is non-empty and its
  `back().iTick` equals `iTick`.
- A ring not supplied this tick is non-empty (a ring is created only by that
  push) and its newest entry carries an earlier tick.
- Every tick reset clears the rings first: `ClearBufferedFrames` runs from
  `ServerSessionRuntime::ResetTransportForLoad`
  (`Engine/Source/Network/Server/ServerSessionRuntime.cpp:309`), reached from
  the save-load reset (`Engine/Source/File/ServerSaveLoad.cpp`) and from
  replay start (`Engine/Source/File/Replay.cpp` through
  `ServerSessionBase::ResetForLoad`), so no surviving ring can already hold
  the new `iTick`.

Origin: an in-function refactor review of `Engine/Source/Network/Server`,
independently verified with caveats. Unmet criterion: the per-tick buffering
path builds no container whose answer the rings already hold.

## Design

Recommended: delete the `activeCoordinates` set, its `reserve`, and its
`insert`, and make the `std::erase_if` predicate
`rEntry.second.back().iTick != iTick`. Keep the
`ScopedSuppressAllocationTracking` at `SBF:33`: the ring pushes and the
compressed-data copy still allocate.

What this gains is the removal of the set and its per-tick node allocations.
It does not move anything onto or off the tracked-allocation path, since the
whole function already suppresses tracking, and the per-tick
`compressedData.assign` (`SBF:66`) still allocates.

## Critical files

- `Engine/Source/Network/Server/ServerBufferedFrames.cpp`

## In scope

- `ServerBufferedFrames::BufferFrame`: the `activeCoordinates` set and the
  prune predicate, and the comment above the prune if it no longer reads
  true.

## Out of scope

- The ring push, the eviction loop, and the compression path.
- `BufferFullFrame` and the debug full-frame ring, which `BufferFrame` does
  not prune.
- `ClearBufferedFrames` and its callers.
- `AGENTS.md`: the rule in `Engine/Source/Network/Server/AGENTS.md`
  `## Buffered State` — prune from the coordinates actually supplied to
  `BufferFrame` — still holds and is unchanged.

## Risk tier

Tier 2 — scoped behavior (the author's classification). Trigger: it changes
how the server decides which resend rings survive a tick, one subsystem's
runtime behavior with the result meant to be identical. No wire,
determinism/CRC, serialization, or threading change.

## Acceptance criteria

- `BufferFrame` builds no set, and prunes exactly the rings whose newest
  entry is not this tick's.
- The server builds.
- Through `/agent-harness`, a replay playback with a subscribed client runs
  with no CRC mismatch and no disconnect, covering a coordinate that is
  published before it joins the dispatch set. The supplied evidence records
  no harness setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
