<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:53:05.418Z","dependsOn":[]} -->
# Generate spaceship registry row indices with std::iota

## Context

`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:496-544`, in `BuildSpaceshipRegistryWindow`, reserves one workbuffer block and then fills `pAscendingRows` with the integers from zero through `iAscendingCount - 1`. This standalone loop expresses sequential index generation. Replacing it with `std::iota` makes that purpose explicit without changing the generated values or adding allocations or passes.

The inspected baseline is `d29fed456d3ede935c5e672f95f13d6733f0660c`. `Common/ExternalHeaders.h:92` already includes `<numeric>` through the existing aggregation/PCH route; `DataPacker/Source/FileManager.cpp:282` already uses `std::iota`. No new include or helper is needed. A search of existing Plans found no owner for this fill loop; `CollectionIdMapIdentity.md` concerns a separate frame-read gate.

## Design

Replace only the standalone four-line ascending-row initialization loop with:

```cpp
std::iota(pAscendingRows, pAscendingRows + iAscendingCount, int64_t{0});
```

Keep the explicitly typed `int64_t` seed: an untyped zero would change the algorithm's incremented value type to `int`, unlike the existing `int64_t` loop counter. Keep the existing count, pointer, allocation, and consumer expressions intact.

This remains one allocation-free linear write pass over the same elements. No speedup is claimed; the change introduces no extra traversal, temporary container, or dispatch mechanism. The separate eligible-row loop must remain separate and unchanged.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp` — sole implementation file; `BuildSpaceshipRegistryWindow`, ascending-row initialization at baseline lines 528-531.
- `Common/ExternalHeaders.h` — read-only evidence that `<numeric>` is available.
- `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` — preserve deterministic acquisition ordering and the existing registry-window contract.

## In scope

- Replace only the `for (int64_t i = 0; i < iAscendingCount; ++i)` loop assigning `pAscendingRows[i] = i` in `BuildSpaceshipRegistryWindow` with the specified typed `std::iota` call.
- Verify the unchanged reservation and downstream row filtering establish equivalence for empty and nonempty ranges.

## Out of scope

- Changes to allocation size, alignment, lifetimes, collection counts, eligibility predicates, filtered row construction, registry acquisition, phase ordering, CRC, formats, or versions.
- Changes to `RegistryFixture.cpp`: its baseline lines 255-259 combine target-ID and index writes in one loop; splitting that loop would introduce an additional pass.
- Other sequential loops, `std::is_permutation` adoption, new helpers, includes, tests, benchmarks, or repository-wide adoption rules.
- Documentation or style-guide amendments: no contract, phase, format, or convention changes, so none is warranted. Existing loop and type conventions permit the replacement.

## Risk and invariants

Future implementation risk: Tier 1, triggered by a local behavior-preserving replacement with no signature or invariant exposure. The function participates in deterministic simulation, but the change must not alter anything it computes; any such alteration exceeds this Plan.

- `iAscendingCount` remains the nonnegative maximum of spaceship and subscriber collection counts.
- The combined reservation always includes `sizeof(engine::RegistrySourceLayer)`, even when `iAscendingCount` is zero. Preserve that valid allocation and the derived `pAscendingRows` pointer so the empty range uses valid equal endpoints without a null-pointer arithmetic assumption.
- For every index `i` in `[0, iAscendingCount)`, the written `int64_t` value remains `i`; a zero count performs no writes.
- The ascending order used by source/subscriber rows and deterministic ranking ties is unchanged. The following eligibility filtering retains its predicate, iteration order, counts, and writes.
- Workbuffer ownership, padding, scratch/layer boundaries, and pointer publication timing remain unchanged.

## Acceptance criteria

1. The scoped loop is replaced by exactly the specified `std::iota` expression with `int64_t{0}`; there are no other implementation changes.
2. A diff and source review establishes the same empty-range behavior, index values, deterministic ordering, allocation/pass count, and subsequent filtering, using the invariants above.
3. Both game configurations consuming `Frame.cpp` compile successfully under the existing PCH setup.

## Verification

During implementation, review the focused diff against each invariant and acceptance criterion. Reconfirm `<numeric>` remains supplied by `Common/ExternalHeaders.h` and no new include is necessary.

Use `/compile` to compile `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp` selectively for Client (`BrokenEngineSandbox`) and Server (`BrokenEngineSandboxServer`), both `Debug|x64`. This C++-only change has no asset, shader, packer, or runtime-data generation change; use Shared runtime data under the compile skill's mode-selection rules. No PREfast run is required.

No `/agent-harness` scenario is required: the exact range, seed type, and unchanged consumers settle behavior by inspection, and both consuming targets settle compile integration. Do not add unit tests. These are future required checks; this Plan's creation does not claim that they have passed.
