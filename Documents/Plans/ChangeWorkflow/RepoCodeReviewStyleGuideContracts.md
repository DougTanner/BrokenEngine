<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T12:47:17.395Z","dependsOn":[]} -->
# Fix: /repo-code-review — check the style guide's correctness-contract rules 9, 53 and 60

## Context
`Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md` maps every
numbered rule of `Documents/C++StyleGuide.txt` to what enforces it on the agent
Change Workflow path. Three rules state correctness or performance contracts
rather than style, and no review owns them:
- Rule 9 (`Documents/C++StyleGuide.txt:68`): exceptions only for fatal errors,
  never control flow; standard exception types by default; no empty derived
  exception type.
- Rule 53 (`:250`): several `push_back`/`emplace_back` calls on one
  `std::vector` are preceded by `reserve`.
- Rule 60 (`:275`): a `std::memcpy` size is gated on the destination, with
  `std::min` of both sizes when they differ.

Current tree:
- `.agents/skills/code-style-review/references/worker.md` step 7 (:67-76) and
  the scanner kinds in `.agents/scripts/Find-SessionCandidates.ps1` (:45-57)
  cover none of the three.
- `.agents/skills/repo-code-review/SKILL.md:25-27` sends style, formatting and
  naming to `/code-style-review`, and its checks
  (`.agents/skills/repo-code-review/references/checks.md`) name none of these
  rules. The nearest checks are the failure-channel rule at :50-73, which
  says there is no universal throw policy, and allocation tracking at :75-88,
  which covers heap allocation only while tracking is active.

## Design
The author's recommendation: add one subsection to `checks.md`,
`### Style guide contracts`, holding one check per rule:
- Rule 9: flag a `throw` a changed path catches and recovers from as normal
  control flow, and a new exception type that only renames a standard one.
  State that a subsystem's existing failure channel (:50-73) still decides
  whether a failure throws; this check only rejects non-fatal use.
- Rule 53: flag two or more `push_back`/`emplace_back` calls on one
  `std::vector`, in sequence or in a loop whose count is known, without a
  preceding `reserve`.
- Rule 60: flag a `std::memcpy` whose size is taken from the source alone
  where the destination can be smaller.
Add one matching bullet to the checks index in
`.agents/skills/repo-code-review/references/worker.md` (:57-83).

Rationale: each rule's failure is a reachable defect or cost that needs the
producer, consumer, and type tracing `/repo-code-review` already does; the
style review's line scanner and hand read cannot see the loop count, the
catch site, or the destination size.

## Critical files
- `.agents/skills/repo-code-review/references/checks.md`
- `.agents/skills/repo-code-review/references/worker.md`

## In scope
- `checks.md`: a new `### Style guide contracts` subsection and its entry in
  the file's section list
- `worker.md`: one checks-index bullet for that subsection

## Out of scope
- `/code-style-review`, its scanner, and every other guide rule
- `.agents/skills/repo-code-review/SKILL.md`
- `Documents/C++StyleGuide.txt`; sweeping existing violations in unchanged
  code
- Any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 1 (documentation): trigger is an edit to a review skill's check prose,
with no script change. The review stays findings-only. Never embed transcript
paths or home paths.

## Acceptance criteria
- `checks.md` holds one check each for rules 9, 53 and 60, and the worker's
  checks index names the subsection
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing, including the
  `validate-skill` row for the `repo-code-review` package

## Notes
Originating record:
`Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md`.
