<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:50:09.611Z","dependsOn":[]} -->
# Reject out-of-range flagship indices in staged fleets

## Context

The accepted finding `CAI/shard-0054/002` identifies a fleet save trust-boundary
gap.  `ReadFleet` rejects a negative `iFlagshipIndex` but accepts an index at or
above the member count (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:55-70`).
The later count/byte checks do not relate the index to the vector
(`:89-103`), so `ReadFleetData` stages and adopts an unusable fleet through the
grid-save path (`:140-175`; `Engine/Source/File/GridSave.cpp:102-157`).
`ResetFleetForLoad`, timer drains, and flagship updates skip the out-of-range
index, while the client FleetSync reader rejects the same nonempty record
(`Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp:438-455`;
`FleetNavigationController.cpp:39-47,132-135`;
`Projects/BrokenEngineSandbox/Source/Network/GameMessages.h:127-168`).

The source finding is recorded at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0054.md:80`
and the consolidated selector at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1188`.
Assigned source and authority hashes match frozen baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; the missing relation check is
pre-existing, unresolved, and outside the audit work.

Impact: a header-valid save can report success while disabling server flagship
navigation and causing the owner to discard its FleetSync.

## Design

Author's recommendation: after reading a Fleet's member count and flagship
index, require `0 <= iFlagshipIndex < iMemberCount` for nonempty fleets and
require exactly index `0` for empty fleets.  Reject a violation through the
existing post-header corrupt-save/fresh-state failure path before staged Fleet
state is adopted or synchronized; preserve valid reset, navigation, and sync
behavior.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:55-103,140-175` — Fleet read and staged owner-map adoption.
- `Engine/Source/File/GridSave.cpp:102-157` — save staging/adoption boundary.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp:438-455` — load reset consumer.
- `Projects/BrokenEngineSandbox/Source/Network/Server/FleetNavigationController.cpp:39-47,132-135` — timer/update consumers.
- `Projects/BrokenEngineSandbox/Source/Network/GameMessages.h:127-168` — client FleetSync predicate.

## In scope

- Semantic flagship-index validation against each deserialized Fleet's member
  count before `ReadFleetData`/grid-save adoption succeeds.
- Existing corrupt-save failure propagation and preservation of valid empty and
  nonempty Fleet navigation/sync state.
- The Fleet serialization/read and direct load-reset consumer boundary named
  above.

## Out of scope

- FleetGuid emptiness, duplicate owner GUIDs, reconnect publication, member
  ordering, flagship-selection policy, or client decoder redesign.
- Repairing an invalid index by selecting a different member, changing save
  layout/version, or changing valid FleetSync wire fields.
- Navigation timers, death rotation, and spawn assignment except as consumers
  proving the malformed state is no longer adopted.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3).  Trigger: opaque
serialized Fleet state enters server/client ownership, deterministic
navigation, and staged save adoption.

Tier rationale: the Design fully specifies a single bounds relation
(`0 <= iFlagshipIndex < iMemberCount`, or exactly zero when empty) inside one
reader function, rejected through the existing post-header corrupt-save path.
Valid saves keep their exact layout, adoption, navigation, and sync behavior.

Preserve these invariants:

- Every adopted nonempty Fleet names exactly one member as flagship; an empty
  Fleet retains its canonical index zero.
- Invalid save input fails before live Fleet state or FleetSync publication;
  valid navigation, death rotation, and wire behavior remain unchanged.
- Persistent owner/Fleet identity, replay, save format, and CRC behavior remain
  compatible for valid data.

## Acceptance criteria

- A structurally valid save with a nonempty Fleet index equal to or greater than
  its member count is rejected before adoption and follows the existing
  post-header reset/fresh-state path.
- An empty Fleet with index zero and a nonempty Fleet with an in-range index
  still load, refresh, navigate, and synchronize normally.
- Server `Debug|x64` builds clean through `/compile`; load and reconnect
  scenarios never publish a FleetSync record the client rejects.

## Coordination

`Documents/Plans/Engine/FleetGuidSentinelValidation.md` and
`Documents/Plans/Engine/FleetOwnerGuidUniqueness.md` also validate
`ServerFleetSerialization.cpp`.  Keep flagship/member relation, FleetGuid
sentinel, and owner-map key uniqueness as separate predicates at the same
staged-read failure boundary, and re-derive line ranges before implementation.

## Notes

The consolidated index records no duplicate-family or external-claim hint for
this flagship-index relation.
