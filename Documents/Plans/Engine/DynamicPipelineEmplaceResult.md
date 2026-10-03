<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:24:39.492Z","dependsOn":[]} -->
# Use the inserted pipeline owner directly

## Context

`DynamicPipelines::AddPipeline` in `Engine/Source/Graphics/Managers/DynamicPipelines.cpp` currently appends a `std::unique_ptr<Pipeline>` with `push_back`, then obtains its raw pointer through a separate `mPipelines.back().get()` statement. The owning member in `DynamicPipelines.h` is `std::vector<std::unique_ptr<Pipeline>>`. C++17's reference return from `emplace_back` allows this existing create-then-use operation to express ownership acquisition and pointer retrieval together, removing one statement without changing the lifecycle.

## Design

Replace exactly these first two statements in `DynamicPipelines::AddPipeline`:

```cpp
mPipelines.push_back(std::make_unique<Pipeline>());
Pipeline* pPipeline = mPipelines.back().get();
```

with:

```cpp
Pipeline* pPipeline = mPipelines.emplace_back(std::make_unique<Pipeline>()).get();
```

Keep `pPipeline->Create(rPipelineInfo);` and `mPipelineMaps[eType].insert_or_assign(crc, pPipeline);` unchanged and in their current order. The returned vector-element reference is used immediately by `.get()`; only a pointer to the separately allocated `Pipeline` is retained.

## Critical files

- `Engine/Source/Graphics/Managers/DynamicPipelines.cpp`: the sole source edit, in `DynamicPipelines::AddPipeline`.
- `Engine/Source/Graphics/Managers/DynamicPipelines.h`: read-only confirmation of vector ownership and raw-pointer map types.
- `Engine/Source/Graphics/Managers/AGENTS.md`: read-only manager lifecycle contracts.
- `Documents/C++StyleGuide.txt`: read-only rule 15 governing explicit types.

## In scope

Only the insertion and `pPipeline` initialization statements at the start of `DynamicPipelines::AddPipeline`.

## Out of scope

Model pipeline creation, other insertion sites, container types or capacity policies, pipeline initialization, map registration, public signatures, comments, style policy changes, architecture changes, instrumentation, and unit tests.

## Risk and invariants

Tier 1: local behavior-preserving simplification with no public signature or invariant exposure.

- Preserve one `Pipeline` allocation, ownership by `mPipelines` before `Create`, and registration only after `Create` returns.
- Preserve the exact pointee identity registered in the map. Vector growth can relocate owners but cannot relocate their separately allocated pointees.
- Preserve failure sequencing: allocation failure prevents insertion; insertion failure releases the temporary owner; a `Create` failure leaves the appended owner in the vector and does not register it; map insertion failure still leaves vector ownership intact. Both insertion forms move the same noexcept-movable `unique_ptr` into vector storage.
- Preserve client-only compilation and all renderer recreation contracts. Simulation, CRC, serialization, data layout, and threading are untouched.

## Performance

The same allocation, unique-pointer move, and possible vector growth remain. `.get()` reads the newly inserted owner directly. This is a clarity improvement with equivalent runtime cost, not a measurable speedup claim; no benchmark or new instrumentation is needed.

## Documentation and style

Retain the explicit `Pipeline* pPipeline` declaration, consistent with style rule 15. No policy update is needed: the existing rule permits the expression, and the manager lifecycle documentation remains accurate. No new comment is useful for this ordinary library expression. Run the required documentation synchronization review during implementation and record no documentation delta for these unchanged contracts.

## Verification

Review the exact diff to establish that only the two statements become the specified single statement, with `Create` and registration unchanged. Confirm the vector still owns `unique_ptr<Pipeline>` and no vector-element reference escapes. Check the failure sequencing against the invariants above. Run `/code-style-review`, `/repo-code-review`, `/comment-review`, and the applicable Change Workflow checks for the C++ change. Use `/compile` to compile the affected BrokenEngineSandbox client translation unit through its documented driver; a full client build is acceptable if that is the supported invocation. No server build or runtime harness session is required for this local ownership-preserving expression change. Do not add unit tests.

## Acceptance criteria

1. `DynamicPipelines::AddPipeline` initializes `Pipeline* pPipeline` directly from `mPipelines.emplace_back(std::make_unique<Pipeline>()).get()`.
2. The vector acquires ownership before `Create`, and the same raw pointer is registered only after successful `Create`; failure sequencing is unchanged.
3. The source diff contains only the specified statement replacement, with no header, policy, comment, or unrelated insertion changes.
4. The affected client compilation succeeds and required reviews identify no unresolved in-scope defect.
