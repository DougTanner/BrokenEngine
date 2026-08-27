<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:49:42.234Z","dependsOn":[]} -->
# Reject reserved `kRespawnPlayer` records before player creation

## Context

The accepted finding `CAI/shard-0048/001` identifies a replay status-variant
gap.  `kRespawnPlayer` is a reserved empty `TransferData` arm, but
`PlayersPostRender::ProcessSpawnStatusChanges` enters the creation branch for
both `kSpawnPlayer` and `kRespawnPlayer`; only the former fills the global ID
and fleet fields (`Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:172-189`;
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:266-321`).
`FrameInput::operator>>` accepts the known tag and seats the default variant,
and the network type-size/read switches also treat it as known
(`Projects/BrokenEngineSandbox/Source/Frame/FrameInput.cpp:33-54`;
`Engine/Source/Network/NetworkSerialization.cpp:149-159,343-358`).  A replay
record can therefore create a normal row with global ID zero, which no
ownership or fleet scan can address.

The source finding is recorded at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0048.md:53`
and the consolidated selector at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1134`.
All assigned source and authority hashes match frozen baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; the reserved-tag admission is
pre-existing, unresolved, and outside the audit work.

Impact: a structurally readable replay can add unowned player rows, and a
carried difference can accumulate them across ticks, changing authoritative
population and potentially exhausting paired collection capacity.

## Design

Author's recommendation: reject `kRespawnPlayer` as an unsupported status at
the replay `FrameInput` read/validation boundary before seating or consuming a
spawn variant, and ensure the player spawn hook cannot materialize the reserved
tag on any other accepted status path.  Route the failure through the existing
corrupt-replay/status recovery; retain the append-only tag values and valid
`kSpawnPlayer` ID/fleet behavior.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.cpp:33-54` — replay status validation and variant seating.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:266-321` — spawn status consumer.
- `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:172-189` — reserved tag and variant contract.
- `Engine/Source/File/Replay.cpp:994-1038` — replay difference input admission.
- `Engine/Source/Network/NetworkSerialization.cpp:149-159,343-358` — known status arm evidence.

## In scope

- Semantic rejection of reserved `kRespawnPlayer` records before any Player
  row, global ID, fleet, or CRC state is created.
- Consistent rejection in the replay/status consumer path while preserving the
  existing supported `kSpawnPlayer` and transfer tags.
- Existing replay failure propagation and no-partial-row behavior.

## Out of scope

- Designing a future respawn payload, adding a new status tag, changing wire
  field sizes, or changing `FrameInput::kiVersion` without a proven layout
  change.
- Fleet ownership policy, global-ID generation, ordinary spawn/destroy
  behavior, or unrelated status variants.
- Repairing a reserved record by assigning a synthetic ID.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3).  Trigger: versioned
replay/status input feeds deterministic Player creation and stable global-ID
ownership.

Preserve these invariants:

- Every accepted Player spawn status carries a real stable global ID before the
  row enters simulation, ownership, fleet, or transfer logic.
- Unsupported/reserved status records fail before collection mutation and use
  the current replay recovery path.
- Valid status tags, frame CRC, replay stream layout, and wire compatibility
  remain unchanged.

Tier rationale: the fix rejects one reserved tag at the existing replay
validation boundary and routes it through the current corrupt-replay recovery,
exactly as the Design specifies. Tag values, field sizes, stream layout, and
every supported status keep their present behavior, so only unusable input is
affected.

## Acceptance criteria

- A structurally valid current-version replay containing one
  `kRespawnPlayer` record aborts/rejects before `PlayersPostRender::Spawn` adds
  a row; no global-ID-zero row remains.
- A carried difference containing the reserved tag cannot add one row per
  replay tick, and valid `kSpawnPlayer` records still create owned players.
- Client and server `Debug|x64` builds clean through `/compile`; the replay
  scenario observes the established corrupt-input result rather than silent
  materialization.

## Notes

The report found no duplicate-family or external-claim dependency.  The
reserved tag is intentionally not treated as a future respawn feature in this
Plan.
