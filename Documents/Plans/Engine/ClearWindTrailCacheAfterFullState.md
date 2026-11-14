<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T14:22:30.366Z","dependsOn":[]} -->
# Stop a resync full state from pairing wind trails with stale positions

## Context

Found by the architecture review phase of a deep analysis of
`Engine/Source/Frame/Collections` (2026-11-14). Line numbers are from commit
`ae681f03` plus the change this Plan landed with; locate by symbol if they
moved.

`WindTrailsInterpolate::sPreviousPositions`
(`Engine/Source/Frame/Collections/WindTrails/WindTrails.h:44`) caches each
trail's previous rendered position, keyed by the trail's visual `Id`. A resync
full state for a cell that is already rendered reissues those ids to different
trails, and the cache then pairs a trail with another trail's old position.

Traced sequence:

1. `Engine/Source/Network/Client/Client.cpp:394-395` reads the full state into
   a fresh `game::Frame`. `FrameBase.cpp:134-142` reads server collections
   only, so `windTrails` is empty and `uiNextVisualUuid` is 1
   (`Engine/Source/Frame/FrameBase.h:203`).
2. `HydrateReceivedFullState`
   (`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:296-301`)
   re-adds the trails through `WindTrailsPostRender::Add`, issuing ids from 1
   under the cell's unchanged frame identifier.
3. A cell with confirmed state keeps rendering: the frame is stored as
   `optionalPendingFullState` (`ClientSessionRuntime.cpp:563-577`) and
   `ReconcileInjectPendingFullState`
   (`Engine/Source/Network/Client/ReconcileReplay.cpp:15-39`) later moves it
   into the snapshot ring.
4. On the next rendered frame `WindTrailsRender.cpp:28-31` keeps every cached
   entry whose id exists in the new `idToIndexMap`; `:94` reads another
   trail's cached position and `:107-110` builds a quad across the gap.

The map is cleared only at `Engine/Source/GameBase.cpp:953` and `:1194`.

Effect: visual only — one spurious wind-deposit quad per affected trail for one
rendered frame. No CRC, serialization, or version exposure. The repository
already keeps visual continuity across this same event for smoke
(`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:303-314`).

Unmet criterion: a wind trail's first rendered frame after its id is reissued
deposits nothing.

## Design

User decision, recorded by the authoring agent from the user's own words
(2026-11-14): clear the wind trail position cache on the main thread after a
resync full state is applied. Accepting the artifact and documenting it was
the other form offered, and the user did not choose it.

The change clears `sPreviousPositions` on the main thread after a reconcile
dispatch in which any coordinate injected or adopted a pending full state
(`ReconcileReplay.cpp:15-39`, `:58-93`). A cleared map makes every trail
deposit nothing for one frame, which is the documented first-frame behavior.

Two sites that look right are wrong:

- `AdoptFullState` only stores the pending frame; frames rendered before the
  injection refill the cache with the old ids.
- The injection itself runs in the per-coordinate reconcile worker, and
  `Engine/Source/Network/Client/AGENTS.md` `## Reconciliation Replay Chain`
  says each worker writes only its coordinate, so a worker must not touch this
  file-scope map.

## Critical files

- `Engine/Source/Network/Client/ReconcileReplay.cpp` and the main-thread caller
  of its dispatch
- `Engine/Source/Frame/Collections/WindTrails/WindTrails.h`
- `Engine/Source/Frame/Collections/WindTrails/AGENTS.md`

## In scope

- One main-thread clear of `WindTrailsInterpolate::sPreviousPositions` after
  the reconcile dispatch, the smallest signal the workers need to report that
  a full state was injected or adopted, and the documentation sync
  `/update-claude-docs` requires.

## Out of scope

- Leaving the artifact in place and only documenting it.
- The id permutation a reconcile replay with a different spawn order can cause
  inside the replay window: not traced, not part of this Plan.
- How visual ids are issued, `EraseStaleRenderState`, and the smoke continuity
  path.

## Risk tier

Tier 3 — invariant/integration. Trigger: the change spans two independently
owned subsystems (`Network/Client` and `Frame/Collections`) and adds a
main-thread write beside worker-thread reconcile code.

## Acceptance criteria

- `sPreviousPositions` is cleared on the main thread after a reconcile
  dispatch that injected or adopted a pending full state, and no reconcile
  worker reads or writes it.
- `/compile` builds the BrokenEngineSandbox client and server.
- The supplied evidence records no harness setup that produces a resync full
  state for a rendered cell. If the executing session finds none, the path is
  verified by code reading plus the build.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
