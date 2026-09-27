<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T13:29:12.263Z","dependsOn":[]} -->
# Cleanup: fix existing code that breaks style guide rules 9, 53 and 60, the /repo-code-review style guide contracts

## Context
`Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md` routes
rules 9, 53 and 60 of `Documents/C++StyleGuide.txt` to new `/repo-code-review`
checks (`Documents/Plans/ChangeWorkflow/RepoCodeReviewStyleGuideContracts.md`):
- Rule 9 (`Documents/C++StyleGuide.txt:68`): exceptions only for fatal
  errors, never control flow; no empty derived exception type.
- Rule 53 (`:250`): several `push_back`/`emplace_back` calls on one
  `std::vector` are preceded by `reserve`.
- Rule 60 (`:275`): a `std::memcpy` size is gated on the destination.

That review reads only session-changed C++, and its Plan leaves existing code
as it is (its `## Out of scope`). The user asked for a follow-up that cleans
up the existing rule-breaking code. `/repo-code-review` takes a session diff
and a targets file (`.agents/skills/repo-code-review/SKILL.md` `## Inputs`)
and has no cleanup scope, so this Plan finds sites by search and applies the
landed checks' wording by hand.

Candidate counts in first-party C++ (`Common/`, `Engine/`, `DataPacker/`,
`Projects/`, `Tools/`) at `1a719473e24b903d7810f80d885122c838c595aa`, before
adjudication: 89 lines matching `catch (` in 36 files, 347 lines matching
`push_back` or `emplace_back` in 76 files, and 109 lines matching `memcpy(` in
43 files. One session can adjudicate these.

## Design
The author's recommendation:
1. Find candidates with `git grep -nP` over
   `-- "Common/*.h" "Common/*.cpp" "Engine/*.h" "Engine/*.cpp" "DataPacker/*.h" "DataPacker/*.cpp" "Projects/*.h" "Projects/*.cpp" "Tools/*.h" "Tools/*.cpp"`:
   `\bcatch\s*\(` and `:\s*public\s+std::\w*(?:exception|error)\b` for rule 9,
   `push_back|emplace_back` for rule 53, and `memcpy\s*\(` for rule 60.
2. Adjudicate each candidate against the landed
   `### Style guide contracts` checks in
   `.agents/skills/repo-code-review/references/checks.md`, with the
   subsystem's existing failure channel (the failure-channel rule in the same
   file) deciding whether a failure throws.
3. Fix accepted sites:
   - Rule 53: add one `reserve` before the appends, sized by the known count.
   - Rule 60: size the copy with `std::min` of the source and destination
     sizes, or with the destination size when the source is never smaller.
   - Rule 9: replace an exception type that only renames a standard one with
     that standard type. Rewrite a `catch` that recovers as normal control flow
     to use the enclosing function's existing non-throwing result (a `bool`,
     `std::optional`, or `std::expected` it already returns); when no such
     channel exists, choosing one is a design decision, so record the site as a
     residual for a separate follow-up Plan instead of changing it.
4. Record every other site the checks flag but step 3 does not fix as a
   residual with its `path:line` and rule.

Rationale: the three rules need the loop count, the catch site, or the
destination size, which a search can find but not decide; the landed checks
are the adjudication text review will apply to new code.

## Critical files
- First-party C++ under `Common/`, `Engine/`, `DataPacker/`, `Projects/` and
  `Tools/` (`*.h`, `*.cpp`)
- `.agents/skills/repo-code-review/references/checks.md` (read only)

## In scope
- Accepted rule 9, 53 and 60 sites in `*.h` and `*.cpp` under the five
  directories, fixed as `## Design` step 3 states

## Out of scope
- `ThirdParty/`, shaders, and every non-C++ file
- Rule 9 sites with no existing non-throwing result channel (residuals)
- Every other guide rule (the scanner and hand-read sweep Plans)
- `.agents/`, `Documents/C++StyleGuide.txt`
- Any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 2 (scoped behavior): trigger is control-flow changes at rule 9 catch
sites and added size gates at rule 60 copies, each inside its own function
with format and trust unchanged. Escalate to Tier 3 if a rule 60 site copies
serialized, wire, replay, or `.pack` bytes and the gate would change what is
copied, or a rule 9 change crosses a thread or subsystem boundary. A
`reserve` added where allocation tracking is active meets the allocation
tracking check in `checks.md`. Sim output stays
bit-identical. Never embed transcript paths or home paths.

## Acceptance criteria
- Each candidate from `## Design` step 1 is fixed, a permitted form under the
  landed checks, or a recorded residual
- `/compile` Client and Server Debug builds pass, plus DataPacker and each
  Tools project whose files changed
- An `/agent-harness` replay determinism check passes

## Notes
Originating record:
`Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md`.
