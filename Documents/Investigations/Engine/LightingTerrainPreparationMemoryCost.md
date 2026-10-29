# Lighting terrain preparation cost after RAM buffer placement

## Question

Does placing the lighting foundation's temporary buffers in system RAM make terrain trace-geometry
preparation about ten times slower? This record keeps the question open after the matched-cost
experiment (commit `49cbb2ff` on `lighting-foundation-handoff-20261028`) was reverted; that
experiment was never compiled and added about 260 lines of temporary receipts beyond what the
question needs. The retained memory policy is recorded in
[LightingFoundationHandoff.md](LightingFoundationHandoff.md) `## Decisions`.

## Observation

On the same saved workload (N970219 vertices, P22 placements, T28 tiles; equal shader packs and
selected witness records), GPU-timestamp terrain `productionMs` rose from 14.88 to 150.84 and
`validationMs` from 4.03 to 12.77 after the user-selected memory policy moved compatible temporary
buffers to coherent system RAM. Both runs were instrumented pre-rebase builds, so neither is a
current measurement.

The cause is unproven. `productionMs` in `Engine/Source/Graphics/Lighting/Terrain.cpp` spans
producer dispatches, tile rasters, barriers and the terrain BLAS builds in one interval, and it also
absorbs the validation-mode evidence writes. Acceleration-structure build scratch in system RAM is
one plausible contributor, not an established one.

## When to run

After the foundation builds cleanly on current data with its temporary diagnostic instrumentation
removed, and only if that build still shows terrain preparation far above the 15 ms order. If the
gap is gone, delete this record.

## Method

The measured producer no longer exists in that form: the foundation strip removed the off-screen
terrain tile rasters and the validation writes, so terrain preparation is now one compute dispatch
per drawn placement plus its BLAS builds, and `productionMs`/`validationMs` are gone. The gap may
therefore be moot; measure before investigating further.

Use the existing terrain timing; add no receipts or new harness commands. Terrain preparation runs
only while Lighting comparison mode is not disabled, and its GPU time is the `query_profile`
`gpuTimers` row whose `name` ends `Lighting Terrain`. On the same saved workload and binary, run
once with the current placement and once with the foundation's temporary buffers forced to the
default device-local placement, then compare that row. If RAM placement accounts for a gap, return
the measured numbers to the user: keeping or narrowing the RAM placement is their decision, because
they selected it to relieve GPU memory pressure.
