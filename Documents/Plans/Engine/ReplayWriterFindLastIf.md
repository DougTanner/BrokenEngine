<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:26:16.401Z","dependsOn":[]} -->
# Name the last eligible replay writer search with C++23 find_last_if

## Context

Inventory F087 is C++23 `std::ranges::find_last`, `find_last_if`, and `find_last_if_not`. The actionable application is only `find_last_if` in `engine::ReplayFixtures::ArmPersistenceFailure`, `Engine/Source/Agent/Commands/ReplayFixtures.cpp:158`. At baseline `6c6245f4b1dee82d7721ef271ce4925ca754ff47`, lines 179–195 implement a last-match search with a nullable pointer accumulator, reverse iterator loop, assignment, break, and null check. Naming this operation removes bookkeeping while retaining the existing fixture selection behavior. This is a demonstrated clarity improvement, not a bug or measured optimization.

The function first selects the latest generation directly. Only when that generation is terminal does it search backward for a terminal generation retaining an end frame. A live latest writer must not be bypassed.

## Design

The author's recommendation is to replace only the terminal-branch search with the following expression and checks, preserving the surrounding branch and initial back-element selection:

```cpp
auto selectedIt = std::ranges::find_last_if(rWriterGenerations, [](const Replay::ReplayWriterState& rGeneration)
{
	return rGeneration.bTerminal && rGeneration.pRetainedEndFrame != nullptr;
}).begin();
if (selectedIt == rWriterGenerations.end())
{
	return false;
}
pSelectedGeneration = &*selectedIt;
```

No named subrange, helper, iterator alias, or ownership transfer is needed. `Pch.h` includes `ExternalHeaders.h`, and `Common/ExternalHeaders.h:59` already includes `<algorithm>`; no include change is needed.

[P1223R5 section 0.3 and find-last wording](https://wg21.link/P1223R5) specify a suffix beginning at the final match, or the empty end suffix when absent. Immediate `.begin()` therefore gives the required iterator. The temporary subrange owns no vector elements; its destruction leaves the copied iterator valid because the lvalue vector remains alive and unmodified through pointer use.

Reverse early exit is an explicit invariant of this application, not a consequence inferred from the standard's linear upper bound. The inspected VS 2026 MSVC 14.51.36231 `include/algorithm:3547` range overload routes the bidirectional const vector to `_Find_last_if_unchecked`; its bidirectional/common branch at line 3568 decrements from end and returns at the first matching predicate. Thus this supported implementation preserves newest-to-oldest predicate order and predicate count, including stopping at the first match. Preserve that implementation property when executing this plan; if the selected toolset no longer has it, surface the contradiction before implementation rather than accepting a forward full scan. No speedup is claimed.

## Critical files

- `Engine/Source/Agent/Commands/ReplayFixtures.cpp` — sole intended edit, terminal branch in `ArmPersistenceFailure`.
- `Common/ExternalHeaders.h` and `Projects/BrokenEngineSandbox/Source/Pch.h` — read-only include evidence.
- `Engine/Source/Agent/AGENTS.md` — existing fixture ownership and main-thread contract; synchronize only if implementation reveals a factual documentation discrepancy.
- `Documents/C++StyleGuide.txt` — existing rules 2, 15(c), and 18 cover lambda braces, iterator `auto`, and const-reference parameters; no rule amendment is justified for one search.

## In scope

Replace the null reset, reverse loop, and pointer-null test inside `ArmPersistenceFailure`'s `pSelectedGeneration->bTerminal` branch with the iterator expression, end check, and pointer assignment above. Perform the required focused review and documentation synchronization checks without manufacturing documentation edits.

## Out of scope

Other replay searches; adoption of `find_last` or `find_last_if_not`; wholesale algorithm modernization; fixture API or schema changes; new helpers or tests; style-guide additions; unrelated comments; any persistence, replay format, simulation, wire, project membership, threading, or ownership change.

## Acceptance criteria

| Criterion | Decisive evidence |
|---|---|
| Missing binding, missing coord, and empty generations still return false without Binding mutation. | Source review of unchanged guards before selection. |
| A nonterminal latest writer remains selected directly, regardless of retained frame. | Branch review proves search is still conditional on latest writer being terminal. |
| With terminal latest writer, choose the newest terminal writer having retained end frame; skip terminal writers without a frame and nonterminal writers. | Exact predicate and final-match semantics, including multiple eligible generations. |
| No eligible writer returns false before any Binding mutation. | End comparison precedes dereference and all arming assignments. |
| Activation tick and final failure point/coordinate assignments remain identical. | Focused before/after diff. |
| Reverse short circuit, allocation-free selection, vector/frame ownership and pointer lifetime are preserved. | Supported STL bidirectional/common implementation inspection and local lifetime review. |
| C++23 expression is supported in the owning target. | Successful affected server build through `/compile`. |

## Verification

Review the acceptance cases independently against the original and replacement control flow. Inspect the implementation of `find_last_if` in the toolset used for the build for the documented reverse early exit. Build BrokenEngineSandboxServer Debug|x64 through `/compile`; the source is whole-file `BT_SERVER` and listed in `BrokenEngineSandboxServer.vcxproj`. Run the applicable C++ correctness, style, comment, affected-code, and AGENTS synchronization workflow. No new unit tests or runtime driving are needed for this local semantics-preserving rewrite. Any subsequently justified live verification must use `/agent-harness`.

## Notes

Future implementation: Change Workflow Tier 1, triggered by a local behavior-preserving replacement with no public signature or invariant change. Replay determinism, CRC, serialization, compatibility, allocation, trust, and threading remain unchanged. No version bump applies. No dependencies or Coordination constraints were found: the live Plans search for `ArmPersistenceFailure`, `ReplayFixtures`, `find_last_if`, and F087 had no overlap at authoring. Recommendations above describe the proposed implementation; this document does not assert separate user approval of its choices.
