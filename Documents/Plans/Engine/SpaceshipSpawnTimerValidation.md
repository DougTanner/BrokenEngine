<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T02:00:02.174Z","dependsOn":[]} -->
# Bound persisted Spaceship spawn timers before the spawn loop

## Context

Final survivor `S014-C003` is a retained HIGH deterministic-frame finding. `FrameInterpolate::Read` and `ServerRead` normalize only non-finite `fSpawnTimer`; a finite value near `float::max()` survives. `FramePostRender::Spawn` subtracts `0.5f` while the timer remains above the threshold and calls `SpawnSpaceshipGroup` without a bound, but binary32 subtraction at that magnitude need not make progress (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:676-699,415-440`).

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-019.md` under `S014-C003 — FINAL: RETAIN_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-014.md:151` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:240`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to admit only finite spawn timers within the range that the fixed-tick spawn loop can consume and make the existing frame/save/replay/full-state failure path reject an out-of-range value before `Spawn`. Preserve ordinary finite timer accumulation, random-draw cadence, Spaceship spawn behavior, and the existing non-finite normalization policy where valid.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:635-699` — timer CRC/read/write and interpolation admission.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:415-440` — `FramePostRender::Spawn` timer loop.
- `Engine/Source/Frame/FrameBase.cpp:337-361` — fixed-tick phase ordering.
- `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` and `Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md` — deterministic frame/progress contracts.

## In scope

- Finite and bounded `fSpawnTimer` validation at build-local/frame-read and server-read boundaries.
- Existing corrupt save/replay/full-state failure propagation before the Spawn phase.
- Bounded loop progress and valid timer/spawn/random/CRC behavior.

## Out of scope

- Spaceship spawn rates, group composition, random algorithm, frame layout/version, CRC algorithm, or unrelated Player timers.
- Silently clamping malformed values into arbitrary spawn bursts, new compatibility readers, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: opaque save/replay/full-state scalar data enters CRC-affecting fixed-tick phase control and can prevent deterministic client/server progress.

Preserve these invariants:

- Every accepted finite spawn timer permits bounded progress through `FramePostRender::Spawn`.
- Invalid frame input fails before Spawn/state publication rather than looping or growing collections indefinitely.
- Valid save/replay/full-state timer values, random draws, CRC membership, and client/server behavior remain compatible.

## Acceptance criteria

- Current-version save, replay, and full-state frames containing `float::max()` or another loop-nonprogress timer are rejected before Spawn and follow the existing failure channel.
- Ordinary finite timers and the non-finite-to-zero path retain current spawn cadence and CRC behavior.
- Client and server `Debug|x64` builds pass through `/compile`; a focused malformed-frame scenario cannot hang a tick or grow Spaceships without bound.

## Notes

Origin: `S014-C003`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-019.md` (`S014-C003 — FINAL: RETAIN_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-014.md:151`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:240`. No exact existing Plan was found; the Player timer Plan is a distinct collection/loop. No source fix or build was performed during routing.
