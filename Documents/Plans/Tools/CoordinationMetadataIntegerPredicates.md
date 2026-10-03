<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:38:27.206Z","dependsOn":[]} -->
# Express coordination metadata integer checks with C++20 utilities

## Context

`Tools/ToolCommon/CoordinationStore.cpp:382-419` manually expresses mixed-sign equality and destination-type representability in three metadata predicates. The current checks are correct, but their sign checks, casts, and explicit bounds obscure the operation being validated. This is a user-authorized clarity refactor, with no intended acceptance-policy change.

`JsonIntegerEquals` compares unsigned JSON against signed `iExpected`; `JsonInt64` rejects unsigned values above `INT64_MAX`; `ValidateMetadataEnvelope` restricts an optional signed PID to the `uint32_t` range. All three currently reject inappropriate JSON kinds through existing guards. The gap is the hand-written spelling of operations directly represented by C++20 integer comparison utilities.

## Design

I recommend exactly these replacements because they name the existing predicates without introducing helpers or changing validation policy:

1. In `JsonIntegerEquals`, replace only the unsigned branch return with `return std::cmp_equal(rValue.get<uint64_t>(), iExpected);`. Keep unsigned-first dispatch and the signed/integer fallback.
2. In `JsonInt64`, replace only `uiValue <= static_cast<uint64_t>(INT64_MAX)` with `std::in_range<int64_t>(uiValue)`. Preserve the explicit cast after the check, optional construction, and rejection through `std::nullopt`.
3. In `ValidateMetadataEnvelope`, replace `claimantPid && *claimantPid >= 0 && *claimantPid <= UINT32_MAX` with `claimantPid && std::in_range<uint32_t>(*claimantPid)`. Preserve the presence guard before dereference and every other envelope condition.
4. Move the existing `<utility>` include from `CoordinationStore.cpp` into the sorted standard-header group of `Tools/ToolCommon/ToolCliCommon.h`, as required by the ToolCommon header ownership rule. Leave all other includes alone.

The [C++ working draft integer comparison specification](https://eel.is/c++draft/utility.intcmp) defines sign-aware equality and destination-bound range checking for these signed/unsigned integer operands. Its predicates preserve the numeric checks here.

The first replacement evaluates `get<uint64_t>()` even when `iExpected` is negative. Preserve the preceding `is_number_unsigned()` guard: `ThirdParty/tinygltf/json.hpp:3976-4005`, `get_arithmetic_value`, implements that category as a stored-value read and scalar conversion. This does not allocate, parse, mutate, or throw for the established category. Do not apply unchecked extraction to other kinds.

### Risk and invariants

Tier 1: local behavior-preserving work with no public signature or invariant change. The code sits at an existing metadata validation boundary, but neither what it accepts nor what it trusts changes. No lock acquisition, session ownership, coordination ordering, format, schema version, serialization layout, threading, wire, save/replay, or determinism/CRC changes are involved.

Performance scope is normal Release AgentTools. Adoption review inspected MSVC 14.51.36231 `include/utility`: `_Cmp_equal` (865-873) uses the same sign/equality checks, and `_In_range` (930-952) removes unnecessary bounds at compile time. The unsigned-to-signed case retains one upper bound; the signed-to-unsigned PID case retains lower and upper bounds. This is equivalent scalar work with no allocation, iteration, or extra metadata access. Both Release tool projects use MaxSpeed, AnySuitable inlining, and whole-program optimization. This supports non-regression, not a speedup claim; Debug may retain call overhead. No benchmark or generated-code result is claimed.

## Critical files

- `Tools/ToolCommon/CoordinationStore.cpp`: `JsonIntegerEquals`, `JsonInt64`, `ValidateMetadataEnvelope`, and its `<utility>` include.
- `Tools/ToolCommon/ToolCliCommon.h`: standard-header group.

## In scope

Only the three predicates and the `<utility>` include relocation described above, plus required verification and documentation checks for those edits.

## Out of scope

Other comparisons or comparison-family functions; new helpers, tests, or abstractions; JSON extraction or envelope restructuring; changing accepted values or JSON kinds; other include cleanup; third-party edits; coordination policy; public APIs; project membership; global language/style policy changes.

## Acceptance criteria

- The diff contains the three exact predicate replacements and one include relocation, with no validation-policy changes.
- Source-based equivalence review records unsigned JSON against negative expected values, including `UINT64_MAX`, as unequal; zero equals zero; signed/integer fallback stays identical.
- Source-based equivalence review records unsigned `INT64_MAX` as accepted by `JsonInt64`, `INT64_MAX + 1` and `UINT64_MAX` as rejected; signed values retain existing behavior.
- PID range review records -1 rejected, 0 and `UINT32_MAX` accepted by the PID predicate, and `UINT32_MAX + 1` rejected. Missing PID and floats, strings, booleans, null, arrays, and objects retain rejection. Other envelope fields still gate the overall result.
- `/compile` builds AgentHarness and WorktreeCli in Release|x64 successfully, proving both consumers can use the common `<utility>` declaration. No unit tests are added.

## Verification

Use source and standard-library equivalence review to settle the boundary matrix above, including evaluation order and optional safety. Run the ordinary C++ correctness, comment, affected-code, code-style, and documentation checks required by Change Workflow. Use `/compile` for both tool targets; no game launch or `/agent-harness` runtime exercise is required for these scalar predicate substitutions. If the implementation cannot preserve the stated equivalence, return the discrepancy instead of broadening the change.

## Documentation and code style

`Tools/ToolCommon/AGENTS.md` owns standard-header aggregation in `ToolCliCommon.h`; `.agents/references/cpp-conventions.md` routes PCH-less AgentTools there. `Documents/C++StyleGuide.txt` rule 47 owns include grouping and order. Apply those existing rules to the relocation. No AGENTS or style-guide prose update is proposed: the APIs, behavior, architecture, and language policy remain unchanged. Perform `/update-claude-docs` as the implementation workflow requires and record no update needed if that remains true.

## Coordination

No dependencies or mandatory coordination constraints. Existing Plans were searched by all three symbols, both critical paths, mixed-sign and representability terms, comparison utility names, and Coordination sections; none owns this implementation boundary.

## Notes

Planning only: no source change, build, runtime verification, or performance measurement has been performed for this Plan. Future implementation follows the repository Change Workflow and its finalization route.
