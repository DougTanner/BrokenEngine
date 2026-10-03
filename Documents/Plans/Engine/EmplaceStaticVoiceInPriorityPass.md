<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:32:12.303Z","dependsOn":[]} -->
# Construct the new static voice directly in its vector

## Context

`StaticVoices::PriorityPass` in `Engine/Source/Audio/StaticVoices.cpp` currently calls `mVoices.push_back(StaticVoice(...))` and then obtains `mVoices.back()` as `rNewVoice` (baseline `d29fed456d3ede935c5e672f95f13d6733f0660c`, lines 448–449). Direct emplacement expresses construction and immediate use in one statement and removes an unnecessary temporary, move, and moved-from destructor. No measured speedup is claimed.

`StaticVoice::StaticVoice` initializes the voice fields and calls `SetVolume(0.0f)` followed by `Start()`. Its move constructor delegates to move assignment, which transfers fields and the source pointer and nulls the moved-from pointer. Its destructor only asserts that pointer is null (`Engine/Source/Audio/StaticVoice.cpp`, lines 65–119). The insertion arguments are local values, not references into `mVoices`. `StaticVoices::Init` reserves `kiMaxStaticVoices + kiMaxFadeOutPool`, and `PriorityPass` enforces the primary cap before insertion. The returned reference is used before any subsequent insertion.

## Design

Replace the two statements with exactly:

```cpp
StaticVoice& rNewVoice = mVoices.emplace_back(pVoice, id, fSoundVolume, fPitch, fFadeOutTime, vecPosition, vecVelocity, uiCrc);
```

Retain the surrounding acquisition, null check, pooled-voice fade initialization, `Apply3dVolume` call, and activation flag update. The same public constructor executes once for the same eight arguments. Its XAudio2 calls and their order remain identical. Direct construction places the acquired source pointer into its final owning vector entry without the temporary ownership transfer.

Style rule 53 already covers emplacement and reserving vector capacity; the existing reserve remains sufficient. No style-guide or AGENTS.md amendment is warranted because this changes no contract or convention.

## Critical files

- `Engine/Source/Audio/StaticVoices.cpp`: change only the new-voice insertion and `rNewVoice` initialization in `StaticVoices::PriorityPass`; inspect `StaticVoices::Init` for capacity evidence.
- `Engine/Source/Audio/StaticVoice.h` and `Engine/Source/Audio/StaticVoice.cpp`: read-only constructor, move, and destruction evidence.
- `Engine/Source/Audio/AGENTS.md`: audio presentation and ownership contracts.

## In scope

- Combine the `push_back(StaticVoice(...))` and following `back()` statements in `StaticVoices::PriorityPass` into the typed reference-returning emplacement above.
- Verify unchanged ownership, capacity, and same-pass mixing behavior from the concrete call site and existing constructor/move/destructor bodies.

## Out of scope

- Other insertions, containers, emplacement sweeps, constructor or move/destructor changes, and audio lifecycle fixes.
- Changes to vector reserve or caps, voice pooling, fade behavior, spatialization, XAudio2 calls, error handling, simulation state, or audio instrumentation.
- New tests, benchmarks, documentation/style amendments, or repository-wide feature adoption rules.

## Risk and invariants

Future implementation is Tier 1: one local behavior-preserving construction change, with no public signature or invariant exposure.

- The same source pointer is owned by the new `StaticVoice` entry; no additional release, destruction, `Start`, or `SetVolume` occurs.
- Constructor arguments and all subsequent fade, mix, and activation operations retain their values and ordering.
- Existing reservation and cap enforcement continue to prevent growth beyond reserved capacity during the main loop. No argument aliases vector storage, and `rNewVoice` is not retained across another insertion.
- No runtime allocation, storage, branch, or work is added. Removed temporary/move/destructor work is the only intended construction-path difference; no performance benchmark is required for this mechanically established property.

## Acceptance criteria and verification

| Criterion | Required future evidence |
|-----------|--------------------------|
| Exact minimal application | Diff shows only the two insertion/reference statements replaced by the specified single statement; eight arguments remain unchanged. |
| Audio ownership and playback behavior preserved | Inspect the public constructor and move/destructor bodies against the changed insertion: final fields and `mpVoice` match, constructor XAudio2 calls still run once, and removed temporary destruction performs only its null assertion. Confirm pooled fade initialization, `Apply3dVolume`, and activation flag order are unchanged. |
| No added runtime overhead or invalidation | Confirm existing reserve and cap paths, local argument provenance, and reference use before next insertion. The diff adds no allocation, helper, extra traversal, or other operation. |
| Supported client construction | Run `/compile` for the BrokenEngineSandbox client Debug target and record the result; it must compile and link successfully. No server build is needed for this client-only implementation-file change. |

Perform the repository's triggered C++ review and cleanup steps for the implementation. No `/agent-harness` run is required: source inspection decisively proves this local construction substitution preserves audio ownership and calls, and a live playback check would add no evidence about the removed temporary. If inspection finds changed ownership semantics or added work, resolve that within this scope before acceptance rather than claiming equivalence.

These checks are requirements for future implementation; they have not been run as part of writing this Plan.
