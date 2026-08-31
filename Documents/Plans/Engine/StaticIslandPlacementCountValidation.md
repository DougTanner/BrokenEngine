<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T02:00:05.521Z","dependsOn":[]} -->
# Reject empty island-placement lists before client adoption

## Context

Final survivor `S015-C010` is a retained HIGH client/game navigation finding. The island generator's contract places a dominant anchor at index zero, but the static-data reader accepts `iCount == 0`; client adoption keeps the empty list and skips derived-data construction (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp:316-367`; `Engine/Source/Frame/FrameStaticData.cpp:24-37`; `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSessionReceive.cpp:18-31`). Player navigation modes 4 and 5 then perform `at(0)` or modulo by `islands.size()`.

The final disposition for `S015-C010` is recorded at `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-021.md:53`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-015.md:170` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:258`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to require a nonempty placement list at the existing static-data read/receive boundary and reject an empty list through the existing corrupt-input path before client slot/frame adoption. Preserve the generator's index-zero anchor contract, valid placement ordering, derived-data skip rules for truly valid cases, and all valid navigation behavior.

## Critical files

- `Engine/Source/Frame/FrameStaticData.cpp:24-37` — static placement count read.
- `Engine/Source/Network/Client/ClientReceive.cpp:324-339` — static-data packet boundary (read-only receive evidence).
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSessionReceive.cpp:18-31` — client static-data adoption.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp:316-367` — modes 4/5 consumers.
- `Engine/Source/Frame/IslandChainPlacement.cpp:312-322` — generator anchor contract.

## In scope

- Empty-placement-list validation before save/network static-data adoption.
- Existing corrupt static-data failure propagation with no partial slot/frame mutation.
- Valid nonempty placement order, anchor, derived data, and Player navigation.

## Out of scope

- Island-template membership, area geometry, navigation topology, generator algorithm, or placement format/version.
- Adding a fallback island, changing modes 4/5, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped static-data admission behavior). Trigger: opaque network/static placement state controls deterministic Player navigation, but the correction is a local nonempty-count predicate with unchanged layout and valid behavior.

Preserve these invariants:

- Every adopted static-data placement list has a valid index-zero anchor for Player navigation.
- Empty malformed data is rejected before live slot/frame adoption and cannot reach modulo/`at` operations.
- Valid static data, placement ordering, generator output, and navigation random-draw behavior remain unchanged.

## Acceptance criteria

- A structurally valid static-data packet with zero placements is rejected before adoption and leaves the prior slot/frame state unchanged.
- A generated/nonempty packet applies normally; Player modes 4 and 5 continue consuming their existing random draws and selections.
- Client and server `Debug|x64` builds pass through `/compile`; a malformed-static-data scenario produces controlled rejection rather than a navigation exception.

## Coordination

`Documents/Plans/Engine/ClientStaticIslandReference.md` owns membership of placement CRCs, and `Documents/Plans/Engine/FrameAreaValidation.md` owns area geometry. Keep nonempty-count, membership, and area predicates distinct at the shared static-data boundary; preserve whole-packet rejection and slot immutability.

## Notes

Origin: `S015-C010`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-021.md:53`, source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-015.md:170`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:258`. No exact existing Plan was found. No source fix or build was performed during routing.
