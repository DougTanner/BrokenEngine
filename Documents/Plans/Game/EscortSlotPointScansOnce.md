<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:09:40.864Z","dependsOn":[]} -->
# Scan the Fighter rows once per escort slot lookup

## Context

Paths are relative to `Projects/BrokenEngineSandbox/Source/`; `Ships/` stands
for `Frame/Collections/Ships/`. Line numbers are as of commit `fe1fb69b`.

`ChooseFleetTactic` calls the file-local `EscortSlotPoint` twice for an
escorting fleet Fighter (`Ships/FleetCoordination.cpp:272-273`), with the same
`rPreviousFrame`, `rView`, and `globalId` and a different `iTickOffset` (0,
then `kiEscortLeadTicks`). The row scan in `EscortSlotPoint` (`:132-148`) that
produces `iCount` and `iSlot` reads only `pFlagshipGlobalIds`,
`pLifecycleFlags`, and `pGlobalIds`; `iTickOffset` is first read at `:152`. So
both calls walk every previous-frame Fighter row in the cell, all fleets and
random-spawn rows included, to get the same two integers.

The only route to the pair is a Patrol fleet Fighter's escort tick:
`Frame/Collections/Fighters/Fighters.cpp:633` calls `ChooseFighterFleetTactic`,
which calls `ChooseFleetTactic` at `FleetCoordination.cpp:301`; an Omni returns
before the pair (`:268-271`).

Originating gap: a repeated-local-logic finding of the in-function refactoring
phase of a deep analysis of `Ships/` (the directory, not its subdirectories) at
commit `fe1fb69b`, under the root `AGENTS.md` KISS and DRY directives. It is
pre-existing debt, not an unmet acceptance criterion of any change.

Caveat from the verifying review: the saving is real but small and unmeasured.
A fleet's Fighter count is bounded by `kiMaximumFleetSquadronsPerType` and
`kpiFleetSquadronCapacities` (`Fleet.h`), the scan is three integer loads per
row, and no profile shows it. The growth with rows times escorting Fighters
remains; this change only halves it. Nothing was built or run.

## Design

Recommended: inside `FleetCoordination.cpp`, run the row scan once per
escorting Fighter and produce both points, the slot point and the lead point,
from the one `iCount` and `iSlot`. Rationale: the scan's inputs are identical
for both calls, and the change stays inside one file-local function and its two
call lines.

The phase arithmetic at `:152-156` runs once per offset exactly as today, so
both results stay bit-identical: the scan is integer-only, the row order and
the float operation order are unchanged, and nothing writes a flag. How the two
points are returned (for example a small file-local result struct, or a scan
helper that returns `iCount` and `iSlot`) is a trivial choice left to the
executing session. The comment above `EscortSlotPoint` (`:124-126`) is updated
to match.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/FleetCoordination.cpp`

## In scope

- The file-local static `EscortSlotPoint` and its comment
  (`FleetCoordination.cpp:124-157`).
- The two `EscortSlotPoint` calls in `ChooseFleetTactic`
  (`FleetCoordination.cpp:272-273`).

## Out of scope

- `FleetCoordination.h` and every caller outside the file.
- The escort slot rule itself: the slot order, the lap clock, the lead, and the
  speed formula below the two calls.
- The scan's growth with rows times escorting Fighters, for example a per-fleet
  cache on `FleetBoard`.
- `FindSquadronTarget`'s similar row walk, and every other function in the
  file.
- `AGENTS.md` files: `Frame/Collections/Fighters/AGENTS.md` describes the slots,
  not the scan count.

## Acceptance criteria

1. One escort tick of a fleet Fighter walks the previous-frame Fighter rows
   once: the diff shows a single scan feeding both points.
2. The diff leaves the per-offset phase arithmetic unchanged.
3. The BrokenEngineSandbox client and server build.
4. A replay determinism check through `/agent-harness`, with fleet Fighters
   escorting their flagship, reports no CRC mismatch. The originating analysis
   ran nothing, so it records no harness setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- Change Workflow tier: Tier 2 (the author's classification). Trigger: the
  function feeds the per-tick CRC through the escort destination, an invariant
  exposure Tier 1 excludes. Its results are unchanged, so no determinism/CRC
  surface changes, and no header changes.
- Exposure: determinism/CRC only if the arithmetic or row order changed. No
  serialization, `.pack`, `kiVersion`, wire, replay-format, threading,
  allocation, or shader exposure.
