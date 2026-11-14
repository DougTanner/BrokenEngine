<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:16:45.259Z","dependsOn":[]} -->
# Skip the draw of a dying Omni whose destroyed time has run out

## Context

Line numbers are from commit `106613c6`; locate by symbol if they moved. Paths
without a root are under
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`.

`RenderPipeline` in `Omnis/OmnisRender.cpp` picks the death scale from the
interpolated `pDestroyedTimes` alone (`:191-195`): a positive time shrinks the
hull, anything else draws at scale 1. An alive Omni also stores zero
(`Omnis/Omnis.cpp:273`), so the test cannot tell "alive" from "dying and
finished".

Traced in code, not run live:

- `OmnisPostRender::BeginExplosion` sets `kDying` and `pDestroyedTimes` to
  `kDestroyTime` (`Omnis/OmnisCombat.cpp:100`, `:102`).
- Each later tick, Interpolate `Update` writes the previous value minus the
  tick delta, clamped at zero, for a row whose previous PostRender is `kDying`
  (`Omnis/Omnis.cpp:374-377`, `:381`). The Destroy sweep removes the row on the
  tick that value reaches zero (`Omnis/Omnis.cpp:215-218`).
- `kDestroyTime` is not a whole number of ticks at `kiTickRate`, so the last
  committed frame that still holds the row holds a positive remainder smaller
  than one tick.
- Render runs Interpolate `Update` on a copy of the source frame with a delta
  inside the one-tick window (`Engine/Source/GameBase.cpp:830-832`,
  `Engine/Source/GameBase.h:336`). Once that delta passes the remainder, the
  copy holds zero, the scale test fails, and the hull draws at scale 1 until
  the next tick removes the row. The render frame before it draws the hull
  almost fully shrunk.

So a dying Omni that is not healed (a random-spawn Omni; a fleet Omni heals
instead, `Omnis/OmnisCombat.cpp:114-125`) can be drawn at full size for the
end of its final tick window. Fighters avoid this: alive Fighters store -1
(`Fighters/Fighters.cpp:494`), and `Fighters/FightersRender.cpp:135-138` skips
a row whose time is exactly zero.

How visible the full-size draw is among the death explosions is unproven: the
path is proven from code only.

`Documents/Plans/Game/EngineModelRenderHelpers.md` `## Out of scope` leaves
each ship type's death scale alone, so that Plan does not cover this.

Originating gap: the in-function refactoring phase of a findings-only analysis
of the `Omnis` directory, confirmed by an independent verification read. It is
pre-existing debt. Unmet criterion: no render frame draws a `kDying` Omni row
at scale 1.

## Design

Recommended (the author's recommendation): in the row loop of `RenderPipeline`
(`Omnis/OmnisRender.cpp:184-230`), skip a row that is `kDying` in
`rPostRender.pLifecycleFlags` and whose interpolated `pDestroyedTimes` is not
positive, before the visibility test at `:200`.

Rationale: `rPostRender` is already a parameter of the function, and skipping
the row instead of scaling it to zero keeps the `XMMatrixInverse` of the
instance transform off a singular matrix. The count loop at `:158-162` may
then reserve for the skipped rows, as it already does for culled rows; leave
it.

Risk tier: Tier 2 (the author's classification). Trigger: one subsystem's
client render behavior. Exposure: client visual only; `RenderPipeline` writes
no simulation state, so there is no CRC, wire, layout, or version change.

## Critical files

- `Omnis/OmnisRender.cpp`

## In scope

- `RenderPipeline` in `Omnis/OmnisRender.cpp`: the row loop's skip test.

## Out of scope

- The alive value of `pDestroyedTimes`, and any change to Interpolate
  `Update`, `BeginExplosion`, or the Destroy sweep.
- The death scale expression and `kfDeathShrinkPower`.
- The count loop, the pipeline loops, and the `ModelLayout` fill; this Plan
  does not change them.
- Fighters and Battleships render.

## Acceptance criteria

1. No path through the row loop draws a row that is `kDying` with a
   non-positive interpolated `pDestroyedTimes`; the diff settles this.
2. `/compile` builds the BrokenEngineSandbox client.
3. Criterion 1 is verified by code reading plus the build of criterion 2. The
   full-size draw lasts less than one tick and no existing `/agent-harness`
   command shows it observably, so no live observation is required.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
