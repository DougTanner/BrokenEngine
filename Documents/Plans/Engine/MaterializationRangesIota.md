<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:24:56.665Z","dependsOn":[]} -->
# Use ranges::iota for materialization indices

## Context

Inventory F080 covers C++23 `ranges::iota`, `ranges::shift_left`, and `ranges::shift_right`. The bounded adoption is the whole-range `iota` overload in `BuildMaterializationInventory`, `DataPacker/Source/FileManager.cpp:251-287`, inspected at baseline `6c6245f4b1dee82d7721ef271ce4925ca754ff47`. `MaterializationInventory::order` is a `std::vector<size_t>` (line 247); lines 281-282 resize it to the file count and fill it with `std::iota(inventory.order.begin(), inventory.order.end(), 0)`. Naming the complete destination directly makes whole-container intent explicit. This is a small clarity improvement, with no performance claim or existing defect asserted.

The following sort already establishes relative-path order. A focused first-party source search found no shift algorithm calls to improve. Searches of live Plans for the symbols, `iota`, and materialization-index filling found no duplicate implementation scope.

## Design

The author's recommendation is to replace only the fill statement with:

```cpp
std::ranges::iota(inventory.order, 0);
```

Keep the existing resize, vector element type, literal `0`, sort, and comparator. The literal continues to deduce an `int` counter, converted to `size_t` on assignment; changing the pre-existing counter limit is outside this refactor. Both forms assign successive integers to the same live vector elements and increment the same scalar counter. For an empty range neither writes anything. The new algorithm's result is discarded, so no iterator or counter escapes. There is no added allocation or lifetime extension.

[WG21 P2440R1 section 4.1](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p2440r1.html#rangesiota-1) specifies the range overload in `<numeric>`, its assignment/increment loop, and its iterator/value result. `Common/ExternalHeaders.h:92` already includes `<numeric>`; `DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj:177,213` already selects `stdcpp23`. The targeted build verifies support on the configured toolchain without adding a fallback.

## Critical files

- `DataPacker/Source/FileManager.cpp` — the fill statement in `BuildMaterializationInventory` is the sole implementation edit.
- `DataPacker/Source/AGENTS.md` — materialization behavior and reproducibility contracts to preserve; no wording change is needed.
- `Documents/C++StyleGuide.txt`, rule 8 — existing iteration guidance suffices; it neither requires a ranges-algorithm migration nor prohibits this call. No rule amendment is justified by a single equivalent fill.

## In scope

- Replace the one `std::iota` call in `BuildMaterializationInventory` with the whole-range `std::ranges::iota` overload, preserving seed type and value.
- Review the focused diff and compile DataPacker as described below.

## Out of scope

- Other algorithms, shift adoption, a general ranges migration, helper extraction, include or project changes, and style-guide or AGENTS.md edits.
- Counter-width changes, filesystem traversal, allocation accounting, validation, copy-on-write staging/publication, and the sorting comparator.
- Asset generation, actual materialization, benchmarks, and unit tests.

## Risk and invariants

Future implementation is Tier 1: a local behavior-preserving substitution with no public signature or invariant exposure. Preserve file enumeration, index contents and sorted order, all allocation behavior, and all materialization failures. No simulation/CRC, threading, wire, save/replay, serialized layout, exported bytes, or format-version changes are involved. The existing signed-counter representability domain remains unchanged. No new compatibility code or toolchain configuration is proposed.

## Acceptance criteria

1. The only implementation difference is the specified fill call in `BuildMaterializationInventory`; the resized storage, seed, types, and sort remain identical.
2. Source inspection establishes the same empty-range behavior and successive integer writes for the existing defined input domain, with no retained result, new allocation, or lifetime change.
3. DataPacker `Release|x64` compiles and links successfully on the configured toolchain.
4. Documentation and style guidance remain unchanged because the owning contracts and rule 8 already cover the operation.

## Verification

Use the focused source diff to settle criteria 1, 2, and 4. Invoke `/compile` for DataPacker `Release|x64` to settle criterion 3, following its serialized build driver. The equivalence is established by scalar operation semantics and unchanged surrounding code; no asset generation, materialization run, game launch, or new test is necessary. Apply the repository's triggered C++ review and documentation checks; their expected documentation outcome is no change.

## Notes

This is an executable recommendation for a later implementation session. It is self-contained and does not depend on an investigation inventory or temporary review receipt. There are no prerequisites or mandatory coordination constraints with other Plans.
