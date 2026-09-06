<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:50:14.561Z","dependsOn":[]} -->
# Reject duplicate owner GUID records in staged fleet saves

## Context

The accepted finding `CAI/shard-0054/003` identifies an owner-map identity gap.
`ReadFleetData` reads each serialized owner GUID and fleet vector, then uses
`insert_or_assign` (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:140-170`).
Two complete records with the same `ClientGuid` therefore leave only the later
fleet vector in staged state.  Stream/count checks still pass, and
`GridSave` adopts the reduced map (`Engine/Source/File/GridSave.cpp:102-157,160-166`).
The valid writer emits each unordered-map key once, but that producer property
does not validate a damaged or hand-authored save at the reader boundary.

The source finding is recorded at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0054.md:98`
and the consolidated selector at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1197`.
Assigned source and authority hashes match frozen baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; the duplicate-key overwrite is
pre-existing, unresolved, and outside the audit work.

Impact: a successful load can silently discard one owner's persisted fleets
before relink or FleetSync, violating save round-trip preservation.

## Design

Author's recommendation: require each owner-map insertion during
`ReadFleetData` to report success and reject a duplicate `ClientGuid` through
the existing post-header corrupt-save path before staged state is adopted.
Preserve distinct owner records, empty fleet vectors, valid writer ordering,
and isolated staging; do not merge or choose a winner for duplicate input.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:106-175` — valid writer and staged owner-map reader.
- `Engine/Source/File/GridSave.cpp:102-166` — isolated save adoption.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp` — owner-map consumers (read-only identity evidence).
- `Engine/Source/File/AGENTS.md` and `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md` — staged save and persistent-owner contracts.

## In scope

- Duplicate `ClientGuid` detection while reading staged Fleet owner records.
- Existing corrupt-save failure propagation before `mFleets` is adopted, with
  no partial map publication.
- Valid owner-map writing, distinct Fleet vectors, and reconnect/relink state
  after successful valid loads.

## Out of scope

- FleetGuid emptiness or uniqueness, flagship-index relation, reconnect sync,
  owner takeover, or client FleetSync format.
- Repairing duplicate records, merging fleet vectors, changing map order, save
  layout/version, or adding a compatibility mode.
- Duplicate coordinates in the outer grid save, which is owned by
  `Documents/Plans/Engine/GridSaveDuplicateCoord.md`.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3).  Trigger: opaque
serialized owner identity controls staged persistent Fleet state and subsequent
network relink.

Tier rationale: the fix is mechanical and pre-specified — replace the
discarding `insert_or_assign` with an insertion whose result is checked, and
fail through the existing corrupt-save path. Valid saves with distinct owner
GUIDs keep identical layout, staging, and adoption behavior.

Preserve these invariants:

- Each successful staged save contains one owner-map entry per serialized
  `ClientGuid`, and every Fleet vector survives adoption.
- Duplicate input fails before live manager replacement or FleetSync; valid
  distinct owners retain current behavior and map semantics.
- Save/replay/wire formats and simulation CRC remain unchanged for valid data.

## Acceptance criteria

- A structurally valid save with two complete owner rows sharing one
  `ClientGuid` is rejected before adoption, without retaining either partial
  winner in live `mFleets`.
- A save with distinct owner GUIDs, including an owner with an empty Fleet
  vector, still loads and relinks every Fleet.
- Server `Debug|x64` builds clean through `/compile`; reconnect/load evidence
  proves no fleet disappears silently after valid adoption.

## Coordination

`Documents/Plans/Game/FleetFlagshipIndexValidation.md` and
`Documents/Plans/Game/FleetGuidSentinelValidation.md` share the staged Fleet
read boundary but own different invariants.  Keep owner-key insertion checking
independent, preserve the common corrupt-save path, and re-derive line ranges
before implementation.

## Notes

The consolidated index records no duplicate-family or external-claim hint for
this nested owner-map key overwrite.
