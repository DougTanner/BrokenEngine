<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T13:11:52.265Z","dependsOn":[]} -->
# Compute the server display grid bounds in int64_t

## Context

`Documents/C++StyleGuide.txt` rule 13 (lines 101-106) defaults our own locals to `int64_t`. `PaintGridMap` in `Engine/Source/Server/ServerDisplay.cpp` keeps its grid bounds `iMinimumX`, `iMaximumX`, `iMinimumY`, `iMaximumY` as `int32_t` locals (`:208-211`) because they are seeded from `GridCoord::iX`/`iY` (`Engine/Source/Frame/GridCoord.h:8-9`, `int32_t`). The OwnIntegerTypeSweep left them unchanged because a plain widening lets the one-cell padding step outside the `int32_t` range, and the loop's narrowing cast into `GridCoord` (`:251`) would then wrap to a cell on the far side of the grid. Origin: `Documents/Investigations/ChangeWorkflow/OwnIntegerTypeSweepDeferredFixes.md`, deleted when the Plans are created.

The current `int32_t` form is not safe either: the padding at `:221-224` (`iMinimumX -= 1` and the three siblings) is signed `int32_t` overflow, undefined behavior, when an active coordinate sits at `INT32_MIN` or `INT32_MAX`, and `iMaximumX - iMinimumX + 1` at `:226-227` is evaluated in `int32_t` before it is stored in `int64_t`, so it also overflows when the active cells span more than `INT32_MAX` columns or rows. GridCoords are unbounded sparse-grid coordinates, so both are reachable in principle. Rule 13 in `Documents/C++StyleGuide.txt` names no exemption that keeps these four locals, so they are a behavior decision rather than an exemption; this Plan is that decision.

## Design

The author recommends computing the bounds in `int64_t` and clamping the one-cell padding to the `int32_t` coordinate range, so every loop coordinate fits `GridCoord` exactly and no cell outside the coordinate space is drawn:

1. Declare the four bounds at `:208-211` as `int64_t`, initialized from the first active coordinate.
2. The accumulation at `:215-218` compares `int64_t` with `int32_t`; `std::min`/`std::max` need one type, so widen the coordinate operand (`static_cast<int64_t>(rCoordinate.iX)` or the explicit `std::min<int64_t>` form, whichever the reviewer finds plainer).
3. Replace the padding at `:221-224` with a clamp: the minimum moves to `std::max(iMinimum - 1, int32 minimum)` and the maximum to `std::min(iMaximum + 1, int32 maximum)`, using `std::numeric_limits<int32_t>` widened to `int64_t`. Rationale: a padding cell beyond the `int32_t` range has no `GridCoord`, so dropping it is the only correct rendering; the narrowing at `:251` then never changes a value.
4. `iGridWidth`/`iGridHeight` at `:226-227` are then computed in `int64_t` with no change to their text. A span wider than the map makes `iCellSize` fall below 4 and the existing early return at `:238-241` fires, as today for large spans.

Leave the loop at `:243-251` unchanged: its `int64_t` loop variables and the `static_cast<int32_t>` into `GridCoord` are already the rule 13 form.

Risk tier: Tier 2 (`.agents/references/risk-tiers.md`) — trigger: scoped runtime behavior in one subsystem (the server's Win32 status display; behavior changes only at the `int32_t` coordinate extremes). No determinism/CRC, wire, serialization, threading, or trust-boundary exposure: `PaintGridMap` only reads `game::gpGame->mActiveCoordinates` and client connections to paint the server window.

## Critical files

- `Engine/Source/Server/ServerDisplay.cpp` — `PaintGridMap` (`:200`), bounds `:208-227`.

## In scope

- `PaintGridMap` in `Engine/Source/Server/ServerDisplay.cpp`: the four bound declarations (`:208-211`), the accumulation (`:215-218`), and the padding (`:221-224`).

## Out of scope

- `GridCoord` (`Engine/Source/Frame/GridCoord.h`) and its `int32_t` members: grid-save and wire layout.
- The paint loop body (`:243` onward), the hash in `ServerDisplayContentChanged()`, and every other function in `ServerDisplay.cpp`.
- Any other rule 13 site from the origin investigation.

## Acceptance criteria

- The four bounds are `int64_t`; no `int32_t` arithmetic remains in the bound computation.
- With active coordinates at `INT32_MIN` or `INT32_MAX`, the padding stays inside the `int32_t` range, so every `GridCoord` built at `:251` equals its loop coordinates.
- For ordinary coordinate ranges the painted grid is unchanged: one padding cell on each side.

## Notes

- Verification: build the server (`/compile`); the diff settles the extremes. An optional `/agent-harness` server launch confirms the display still paints the active grid with its padding ring.
- Server-only file (`#if defined(BT_SERVER)`); no project-membership change.
