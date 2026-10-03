<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:34:35.316Z","dependsOn":[]} -->
# Move completed worktree paths into their collection

## Context

`Tools/WorktreeCli/LandingLockLifecycle.cpp`, `toolcli::landing::AllRegisteredWorktreesClear`, parses the NUL-delimited output of `git worktree list --porcelain -z`. At baseline `d29fed456d3ede935c5e672f95f13d6733f0660c`, its two `worktrees.push_back(currentWorktree)` calls (lines 137 and 157) copy a completed `std::wstring` into the collection. The parser no longer needs that record's source value. One insertion handles an empty record delimiter; the other handles a remaining final record.

The next path is assigned from a fresh `Utf8ToWide(field.substr(9))` result. Keeping a copy's source buffer therefore supplies no reuse benefit at this site. Moving makes the ownership transfer explicit and avoids copying heap-backed path contents. This is a local allocation/copy improvement, with no end-to-end performance claim or benchmark requirement.

## Design

Replace both insertions with `worktrees.push_back(std::move(currentWorktree))`. Preserve `currentWorktree.clear()` immediately after the loop insertion: subsequent parsing must see an empty source regardless of its unspecified moved-from contents. The final insertion has no later source use. Neither source references nor aliases escape to consumers; the later range loop reads the destination strings.

Add `#include <utility>` to the existing standard-library include group in `Tools/ToolCommon/ToolCliCommon.h`, maintaining alphabetical order. Its owning `AGENTS.md` places shared standard-library consumption headers there. The default-allocator string move transfers allocated storage; short-string handling remains bounded. Do not replace `push_back` or restructure the parser.

No style-guide or AGENTS.md amendment is warranted: style rule 41 already requires explicit `std::` qualification, and no ownership, coordination, or header policy changes.

## Critical files

- `Tools/WorktreeCli/LandingLockLifecycle.cpp` — both completed-record insertions in `AllRegisteredWorktreesClear`.
- `Tools/ToolCommon/ToolCliCommon.h` — standard-library include group.
- `Tools/WorktreeCli/AGENTS.md` and `Tools/ToolCommon/AGENTS.md` — existing coordination and header constraints; reference only.

## In scope

- Change exactly the two `worktrees.push_back(currentWorktree)` arguments in `AllRegisteredWorktreesClear` to `std::move(currentWorktree)`.
- Add the explicit `<utility>` include to `ToolCliCommon.h`.

## Out of scope

- Any other move adoption, container changes, reservation policy, new helper, parser restructuring, or naming cleanup.
- Changes to Git arguments, record validation, marker enumeration, directory checks, failure returns, landing leases, locking, scheduler behavior, or public signatures.
- Project membership changes, documentation/style policy amendments, runtime/game changes, benchmarks, and new unit tests.

## Risk and invariants

Future implementation is **Tier 1**: a local behavior-preserving ownership transfer and an explicit standard-library include, with no signature or invariant exposure. The function participates in coordination, but the change must not alter coordination decisions or control flow; such an expansion requires reassessment rather than inclusion here.

- Preserve each stored path value and record order for both delimiter-terminated and trailing records.
- Keep the loop's explicit `clear()` and `bInvalidEntry` reset unchanged; never depend on a moved-from string being empty.
- Preserve rejection of invalid/empty records and all subsequent Git/filesystem checks exactly.
- Keep tool output, exit behavior, coordination state, and shared-header consumers unchanged.

## Acceptance criteria

1. The source diff contains exactly the two move arguments and the explicit `<utility>` include, apart from unavoidable adjacent whitespace.
2. Each source string is dead as a record value at its insertion; the loop insertion retains its explicit reset and the final insertion has no subsequent source read.
3. Destination paths, ordering, parser branches, and all success/failure decisions remain equivalent by direct control-flow inspection.
4. Both shared-header consumers compile and link successfully in Release|x64 through the candidate tool build path.

## Verification

- Inspect the complete parser and downstream `worktrees` loop against the baseline, including delimiter-terminated records, a trailing record, and invalid-record rejection. A source diff and moved-from lifetime inspection are decisive for semantic equivalence; no live landing-lock recovery exercise is required.
- Run the future implementation's required C++ correctness, style, comment, propagation, and documentation checks under the Change Workflow; no documentation amendment is expected.
- Invoke `/compile` for **WorktreeCli Release|x64** and **AgentHarness Release|x64**, because `ToolCliCommon.h` is consumed by both. Use its candidate-build policy and retain the build evidence. Builds have not been run during Plan authoring.
- No `/agent-harness` scenario or game build is needed: no game behavior or transport changes. Add no unit tests.
- At future landing, follow `/finalize-changes` for required AgentTools promotion; do not modify linked primary binaries directly.
