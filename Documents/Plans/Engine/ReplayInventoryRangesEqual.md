<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T16:33:35.507Z","dependsOn":[]} -->
# Simplify replay inventory identity comparison

## Context

`Engine/Source/File/Replay.cpp`, `Replay::SaveLoadReplay`, constructs an expected inventory with `BuildExpectedReplayInventory(expectedManifest, false)`. After checking its size, the current indexed loop compares `eKind`, `uiCoordKey`, and `iActivationTick`, throwing the same identity error from three branches. The entries also contain `digest`, which deliberately does not participate: expected inventory construction skips hashing.

## Design

Recommendation: replace only that indexed loop with one `std::ranges::equal` call on `expectedManifest.inventory` and `manifest.inventory`, using a captureless predicate with parameters `const ReplayManifestInventoryEntry& rExpected` and `const ReplayManifestInventoryEntry& rActual`. Return the conjunction of equality for `eKind`, `uiCoordKey`, and `iActivationTick`, in that order. On false, throw `std::ios_base::failure("ReplayManifest inventory identity")` once.

Retain the preceding build/size condition and its `ReplayManifest inventory shape` exception unchanged. Retain generation-digest and artifact-digest validation after identity validation and live reset/adoption after all validation. Use surrounding continuation indentation. This C++20 ranges spelling fits the C++23 codebase and the existing ranges sort in this file; it expresses paired sequence equality without repeating iterator endpoints or manual index access.

## Critical files

- `Engine/Source/File/Replay.cpp`: `Replay::SaveLoadReplay` identity loop (currently lines 566–580); `ReplayManifestInventoryEntry`, `BuildExpectedReplayInventory`, and later digest/adoption paths are read-only evidence.
- `Projects/BrokenEngineSandbox/Source/Pch.h`: read-only configuration evidence for `kbDebugInput`.

## In scope

- The ordered inventory identity comparison in `Replay::SaveLoadReplay`, between the existing inventory-shape check and generation-digest calculation.

## Out of scope

- Sorting, duplicate detection, inventory construction, hashing, replay serialization, format versions, the later `initialCoords` cardinality check, and reset/adoption behavior.
- Other algorithm sites, helper extraction, whole-entry equality, tuple projections, new headers, validation rules, instrumentation, fixtures, and unit tests.
- Documentation or style-policy edits: no new contract or convention is introduced. Run the required documentation synchronization review during implementation; no AGENTS.md content change is expected.

## Risk and invariants

Tier 1: a local behavior-preserving expression change with no signature, format, or invariant change. The proximity to replay validation makes exact equivalence essential; changing accepted identities, diagnostic selection, hashing, or adoption ordering exceeds this Plan.

- Failed expected-inventory construction or unequal sizes still produces the shape error.
- Equal lengths with any unequal identity field still produces the identity error; matching identities are accepted regardless of digest values at this stage.
- Field comparison order remains kind, coordinate key, activation tick, with short-circuit evaluation.
- Identity failure precedes digest checks and live state replacement. No replay compatibility or CRC computation changes.

## Performance

The installed MSVC 14.51 STL `algorithm` implementation routes two sized vectors through constant-time counts and `_Equal_count` over unwrapped iterators. With a custom predicate it performs a forward scalar loop, returning on the first mismatch. The proposed predicate adds no allocation, hashing, materialization, or second traversal; the algorithm may retain an additional constant-time size comparison. No speedup is claimed.

`SaveLoadReplay` is inside `if constexpr (kbDebugInput)`, and the current Pch.h disables that constant in Profile and Release. Consequently Release output cannot demonstrate comparison-loop quality. Verify the active Debug path without changing configuration constants or enabling Release replay diagnostics.

## Verification

1. Before editing, retain the session baseline and a Debug server build's comparison disassembly. After editing, review the focused diff against the four invariants above. Trace unequal sizes, each of the three single-field mismatches, matching identities, and differing digest values through the source; these are reasoning checks, not new test cases. Confirm the surrounding checks and the final reset/adoption site remain unchanged.
2. Through `/compile`, build BrokenEngineSandbox server `Debug|x64` for active-path inspection and `Release|x64` for configuration coverage. This source-only change uses Shared data mode. Recheck the active toolchain's ranges-equal implementation and compare the Debug comparison disassembly with baseline: require one traversal, no added allocation or hashing, and no material added per-entry work. Do not treat the absent Release loop as performance evidence or assert identical machine code. If the check reveals material overhead, report the failed acceptance criterion before landing.

## Acceptance criteria

- One ranges comparison and one identity throw replace the indexed loop; the predicate names exactly the three identity fields and excludes `digest`.
- The shape diagnostic, identity diagnostic, digest-validation sequence, and live adoption sequence are unchanged.
- Required builds pass; active-path performance inspection supports no material regression.
- Only the stated Replay.cpp region changes; no documentation or style-policy addition is needed.
