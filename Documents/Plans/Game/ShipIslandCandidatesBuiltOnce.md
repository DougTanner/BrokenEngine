<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:11:45.809Z","dependsOn":[]} -->
# Build the ship island candidates in one place

## Context

Paths are under `Projects/BrokenEngineSandbox/Source/Frame/Collections/` unless
they start elsewhere. Verified against commit `022915b6`.

Each ship type's PostRender `Update` builds the island candidates it passes to
`ComputeReturnDestination` with the same eight lines: count the cell's
`CentredIslands`, reserve a workbuffer block, store one `XMFLOAT4` per
placement (its x and y, the lowest flight plane's height, w 1), and form a
span.

- `BattleshipsPostRender::Update`, `Battleships/Battleships.cpp:414-421`.
- `FightersPostRender::Update`, `Fighters/Fighters.cpp:564-571`.
- `OmnisPostRender::Update`, `Omnis/Omnis.cpp:574-581`.

The three blocks hold no per-type token, and they are the only callers of
`ComputeReturnDestination` (`Ships/ShipBehaviours.cpp:789`;
`Battleships.cpp:505`, `Fighters.cpp:687`, `Omnis.cpp:691`), which already
receives `rStaticData`.

Originating gap: an architecture review of the Battleships collection; unmet
criterion "the helper's input is built in one place" (root `AGENTS.md` DRY).

## Design

Constraints: the candidates are built once per cell per `Update`, not per row,
and their values stay bit-identical. A plain function returning the span cannot
work, because `Workbuffer::PushBuffer` returns a scoped allocation that frees
its block when it leaves scope (`Common/Workbuffer.h:36`, `:214`).

The design is an owning type in `Ships/ShipBehaviours.h`, constructed from
`rStaticData`, that holds the workbuffer allocation and exposes the span, as
`NavRectangleBoard` (`ShipBehaviours.cpp:426-427`) and `FleetBoard`
(`Ships/FleetCoordination.cpp:201`) already own theirs. Each `Update` declares
it where the block stands today, before its boards, so the workbuffer's
last-in-first-out release order is unchanged. It meets both constraints with no
change to `ComputeReturnDestination`.

Considered and rejected: reading `engine::CentredIslands(rStaticData.islands)`
inside `ComputeReturnDestination` instead of passing the candidates, because it
forms each candidate per returning row and so fails the once-per-cell
constraint.

The three types change together.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`

## In scope

- The island-candidates block in `BattleshipsPostRender::Update`,
  `FightersPostRender::Update`, and `OmnisPostRender::Update`, and each one's
  `ComputeReturnDestination` argument.
- One new owning type in `ShipBehaviours.h`/`.cpp`.

## Out of scope

- The rest of `ComputeReturnDestination` and every other `CentredIslands`
  reader.
- `NavRectangleBoard`, `FleetBoard`, and the order of the boards.
- The early return on zero rows in `OmnisPostRender::Update`
  (`Omnis.cpp:563-566`), which Fighters and Battleships do not have; it was
  noted and not assessed.
- `AGENTS.md` files: `Ships/AGENTS.md` names `ComputeReturnDestination` and
  `CentredIslands` but not the candidate buffer.

## Risk tier

Tier 2 — scoped behavior. Trigger: a shared declaration in a header changes
across the three ship collections, in simulation code whose result feeds CRC'd
steering; behavior is meant to be unchanged. No layout, wire, version, or
threading change; the workbuffer stays the per-thread one.

## Acceptance criteria

- The candidate fill loop appears once, and no `Update` body holds it.
- The client and server build.
- A replay determinism check on a scene where random-spawn ships of all three
  types return to an island passes with no CRC mismatch. The supplied evidence
  records no harness setup; the executing session builds one through
  `/agent-harness`.

## Notes

The owning type must not be declared after `NavRectangleBoard` or `FleetBoard`
in a scope that outlives them in a different order than today, because the
workbuffer releases blocks in reverse order.

## Coordination

`Documents/Plans/Game/ShipNavigationDerivesCallerInvariantInputs.md` removes
three `ComputeNavigation` parameters in `ShipBehaviours.h` and
`ShipBehaviours.cpp`, and in the three PostRender `Update` bodies removes the
`vecCellCenter` local, a few lines from the island candidate block this Plan
replaces there, and three `ComputeNavigation` arguments. The two Plans edit
different lines of the same five files; whichever lands second re-derives its
sites from the landed source.

`Documents/Plans/Game/ReduceShipBehavioursCpp.md` may move functions out of
`ShipBehaviours.cpp`. If that reduction lands first, this Plan places the new
owning type from the landed source, in the files that then hold
`NavRectangleBoard` and `ComputeReturnDestination`.
