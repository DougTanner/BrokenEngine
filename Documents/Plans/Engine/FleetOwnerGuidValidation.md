<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T02:00:08.847Z","dependsOn":[]} -->
# Reject empty owner ClientGuids in staged fleet saves

## Context

Final survivor `S016-C004` is a retained HIGH server persistence finding. `ReadFleetData` constructs an owner `ClientGuid` from opaque save bytes and inserts it into staged maps without an emptiness check (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:152-170`). An all-zero key can be adopted, but an empty Hello GUID is replaced with a newly minted nonempty UUID before acceptance, so no reconnect or fleet lookup can address that owner row.

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-022.md` under `S016-C004 — FINAL: RETAIN_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-016.md:188` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:266`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to reject an empty owner `ClientGuid` while reading staged Fleet owner records, through the existing corrupt-save path before `mFleets` adoption. Preserve valid nonempty owners, disconnected-owner retention, fleet vectors, reconnect/relink, and FleetSync behavior.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:152-170` — owner GUID read and staged insertion.
- `Engine/Source/File/GridSave.cpp:102-166` — isolated save staging/adoption.
- `Engine/Source/Network/Server/ServerReceive.cpp:325-335` — empty Hello GUID minting (read-only identity evidence).
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp:26-34,369-390` — owner-key consumers.
- `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md` — persistent owner/reconnect contract.

## In scope

- Empty/all-zero owner `ClientGuid` validation in staged Fleet save input.
- Existing corrupt-save failure before live fleet-manager adoption.
- Valid nonempty owner records, disconnected retention, reconnect lookup, and FleetSync.

## Out of scope

- Duplicate owner keys, FleetGuid emptiness, flagship indices, fleet/member count caps, owner takeover, or GUID generation policy.
- Save layout/version changes, aliasing an empty owner to a new client, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped staged Fleet identity validation). Trigger: opaque serialized owner identity controls persistent server Fleet state and reconnect lookup, but the correction is a local reader predicate with unchanged save layout and valid adoption.

Preserve these invariants:

- Every adopted Fleet owner key is a nonempty persistent `ClientGuid` addressable by reconnect/lookup policy.
- Invalid empty-owner records fail before live map replacement or FleetSync publication.
- Valid nonempty/disconnected owner records, Fleet vectors, save format, and CRC behavior remain unchanged.

## Acceptance criteria

- A header-valid save containing an all-zero owner `ClientGuid` is rejected before fleet adoption through the existing corrupt-save path.
- Valid nonempty owners, including disconnected owners and empty Fleet vectors, load and reconnect exactly as before.
- Server `Debug|x64` builds pass through `/compile`.

## Coordination

`Documents/Plans/Engine/FleetOwnerGuidUniqueness.md` owns duplicate owner-key insertion, `Documents/Plans/Engine/FleetGuidSentinelValidation.md` owns per-Fleet identity, and `Documents/Plans/Engine/FleetCountBoundsValidation.md` owns policy ceilings. Keep owner emptiness independent while sharing staged-read rejection and isolated adoption.

## Notes

Origin: `S016-C004`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-022.md` (`S016-C004 — FINAL: RETAIN_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-016.md:188`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:266`. No exact existing Plan was found. No source fix or build was performed during routing.
