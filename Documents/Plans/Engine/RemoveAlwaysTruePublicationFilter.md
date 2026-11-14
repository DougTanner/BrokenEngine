<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:40:23.636Z","dependsOn":[]} -->
# Remove the always-true filter from the tick publication walk

## Context

`SB` is `Engine/Source/Network/Server/ServerBroadcaster.cpp`. Verified against
commit `4cf9d607`.

`ServerBroadcaster::BuildTickPublication` walks the publication coordinates
through the lambda `ForEachPublicationCoord` (`SB:63-95`). Its replay branch
first walks `rActiveCoordinates` and calls back only for an element not found
earlier in the same list (`SB:74-80`). `rActiveCoordinates` is
`game::gpGame->mActiveCoordinates` (`SB:59`), and every writer keeps that list
free of duplicates. Each push is guarded by a `contains` test:
`ServerSessionRuntime::AddSubscribedCoords` (`:338-340`) and
`ServerSessionRuntime::ComputeActiveSet` (`:376-382`) in
`Engine/Source/Network/Server/ServerSessionRuntime.cpp`, and
`ServerSessionBase::AddOwnedEntityCoordinates`. Or the list is rebuilt from
the unique keys of the replay reader map:
`Engine/Source/File/Replay.cpp:556-560` and
`GameBase::RefreshReplayActiveSet` (`Engine/Source/GameBase.cpp:1034-1038`).
So the `std::find` test at `SB:76` is always true, and the loop does what the
non-replay loop at `SB:67-70` does, with a quadratic scan.

Also, `ServerSessionRuntime::CompleteTick` clears
`ServerBroadcaster::mBroadcastStatusChanges`
(`ServerSessionRuntime.cpp:284`) right after `BuildTickPublication` returns,
while `BuildTickPublication` itself retires the transfer batch at its end
(`SB:187`). Nothing reads the map between the two points. The clear is
load-bearing in a multi-tick update and must remain.

Origin: an in-function refactor review of `Engine/Source/Network/Server`,
independently verified. Unmet criterion: the publication walk holds no branch
whose test cannot fail.

## Design

Recommended: in `ForEachPublicationCoord`, run the plain walk of
`mActiveCoordinates` always, return after it when not replaying, and keep the
two replay-only loops over `mBroadcastStatusChanges` and `rTransfers`
(`SB:81-94`) after it. Publication order is unchanged in both modes.

In the same change, move the `mBroadcastStatusChanges.clear()` from
`CompleteTick` to the end of `BuildTickPublication`, beside the transfer
batch clear, so the broadcaster retires both of its tick inputs in one place.

## Critical files

- `Engine/Source/Network/Server/ServerBroadcaster.cpp`
- `Engine/Source/Network/Server/ServerSessionRuntime.cpp`

## In scope

- The `ForEachPublicationCoord` lambda in
  `ServerBroadcaster::BuildTickPublication`, and the `rActiveCoordinates`
  alias if it is then unused.
- The end of `BuildTickPublication`: one added clear.
- `ServerSessionRuntime::CompleteTick`: the removed clear.

## Out of scope

- The two replay-only loops and their `contains` tests.
- The publication block layout, the workbuffer preflight, and `PublishTick`.
- Every other clear of the map: `BuildFrameInputs`, `ResetState`, and the
  replay owner's.
- The writers of `mActiveCoordinates`.

## Risk tier

Tier 2 — scoped behavior (the author's classification). Trigger: it
restructures the code that fixes the order of a tick's published coordinates,
which the buffered updates and the replay publication follow, inside one
subsystem and with the order meant to be unchanged. No wire layout,
determinism/CRC, serialization, or threading change.

## Acceptance criteria

- `ForEachPublicationCoord` has no duplicate test over the active list, and
  visits the same coordinates in the same order as before in both modes.
- `mBroadcastStatusChanges` is cleared once per completed tick, inside
  `BuildTickPublication`, after `PublishTick`.
- The server builds.
- Through `/agent-harness`, a replay playback with a subscribed client and a
  multi-tick update each run with no CRC mismatch. The supplied evidence
  records no harness setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
