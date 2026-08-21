# Registry query-window workbuffer budget

Option-presenting investigation. Not decision-complete, so not a Plan
(`Documents/AGENTS.md` deciding test). It earns a Plan by moving to
`Documents/Plans/<area>/` once the mechanism below is chosen.

## Finding

The two `engine::FrameRegistry` query windows the FrameRegistry change added
size their scratch from live entity counts, against a worker workbuffer that is
a fixed 65,536 bytes. Nothing bounds those counts, so the "no workbuffer growth
inside a query window" property currently holds by headroom, not by
construction.

Both windows — `Frame::MissileUpdateWindow` and `Frame::PlayerSpawnWindow`
(`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:565` and `:573`) — call the
same builder, `BuildSpaceshipRegistryWindow`, so one budget covers both. That
builder makes three worker-workbuffer reservations per window:

- Ascending row indices — `8 * max(spaceshipRows, missileRows)` bytes
  (`Frame.cpp:508-509`).
- Registry scratch — `RegistryScratchBytes` returns
  `(sizeof(int64_t) + sizeof(uint8_t)) * eligibleRows`, i.e.
  `9 * eligibleSpaceshipRows` bytes
  (`Engine/Source/Frame/FrameRegistry.cpp:108-111`, reserved at
  `Frame.cpp:523`).
- One `RegistrySourceLayer` (fixed, tens of bytes, `Frame.cpp:535`).

The worker workbuffer is constructed at 65,536 bytes
(`Common/Source/Threading/Multithreading.cpp:15`,
`std::make_unique<PersistentWorker>(std::nullopt, 65536)`).

Overflowing it is not a clean failure. `common::Workbuffer::Grow`
(`Common/Source/Workbuffer.cpp:78-94`) fires `DEBUG_BREAK()`, logs at `kError`,
and then resizes the backing vector — which reallocates and invalidates every
live pointer taken from the buffer before the grow. The window holds exactly
such pointers (`pAscendingRows`, `pScratch`, `pLayers`) across the whole query,
so a grow mid-window is use-after-free of the query context in Release.

Thresholds, from the arithmetic above against 65,536 bytes:

- Missiles dominant, no eligible spaceships: roughly 8,192 missile rows in one
  cell.
- All spaceships eligible, missiles fewer: roughly 3,855 spaceship rows in one
  cell (`17 * rows`).

Current gameplay is far below both: spawning adds at most
`kiMaxFleetSize` = 16 spaceships per player group every 0.5 s
(`Frame.cpp:245`, `:439-445`), and a firing player produces on the order of 50
missiles. The gap is headroom, not a guarantee: `SpawnSpaceshipGroup` caps only
one group's size and not the live total, the spawn loop runs every half second
with no live-count ceiling, and Collection storage grows dynamically, so a long
session, a stress fixture, or a future higher-density scenario can climb toward
those thresholds with nothing to stop it.

This was proven during the FrameRegistry session's review (a related Critical —
unbounded per-missile batch reservations — was fixed inside that change by
fixed-size stack chunks). Bounding the window's own reservations was outside
that Plan's `## In scope`, which authorized building the two windows and their
scratch but no change to worker workbuffer sizing or to entity spawn limits.

## Why this is not yet a Plan

The fix requires a decision that no existing user direction, approved plan, or
documentation settles: what actually bounds the numbers. The two candidate
mechanisms live in different subsystems, have different costs, and are not
interchangeable.

### Option A — size the worker workbuffer to a documented budget

Raise (or explicitly right-size) the 65,536-byte worker workbuffer and record,
next to that constant, the per-window byte formula and the entity counts it
covers.

- For: one constant and one comment; touches only
  `Common/Source/Threading/Multithreading.cpp` plus documentation; no gameplay
  change; nothing else in the tick has to learn a new rule.
- Against: it only moves the cliff. Without a cap on entity counts there is
  still no count at which the invariant is guaranteed — the budget is an
  assumption about gameplay, and the failure mode past it is still a
  pointer-invalidating grow. It also costs memory in every worker, whether or
  not that worker ever builds a query window.

### Option B — a per-cell entity budget the windows can rely on

Give spaceships and missiles a hard per-cell ceiling and size the workbuffer
against it, so the reservation is provably bounded.

- For: the invariant then holds by construction; the budget becomes a checkable
  property rather than an estimate.
- Against: it is a gameplay-visible rule (what happens at the ceiling — refuse
  the spawn, recycle the oldest row, or something else — is itself an open
  question), it touches simulation state that is CRC'd and replayed, and it is a
  materially larger change than Option A.

A third possibility worth putting to the decider: make the overflow a clean
failure instead of budgeting for it — for example by making the window's
reservation refuse to exceed the buffer and degrade the query rather than grow.
That trades a silent memory corruption for a defined behavior without capping
entities, but it needs a decision about what a degraded query is allowed to
return, which affects determinism.

The author has no recommendation strong enough to record as a decision: Option A
is the smallest change, Option B is the only one that makes the claimed
invariant true, and the choice is a gameplay/architecture call rather than an
implementation detail.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp` —
  `BuildSpaceshipRegistryWindow` (`:495-563`), `MissileUpdateWindow` (`:565`),
  `PlayerSpawnWindow` (`:573`), `SpawnSpaceshipGroup` (`:240`) and the spawn loop
  (`:439-445`).
- `Engine/Source/Frame/FrameRegistry.cpp` — `RegistryScratchBytes` (`:108-111`).
- `Common/Source/Threading/Multithreading.cpp:15` — worker workbuffer size.
- `Common/Source/Workbuffer.cpp:78-94` — `Grow` and its pointer-invalidation
  contract.

## Exposure if implemented

Option A is scoped tool/runtime sizing (expected Change Workflow Tier 2).
Option B reaches simulation state that is CRC'd, serialized, and replayed, and
so is Tier 3. Line numbers above are as of the FrameRegistry session and should
be re-derived when this becomes a Plan.

## Provenance

Recorded as a Change Workflow Step 7 proven out-of-scope residual of the
FrameRegistry session (`Documents/Plans/Engine/FrameRegistry.md`).
