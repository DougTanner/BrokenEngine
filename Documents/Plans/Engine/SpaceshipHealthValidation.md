<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:49:53.665Z","dependsOn":[]} -->
# Validate persisted Spaceship health before damage and transfer

## Context

The accepted finding `CAI/shard-0049/001` identifies a shared gameplay scalar
admission gap.  `pfHealths` is a serialized shared Spaceship column, but frame
reads copy raw IEEE float bytes without finite validation and no Spaceship
post-read hook repairs them (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.h:140-152`;
`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:743-752`).  A structurally
valid save containing `+∞` health is adopted by `GridSave`; with no live Player,
regeneration does not rewrite it, and collision/area damage only tests
`pfHealths[i] <= 0.0f` (`Engine/Source/File/GridSave.cpp:104-166`;
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsCombat.cpp:78-84,163-232`).
Transfer copies the same value, while `SpaceshipsPostRender::Spawn` accepts any
positive health (`Spaceships.cpp:383-417,587-595`).

The source finding is recorded at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0049.md:71`
and the consolidated selector at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1152`.
Assigned source and authority hashes match frozen baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; the missing finite admission is
pre-existing, unresolved, and outside the audit work.

Impact: a damaged save can make an enemy permanently immune to finite damage,
keep invalid health in CRC'd state, and carry it across normal cell transfers.

## Design

Author's recommendation: validate `pfHealths` at the existing frame read and
Spaceship transfer-spawn boundaries before the value enters fixed-tick damage
or destination state.  Reject non-finite health through the established
corrupt-save/replay/transfer path, preserve valid finite positive health and
the intentional arrival-grace default, and do not use a positive comparison as
a malformed-value repair.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.h:140-156` — shared health column.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsCombat.cpp:78-84,163-232` — regeneration and damage transitions.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp:383-417,553-599` — transfer copy and spawn admission.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:743-752` and `Engine/Source/File/GridSave.cpp:104-166` — frame/save adoption.
- `Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:17-27` — destination transfer consumer.

## In scope

- Finite-health validation at save/replay/full-state read and Spaceship
  transfer-spawn admission boundaries.
- Existing corrupt-state failure propagation before a malformed health value is
  adopted or sent through damage/transfer logic.
- Valid health, damage, explosion, transfer, arrival-grace, and CRC behavior.

## Out of scope

- Health tuning, regeneration policy, damage amounts, collision/area-damage
  ordering, registry IDs, or the separate Player timer scalar.
- Rewriting malformed saves, substituting arbitrary health, or changing save/
  replay/transfer layout for valid state.
- Client-only visual behavior unrelated to the shared health column.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3).  Trigger: opaque
serialized/transfer gameplay state enters CRC-affecting fixed-tick damage and
cross-cell lifecycle.

Tier rationale: the fix only adds finite checks at two named boundaries in the
Spaceships collection and rejects corrupt values through the existing
corrupt-save/transfer failure path.  No save, replay, or transfer layout or
semantics change for valid data, and no tick or threading structure is
touched.

Preserve these invariants:

- Every adopted or transferred Spaceship health value is finite before damage,
  destruction, CRC, or destination publication.
- Invalid state fails before live-frame replacement or transfer row creation;
  valid finite health retains the existing destruction and arrival behavior.
- Client/server shared members, replay, save, and transfer compatibility remain
  unchanged.

## Acceptance criteria

- A structurally valid save/replay/full-state record with `NaN`, `+∞`, or `-∞`
  health is rejected before the next tick can publish or damage the ship.
- A malformed transfer carrying non-finite health is rejected before destination
  `Spawn`; finite positive health still transfers and can reach destruction under
  sufficient damage.
- Client and server `Debug|x64` builds clean through `/compile`; load/transfer
  scenarios observe controlled corrupt-input handling rather than permanent
  invulnerability.

## Notes

The consolidated index places this next to the client population-counter family
as a different scalar/trust-boundary root; no duplicate mapping is made.
