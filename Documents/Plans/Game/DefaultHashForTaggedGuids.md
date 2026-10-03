<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:46:36.880Z","dependsOn":[]} -->
# Default hashing for tagged GUIDs

## Context

`Engine/Source/Network/NetworkProtocol.h:88-109` defines the program-defined `Guid128<TAG>`, its two-word equality, a stateless `Guid128Hash<TAG>`, and the `ClientGuidHash` alias. Server fleet maps repeat the explicit hasher in members and public function declarations/definitions. Moving this existing operation into `std::hash<engine::Guid128<TAG>>` removes that repeated policy argument with the same hash computation. This is a narrow use of standard-library specialization for a program-defined key, consistent with `Engine/Source/Frame/Collections/CollectionId.h:90-109`.

Evidence was inspected against baseline `d29fed456d3ede935c5e672f95f13d6733f0660c`. A search of existing executable Plans found no ownership of `Guid128`, `Guid128Hash`, `ClientGuidHash`, or `FleetGuidHash`. This Plan contains its own rationale and does not depend on an investigation document.

`Projects/BrokenEngineSandbox/Source/Fleet.h:10` also declares `FleetGuidHash = engine::Guid128Hash<FleetGuidTag>`; it has no consumers in the inspected source. Removing the generic hasher therefore requires removing this unused alias as well. `FleetGuid` itself remains tag-distinct and unchanged.

## Design

1. Remove `Guid128Hash<TAG>` and `ClientGuidHash` from `NetworkProtocol.h`. Add `template <typename TAG> struct hash<engine::Guid128<TAG>>` in `namespace std`, after the first closing `namespace engine` and before `#include "Network/NetworkMessages.h"`. This location follows the complete GUID definition and makes the specialization visible to downstream consumers before any container instantiation. Preserve aggregation and client/server availability; do not hide the specialization behind `BT_SERVER`.
2. The specialization has exactly one public operation, `size_t operator()(const engine::Guid128<TAG>& rGuid) const`, with the existing body:

   ```cpp
   return std::hash<uint64_t>{}(rGuid.uiHigh) ^ (std::hash<uint64_t>{}(rGuid.uiLow) << 1);
   ```

   Preserve the absence of `noexcept`: do not copy the stronger exception specification from the CollectionId precedent. Add no state, base class, constructor, transparency alias, attributes, or alternate mixing function.
3. Remove the unused `FleetGuidHash` alias in `Fleet.h`. Replace every `std::unordered_map<engine::ClientGuid, std::vector<Fleet>, engine::ClientGuidHash>` with `std::unordered_map<engine::ClientGuid, std::vector<Fleet>>`, and every analogous `int64_t` mapped type with `std::unordered_map<engine::ClientGuid, int64_t>`. Change declaration and definition together; keep reference/const qualifiers, names, other parameters, mapped types, equality, and allocators unchanged.
4. Leave map operations and function bodies unchanged. In particular, preserve `WriteFleetData`'s owner sort by `uiHigh` then `uiLow`, fleet vector order, RNG serialization, and `ReadFleetData` insertion sequence and validation. Preserve fleet traversal and associated RNG consumption in `TickFleetTimers` and pending-update processing.

Changing a hasher type is not by itself a portable guarantee of identical unordered-container iteration. The supported MSVC implementation must be checked, rather than assuming identical hash values settle this. The inspected VS 2026 MSVC `14.51.36231` headers route default and explicitly named hashers through the same `_Uhash_compare` in `unordered_map`; `xhash` computes `_Nothrow_hash` from the call expression and stores the hasher in `_Compressed_pair`. The proposed specialization preserves these relevant properties. Future implementation must reconfirm the installed implementation's insertion/rehash paths have no distinguishing hasher-type dispatch; do not introduce sorting or a wrapper policy to compensate for a discovered difference. A difference is an acceptance failure requiring the proposal to be reconsidered.

## Critical files

| File | Required change |
| --- | --- |
| `Engine/Source/Network/NetworkProtocol.h` | Relocate GUID hashing into the visible `std::hash` specialization; remove old generic hasher and client alias. |
| `Projects/BrokenEngineSandbox/Source/Fleet.h` | Remove the now-obsolete unused `FleetGuidHash` alias only. |
| `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.h` | Default hashing for `mFleets` and `mGuidToClientId`. |
| `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.h` | Default hashing for `SaveStagedState::fleets`, `SaveStagedState::guidToClientId`, and `WriteFleetData`/`ReadFleetData` declarations. |
| `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp` | Matching `WriteFleetData`/`ReadFleetData` definitions. |
| `Projects/BrokenEngineSandbox/Source/Network/Server/FleetNavigationController.h` | Matching `TickFleetTimers`/`ProcessFlagshipUpdates` declarations. |
| `Projects/BrokenEngineSandbox/Source/Network/Server/FleetNavigationController.cpp` | Matching `TickFleetTimers`/`ProcessFlagshipUpdates` definitions. |

## In scope

- The exact hasher relocation and alias removals above.
- The listed ClientGuid map members, staged-state members, and public parameter types, updated consistently across declarations and definitions.
- Review of supported STL behavior and the header visibility needed to establish equivalent container behavior.

## Out of scope

- GUID representation, equality, tags, construction, generation, or existing static assertions.
- Hash arithmetic changes, `noexcept` strengthening, map aliases, container replacements, sorting changes, or allocation changes.
- Fleet navigation algorithms, RNG draws, serialization bodies, wire/save/replay formats, version constants, and trust checks.
- The hasher for `std::tuple<int, int, bool>` or unrelated hashers; that tuple has no program-defined key type.
- Repository-wide standard-library adoption policy, new tests, new fixtures, and unrelated cleanup.

## Risk and invariants

Future implementation is **Tier 3**: the shared engine Network identity header and independently owned game fleet APIs change together, and fleet iteration feeds RNG-sensitive navigation. This is not a Tier-1 local mechanical edit despite its small diff. The intended change preserves determinism, layout, and serialization; these remain acceptance requirements, not permission to change them. Follow the Tier-3 plan review and implementation workflow when this Plan is claimed.

- Both GUID words, default equality, tag separation, layout assertions, wire bytes, and persisted bytes remain unchanged. No compatibility version bump is warranted for this change.
- Hash values, empty/stateless construction, call exception specification, bucket behavior, insertion/rehash outcomes, and traversal results remain equivalent on the supported toolchain for the same keys and operation sequence.
- Live and staged map types match, including move adoption; function declarations match definitions in both build configurations.
- No added runtime work, allocation, indirection, or public engine dependency on game-only names.

## Acceptance criteria and verification

| Criterion | Required future evidence |
| --- | --- |
| Exact operation preserved | Diff confirms verbatim hash expression, `const` call operator without `noexcept`, no state, and unchanged GUID definitions/assertions. |
| Public specialization visible and types consistent | Inspect `Engine.h`/PCH inclusion and specialization placement before `NetworkMessages.h`; search all source for the three removed hasher names and confirm zero references. Inspect all listed map signatures and staged/live adoption. |
| Container traversal and cost preserved | Inspect supported MSVC `unordered_map`/`xhash` paths for hasher-type discrimination, noexcept dispatch, storage traits, insertion, and rehash. Record why old/new empty hashers with the same expression and exception specification take identical paths. A hash-value comparison alone does not pass. |
| Serialization and RNG behavior preserved | Diff confirms no body changes to serialization/navigation, preserving sorted owner output, vector order, insertion order, RNG state writes/reads, and fleet traversal. Combine this with the container-path proof above. |
| Both executables compile and link | Through `/compile`, build BrokenEngineSandbox client and server in Debug and Release. Shared header/PCH exposure requires both targets; DataPacker and ThirdParty rebuilds are not independently required by this scope. Record results, do not infer them from source inspection. |
| Minimal coherent final diff | Perform required C++ correctness, style, comment, affected-code and Tier-3 reviews; documentation synchronization review should confirm no contract text changed. |

No unit tests or new runtime fixture are authorized. No `/agent-harness` run is required when the source/STL proof settles unchanged traversal and serialization: the proposal changes types and visibility only. If that proof fails, runtime smoke success cannot substitute for preserving all affected behavior; report the unresolved acceptance failure rather than broadening implementation.

## Documentation and style

No amendment to `Documents/C++StyleGuide.txt` or an `AGENTS.md` is warranted: existing rules 32 and 41 permit the standard-library container and explicitly qualified names, and CollectionId already demonstrates this specialization pattern. Identity, serialization, and ownership contracts remain unchanged. Do not add explanatory comments that merely narrate the relocation.

## Execution status

Plan authoring only. No C++ edits, builds, runtime checks, or future implementation reviews have run as part of writing this Plan.
