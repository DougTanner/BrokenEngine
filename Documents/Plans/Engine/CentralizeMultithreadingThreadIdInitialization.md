<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T17:51:15.604Z","dependsOn":[]} -->
# Centralize Multithreading construction-thread initialization

## Context

`common::Multithreading` captures its constructing thread in `mMainThreadId`. Both constructors in `Common/Threading/Multithreading.cpp` repeat `: mMainThreadId(std::this_thread::get_id())`; the private declaration in `Multithreading.h` has no initializer. `IsMainThread()` compares the current thread ID with this stored identity. This is a concrete opportunity to apply a C++11 default member initializer and keep the shared construction invariant in one place.

Source inspection at baseline `d29fed456d3ede935c5e672f95f13d6733f0660c` confirms that `mMainThreadId` precedes `mWorkers` and `mbDispatchActive` in declaration order. Both constructor bodies create workers only after member initialization. The primary constructor asserts and assigns `gpMultithreading`; the named-pool constructor leaves that singleton alone. No existing Plan owns this change (searched `Documents/Plans` for `Multithreading`, `mMainThreadId`, and `member initializ`).

## Design

Change the private declaration to `std::thread::id mMainThreadId = std::this_thread::get_id();`. Remove the complete `: mMainThreadId(std::this_thread::get_id())` line from each constructor definition, leaving their signatures and bodies intact. Neither constructor retains a member-initializer list after this removal.

The default member initializer executes once per construction, on the constructing thread, at the same declaration-order point as the existing explicit initializers. This preserves the value and timing without adding allocation, synchronization, or runtime work. It does not change object layout, public signatures, or deterministic frame state.

`Documents/C++StyleGuide.txt` rule 36 already directs class-member initialization into headers. `Common/Threading/AGENTS.md` remains accurate. No documentation or style-rule amendment is warranted; no broader initialization policy is proposed.

## Critical files

- `Common/Threading/Multithreading.h`: `common::Multithreading::mMainThreadId`; `IsMainThread()` and member declaration order provide verification context.
- `Common/Threading/Multithreading.cpp`: `Multithreading(int64_t)` and `Multithreading(Threads, int64_t, int64_t)` member-initializer lists.

## In scope

- Add the exact default member initializer above to `mMainThreadId`.
- Remove its identical initializer from both constructor definitions.
- Review the two constructors and unchanged `IsMainThread()` to verify construction-thread identity and ordering remain equivalent.

## Out of scope

- Constructor bodies, singleton ownership, worker creation, dispatch, synchronization, destruction, signatures, member ordering, and member types.
- Other initializers or classes, including `PersistentWorker` and `StaticVoice`; repository-wide initialization sweeps.
- New abstractions, comments, documentation/style amendments, tests, format changes, and performance instrumentation.

## Risk and invariants

Future implementation risk: **Tier 1**, a local behavior-preserving relocation of an identical initializer with no public-signature or invariant exposure. The threading subsystem location alone does not change threading behavior. If implementation requires changing identity capture timing, ownership, synchronization, or worker lifecycle, stop and reclassify that enlarged work rather than include it here.

- Each constructor captures its own constructing thread exactly once before later members and the constructor body.
- `IsMainThread()` has identical results for every caller.
- Only the primary constructor asserts and assigns `gpMultithreading`; the named pool remains independent.
- Worker counts, workbuffer arguments, exception behavior, layout, and dispatch semantics remain unchanged.
- No additional runtime work, allocation, or synchronization is introduced.

## Acceptance criteria

1. The header has exactly one `mMainThreadId` initializer, using `std::this_thread::get_id()`; neither constructor overrides it.
2. The implementation diff consists solely of that declaration edit and removal of the two identical initializer lines.
3. Review establishes unchanged declaration-order initialization and all invariants above.
4. The affected BrokenEngineSandbox client and server targets compile successfully through `/compile`.

## Verification

At implementation time, inspect the complete two-file diff and both construction paths. A search for `mMainThreadId` must show its declaration initializer and the unchanged comparison in `IsMainThread()`, with no constructor initializer remaining. Verify the member order and unchanged constructor bodies directly; this source proof settles the behavior and performance criteria.

Invoke `/compile` for the BrokenEngineSandbox client and server Release targets using the skill's supported invocation and data mode. Record actual build outcomes then; none have been run for this Plan. No `/agent-harness` run is required because this mechanical relocation has no runtime-observable behavior change and source equivalence is decisive. Do not add unit tests.
