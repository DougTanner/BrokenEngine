<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:50:19.695Z","dependsOn":[]} -->
# Reject empty Fleet GUIDs before staged fleet adoption

## Context

The accepted finding `CAI/shard-0054/004` identifies a persistent Fleet identity
gap.  `ReadFleet` accepts an all-zero `FleetGuid` while reading a save
(`Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:55-60`).
The client FleetSync decoder also accepts and displays the record
(`Projects/BrokenEngineSandbox/Source/Network/GameMessages.h:127-168`), and a
normal HUD request can name it.  Server lookup succeeds, but
`ServerBroadcaster::BuildFrameInputs` uses an empty `FleetGuid` as the
non-fleet spawn sentinel and `ServerFleetManager::OnPlayerSpawned` returns
before adding the member (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp:36-45,123-152,293-299`;
`Engine/Source/Network/Server/ServerBroadcaster.cpp:35-69`).

The source finding is recorded at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0054.md:116`
and the consolidated selector at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1206`.
Assigned source and authority hashes match frozen baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; the reserved-identity admission
is pre-existing, unresolved, and outside the audit work.

Impact: a header-valid save can expose a fleet whose spawn controls repeatedly
create client-owned players that never enter the Fleet roster or establish a
flagship.

## Design

Author's recommendation: reject an empty `FleetGuid` while staging Fleet save
records, before the Fleet can be adopted or emitted in FleetSync.  Preserve the
empty value exclusively as the existing non-fleet `ClientSpawnInfo` sentinel,
retain valid nonempty Fleet lookup and spawn behavior, and route the invalid
record through the existing post-header corrupt-save/fresh-state path.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:55-60,140-175` — Fleet identity read and staged adoption.
- `Projects/BrokenEngineSandbox/Source/Network/GameMessages.h:127-168` — FleetSync reader evidence.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp:36-45,123-152,293-329` — lookup and member-association consumers.
- `Engine/Source/Network/Server/ServerBroadcaster.cpp:35-69` — empty FleetGuid sentinel in spawn input.
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp:317-325` — ordinary Fleet spawn request consumer.

## In scope

- Empty/all-zero `FleetGuid` rejection at staged Fleet save-read admission.
- Existing corrupt-save failure propagation before `mFleets` or FleetSync can
  contain the reserved sentinel.
- Valid nonempty Fleet identity, lookup, sync, spawn, and flagship behavior.

## Out of scope

- Duplicate owner GUIDs, duplicate Fleet GUID policy, flagship-index relation,
  reconnect publication, or client FleetSync schema changes.
- Changing the non-fleet `ClientSpawnInfo` sentinel, valid Fleet GUID
  generation, request routing, or member association.
- Repairing a zero GUID by minting a new identity or rewriting the save.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3).  Trigger: opaque
serialized persistent identity crosses staged save, server spawn, and client
FleetSync boundaries.

Tier rationale: the fix is one emptiness test on a value already read in
`ReadFleet`, rejected through the existing corrupt-save path. Nothing about the
save layout, the sentinel's meaning, or valid Fleet identity and spawn
behavior changes.

Preserve these invariants:

- Every actual Fleet stored in `mFleets` or sent in FleetSync has a nonempty
  FleetGuid; empty remains reserved for non-fleet spawn input.
- Invalid save input fails before Fleet state or sync publication; valid Fleet
  requests continue to associate spawned players and flagship state normally.
- Fleet save/wire layout, owner identity, simulation CRC, and reconnect behavior
  remain unchanged for valid data.

## Acceptance criteria

- A structurally valid save containing an empty FleetGuid is rejected before
  staged adoption, FleetSync, or a HUD spawn request can resolve it.
- A valid nonempty empty-member Fleet still loads, syncs, and accepts a player
  spawn that is appended to the Fleet roster.
- Server `Debug|x64` builds clean through `/compile`; load/sync/spawn evidence
  observes controlled corrupt-input handling rather than untracked players.

## Coordination

`Documents/Plans/Game/FleetFlagshipIndexValidation.md` and
`Documents/Plans/Game/FleetOwnerGuidUniqueness.md` validate other predicates
at the same staged Fleet reader.  Keep the reserved FleetGuid sentinel check
independent, preserve the common failure path, and re-derive line ranges before
implementation.

## Notes

The consolidated index lists this candidate among repeated owner/reconnect
references but records no duplicate-family hint for the empty FleetGuid root.
