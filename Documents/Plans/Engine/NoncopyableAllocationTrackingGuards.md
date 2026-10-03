<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:14:40.429Z","dependsOn":[]} -->
# Make allocation-tracking guards noncopyable

## Context

`Common/AllocationTracking.h` defines two global-namespace RAII guards. `ScopedSuppressAllocationTracking` increments `giAllocationTrackingSuppressed` on default construction and decrements it on destruction; `ScopedResumeAllocationTracking` does the reverse inside an existing suppression scope. Both currently permit implicit copying. A copy construction would skip the counter adjustment while introducing another destructor, so copying does not represent their scope ownership correctly. No current copy misuse or resulting runtime bug was observed; this change encodes the existing ownership contract.

Source inspection at baseline `d29fed456d3ede935c5e672f95f13d6733f0660c` found ordinary local guard declarations across Engine and Projects, including the nested resume guard in `Engine/Source/Network/Server/ServerSessionRuntime.cpp:238`. `Common/Workbuffer.h` already uses deleted copy special members for RAII ownership. `Common/ScopedLambda.h` already has a noncopyable `std::move_only_function` member and needs no change. Searching existing Plans for allocation-tracking and guard-copy ownership found no duplicate proposal.

## Design

Add these public declarations to `ScopedSuppressAllocationTracking`, beside its existing constructor:

```cpp
ScopedSuppressAllocationTracking(const ScopedSuppressAllocationTracking&) = delete;
ScopedSuppressAllocationTracking& operator=(const ScopedSuppressAllocationTracking&) = delete;
```

Add the corresponding declarations to `ScopedResumeAllocationTracking`:

```cpp
ScopedResumeAllocationTracking(const ScopedResumeAllocationTracking&) = delete;
ScopedResumeAllocationTracking& operator=(const ScopedResumeAllocationTracking&) = delete;
```

Retain both existing default constructors explicitly, including their bodies. Keep both destructor bodies unchanged. Their user-declared destructors already suppress implicit move special members; deleted const-reference copy operations also reject rvalues, so separate move deletions are unnecessary. No runtime instructions or storage are added.

In `Common/AGENTS.md` under `## Allocation-Free Scratch`, add one sentence to the existing allocation-tracking paragraph stating that both guards are noncopyable and nonmovable, and each must remain within its owning scope on the constructing thread. This is a local ownership clarification; no global C++ style amendment or repository-wide special-member policy is warranted.

## Critical files

- `Common/AllocationTracking.h`: the two guard definitions.
- `Common/AGENTS.md`, `## Allocation-Free Scratch`: owning usage contract.

## In scope

- Delete the copy constructor and copy assignment operator of `ScopedSuppressAllocationTracking` and `ScopedResumeAllocationTracking` exactly as above.
- Add the single owning-documentation sentence specified above.

## Out of scope

- Counter behavior, guard placement, nesting semantics, thread handling, assertions, allocator overrides, or any call-site redesign.
- `ScopedLambda`, Workbuffer, other RAII types, generic ownership helpers, or a broader special-member sweep.
- Extra move declarations, runtime checks, members, tests, benchmarks, and global style-guide edits.

## Risk and invariants

Future implementation is Tier 2: it tightens the public construction/assignment interface of two Common types, exceeding Tier 1's no-public-signature-exposure condition. It does not change threading behavior, deterministic state, wire or serialized data, layout, or runtime counter operations; use across subsystems requires compile coverage, not cross-subsystem implementation changes.

- Each supported guard lifetime performs exactly its existing constructor/destructor counter pair on the same thread.
- Existing local default construction and destruction remain valid.
- Suppression/resume nesting, counter storage, object size, and hot-path work remain unchanged.
- No observed runtime bug is claimed, and no source outside the two named files is changed to force compilation.

## Acceptance criteria

1. Each guard has exactly the two deleted copy declarations shown above and retains its original default constructor and destructor bodies.
2. Neither guard is copy-constructible, copy-assignable, move-constructible, or move-assignable, as established by its declarations and special-member rules.
3. Existing call sites compile with unchanged scope lifetimes; no runtime instructions, state, or allocations are added.
4. The owning documentation states the noncopyable/nonmovable same-thread scope contract once, with no global style change.

## Verification

During future implementation, inspect the complete diff against the invariants and repeat the guard usage search to identify any newly introduced transfer attempt. A discovered transfer is an acceptance failure to resolve within the authorized scope, not permission for unrelated call-site redesign.

Use `/compile` for BrokenEngineSandbox Client and Server in `Debug|x64`, and DataPacker in `Release|x64`, to cover the runtime consumers and the Common aggregation header's offline consumer. Shared runtime data is appropriate because the change affects neither assets nor generated/serialized data contracts; no export is needed. Retain build evidence and require all three targets to pass. No unit tests, performance benchmarks, or `/agent-harness` runs are required: this is a compile-time ownership restriction, and unchanged constructor/destructor bodies decisively establish the runtime invariants. Apply the normal C++ and owning-documentation reviews during implementation. None of these future checks has been run as part of authoring this Plan.
