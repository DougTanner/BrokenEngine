<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T02:00:12.252Z","dependsOn":[]} -->
# Enforce Fleet and member caps while reading saves

## Context

Final survivor `S016-C005` is a retained HIGH server persistence finding. Runtime policy caps each client at 16 Fleets and each Fleet at 16 members, but `ReadFleet` and `ReadFleetData` call only a remaining-byte count validator before resizing nested vectors (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:89-92,157-165`). A large structurally readable save can therefore adopt policy-invalid state and drive disproportionate allocation, loops, and FleetSync output.

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-022.md` under `S016-C005 — FINAL: RETAIN_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-016.md:202` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:267`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to apply the existing per-client Fleet and per-Fleet member ceilings while deserializing staged save data, before nested allocation and before live adoption. Reject an over-cap record through the existing corrupt-save path; preserve valid boundary counts, empty vectors, ordering, and FleetSync/reconnect behavior.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:89-92,157-165` — count reads and vector sizing.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp:16-19,63-67,144-149,318-325` — authoritative caps and request-side precedents.
- `Engine/Source/File/GridSave.cpp:102-166` — isolated save adoption.
- `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md` — save/replay Fleet bounds contract.

## In scope

- Maximum 16-Fleet-per-owner and 16-member-per-Fleet validation during staged save reads.
- Existing corrupt-save failure before nested over-cap allocation or live adoption.
- Valid empty, boundary-count, ordering, reconnect, and FleetSync behavior.

## Out of scope

- Flagship relation, FleetGuid/owner identity, duplicate keys, network request limits, or FleetSync wire redesign.
- Raising/changing runtime caps, partial truncation/repair, save layout/version changes, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped staged Fleet count validation). Trigger: opaque save counts control persistent server Fleet allocation/publication, but the correction reuses existing policy constants and failure handling without changing valid layout or protocol.

Preserve these invariants:

- Adopted save state never exceeds the current 16/16 Fleet/member policy.
- Over-cap input fails before oversized nested allocation or live Fleet publication.
- Valid empty and boundary counts, ordering, reconnect, FleetSync, save layout, and CRC behavior remain unchanged.

## Acceptance criteria

- A save with 17 members in one Fleet or 17 Fleets for one owner is rejected before staged adoption and follows the existing corrupt-save path.
- Saves at zero and exactly 16 members/Fleets load, reconnect, and synchronize normally.
- Server `Debug|x64` builds pass through `/compile`.

## Coordination

`Documents/Plans/Game/FleetFlagshipIndexValidation.md`, `Documents/Plans/Game/FleetOwnerGuidValidation.md`, `Documents/Plans/Game/FleetGuidSentinelValidation.md`, and `Documents/Plans/Game/FleetOwnerGuidUniqueness.md` own separate staged Fleet predicates. Keep count ceilings independent while sharing the common corrupt-save failure and isolated adoption boundary.

## Notes

Origin: `S016-C005`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-022.md` (`S016-C005 — FINAL: RETAIN_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-016.md:202`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:267`. No exact existing Plan was found. No source fix or build was performed during routing.
