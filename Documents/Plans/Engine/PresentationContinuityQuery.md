<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-12T00:11:21.878Z","dependsOn":[]} -->
# Expose the CPU water origins and retained render areas to a read-only client query

## Context

Two client presentation-continuity guarantees have no live observation point.
`PopulateWaterReducedUv` publishes the camera-relative water origin
(`Engine/Source/Graphics/Render/WaterUniforms.cpp:56-59`) and the CPU-reduced
normal and colour-noise origins
(`Engine/Source/Graphics/Render/WaterUniforms.cpp:136-158`) into the mapped
global layout each frame. `RetainedAreaBasis::Advance`
(`Engine/Source/Graphics/Render/Render.h:58-76`) shifts the retained
shadow/lighting/smoke current and previous area descriptors by exactly one cell
when the camera basis moves to an adjacent cell, and returns `std::nullopt` for a
larger step so the owner resets instead; its three owners are
`Engine/Source/Graphics/Render/GlobalUniforms.cpp:318-330`,
`Engine/Source/Graphics/Render/LightingUniforms.cpp:78-108`, and
`Engine/Source/Graphics/Render/SmokeUniforms.cpp:78-81`, each holding the basis
and the descriptors in function-local statics. No agent command reports any of
those values: the client dispatcher offers the fixtures, `describe_scene`,
`desync_probe`, and `set_client_grid_coord`
(`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp:115-160`),
and `describe_scene`'s camera block carries only `basisCoord`, `eye`,
`visibleArea`, and `lod`
(`Projects/BrokenEngineSandbox/Source/Agent/AgentScene.cpp:171-180`).

Originating gap: `Documents/Plans/Engine/UnboundedRenderCoordinates.md`
acceptance criterion 8's water-phase continuity and history-retention clauses
("continuous water phase, preserved adjacent wind/smoke/lighting/shadow history
... a multi-cell render jump resets history") were landed settled only by code
and adversarial review traces, because a screenshot cannot distinguish a
preserved descriptor from a reset one, and the reduced water phase is
sub-pixel.

Impact: a future change that breaks the one-cell rebase or the phase
reconstruction regresses silently until it is noticed by eye.

## Design

Author's recommendation: add one read-only client agent command,
`presentation_continuity_probe`, taking no parameters, as a focused engine
Commands module beside the existing `cell_coordinate_probe`
(`Engine/Source/Agent/Commands/CellCoordinateProbe.cpp`), dispatched from
`ExecuteSharedAgentCommand` under a `BT_CLIENT` guard. It reports, from the most
recently rendered frame: the camera basis coordinate, the water origin pair, the
three reduced normal origin pairs and the reduced noise origin pair, and, for
each of the shadow, lighting, and smoke owners, its retained basis coordinate,
its current and previous area descriptors, and whether the last basis advance
reset history instead of shifting.

Rationale for a separate command rather than new `describe_scene` camera fields:
the values are engine render state owned by three engine translation units,
while `describe_scene` is the game-side scene dump; the engine probe keeps the
render accessors inside the engine.

The three owners hold their state in function-local statics and the water values
land in the per-command-buffer mapped uniform buffer, which an agent command must
not read. The recommended mechanism is therefore a small render-owned snapshot
struct in `Render.h`, written by the existing render path as it publishes each
value and read by the command; no new threading, no new per-frame work beyond
the copies, and no change to what is published to the GPU.

## Critical files

- `Engine/Source/Graphics/Render/WaterUniforms.cpp:56-59,136-158` — the published origins to snapshot.
- `Engine/Source/Graphics/Render/Render.h:58-90` — `RetainedAreaBasis`, `ShiftArea`, `TemporalAreaLatch`, and the home of the snapshot struct.
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp:318-330`, `Engine/Source/Graphics/Render/LightingUniforms.cpp:78-108`, `Engine/Source/Graphics/Render/SmokeUniforms.cpp:78-81` — the three retained descriptor owners and their reset paths.
- `Engine/Source/Agent/Commands/CellCoordinateProbe.cpp` — the read-only both-endpoint probe this command is modelled on.
- `Engine/Source/Agent/AgentCommandsShared.cpp:222` and `Engine/Source/Agent/AgentCommandsShared.h` — the shared dispatcher and its documented command list.
- `.agents/skills/agent-harness/references/command-reference.md` — the harness command contract.
- `Engine/Source/Agent/AGENTS.md` and `Engine/Source/Graphics/Render/AGENTS.md` — dispatcher and render documentation.

## In scope

- The read-only `presentation_continuity_probe` command module, its dispatch
  entry under the client guard, its strict no-parameter validation, and its
  result schema.
- A render-owned snapshot of the water origins, the three retained basis
  coordinates, their current/previous area descriptors, and the per-owner
  history-reset flag, written where each value is already published.
- Harness command-reference and AGENTS.md documentation for the command.

## Out of scope

- Any change to water phase reduction, the rebase step, the reset rule, the
  temporal latches, or what is written into the global layout.
- `describe_scene`, its camera block, `desync_probe`, or any other existing
  command's schema.
- Server-side exposure: the reported state is client-only presentation state
  outside the CRC.
- Shader source, GLSL bindings, uniform layout, or GPU history handling.
- New unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: a new agent command boundary plus new
per-frame state publication inside the client render subsystem. The command is
read-only, the reported values are client-only and outside the CRC, and no
existing schema, wire format, or uniform layout changes.

Preserve these invariants:

- The command mutates no render, camera, simulation, or subscription state and
  changes no frame timing.
- Reported values come from a snapshot published by the render path, never from
  a mapped uniform buffer read outside it.
- Deterministic simulation, CRC composition, replay, and save data are
  untouched; the reported presentation state stays outside the CRC.
- The command exists only on the client and is rejected on the server.

## Acceptance criteria

- Across a camera crossing into an adjacent cell, sampling the probe before and
  after shows the basis coordinate advance by one cell, each owner's current and
  previous area descriptors shifted by exactly one cell width in the crossed
  axis, no history-reset flag set, and the reduced normal and noise origins
  continuous across the transition rather than jumping by a cell width.
- A multi-cell camera jump sets the history-reset flag for every owner in the
  same probe result.
- The probe rejects any parameter, and the server rejects the command.
- Client `Debug|x64` builds clean through `/compile`, and the command appears in
  the harness command reference with its result schema.

## Notes

The probe is verification observability for guarantees the shipping client
already implements; it adds no player-visible behavior.
