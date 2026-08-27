<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:40:40.900Z","dependsOn":[]} -->
# Fix: Normalize persisted camera eye height for the active build

## Context

The accepted survivor `CAI/shard-0045/001` shows that
`LoadClientState` copies the persisted eye-height target directly into the
remembered state and `Camera::RestoreEyeHeight`
(`Projects/BrokenEngineSandbox/Source/ClientSettings.cpp:86-103`).  Debug/Profile
can persist a target up to 2000 while Release's active ceiling is 600
(`Engine/Source/Graphics/EngineCamera.cpp:15-19`), and neither restore path
normalizes it.  The same version/size-valid file can carry zero, NaN, or
infinity into camera matrix and visible-area math.

The source report is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0045.md:33`;
the consolidated selector is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1083`.
The report's 11 target rows matched frozen baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source was changed in this
routing session.

Impact: Release can start above its documented zoom ceiling after a Debug run,
and malformed local state can poison camera projection/visible-area state.

## Design

Author's recommendation: make `Camera::RestoreEyeHeight` the single
normalization boundary and have it return the normalized value.  If the input
is non-finite, use `kfCameraEyeHeightInitial`; otherwise clamp it to
`[kfMinEyeHeight, kfEyeHeightMax]`, then assign that finite value to both live
and target fields.  Store the returned value in
`mfRememberedCameraEyeHeightTarget` from `LoadClientState`, so the in-memory
mirror and any later save agree with the active build.  Keep the serialized
field/version unchanged.

## Critical files

- `Engine/Source/Graphics/EngineCamera.cpp:15-19,241-307` — active-build
  ceiling, normal zoom clamp, and restore implementation.
- `Engine/Source/Graphics/EngineCamera.h:56-77,130-145` — restore API and
  shared camera constants.
- `Projects/BrokenEngineSandbox/Source/ClientSettings.cpp:61-103` — opaque
  file load and remembered-state publication.
- `Engine/Source/Main.cpp:287-314` — startup call ordering.

## In scope

- Finite/range normalization in `RestoreEyeHeight` and propagation of its
  normalized return value to the remembered client state.
- Keeping startup load timing, valid saved values, camera easing bypass, and
  active Debug/Profile versus Release ceilings intact.
- Updating the declaration/comment for the changed return contract.

## Out of scope

- Client-state file version/layout changes, fleet/focus persistence, mouse-wheel
  input, camera projection equations, or unrelated settings floats.
- The sibling animation/camera audit record `CAI/shard-0027/002`; its probable
  duplicate is mapped only if a live Plan already owns the same boundary.
- Server code, simulation CRC, wire/save/replay formats, and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Trigger: opaque
persisted state crosses game and engine camera boundaries and feeds
render-visible matrix/LOD state; trust boundary and client integration are
higher-risk surfaces.

Preserve these invariants:

- Restored eye height is finite and within the active build's minimum/maximum
  range before any camera update or visible-area calculation.
- Live camera and remembered target hold the same normalized value.
- Valid in-range saved values restore exactly, while invalid values use the
  documented finite default/clamp behavior.
- No simulation CRC, wire, save, replay, or `.pack` representation changes.

Tier rationale: the Design fully specifies a finite check and clamp inside one
function, `Camera::RestoreEyeHeight`, plus storing its returned value at the
single caller. The persisted field and version are unchanged and an in-range
value restores exactly as it does today, so only out-of-range or non-finite
input behaves differently.

## Acceptance criteria

- A Debug/Profile-saved 2000 target loaded by Release is clamped to 600 before
  the first render and remains within the Release ceiling without scroll input.
- Version/size-valid zero, NaN, positive infinity, and negative infinity files
  produce finite in-range camera and remembered state, with no invalid matrix or
  zoom-bucket calculation.
- An in-range current-format file restores the exact eye height and saves it
  unchanged on the next state capture.
- Client Debug and Release builds pass `/compile`; a harness restart/state-load
  scenario has no non-finite camera or visible-area output.

## Notes

Consolidated `DUP-003` also names `CAI/shard-0027/002` at the
`RestoreEyeHeight` boundary.  No executable Plan for that sibling existed in
the live inventory when this Plan was drafted; this Plan remains the candidate
owner if its boundary is later proven identical.
