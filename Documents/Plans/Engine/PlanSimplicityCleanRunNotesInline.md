<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T20:22:49.723Z","dependsOn":[]} -->
# Fix: /plan-simplicity-review — a PASS run still returned its Q1-Q9 judgment notes inline

## Context
A `/plan-simplicity-review` reviewer dispatched in this session returned a PASS
result whose message carried a preamble plus roughly 1,500 characters of Q1-Q9
per-question judgment notes inline, above the shared handoff lines. The manager
used none of that prose: the only thing the PASS result decided was that the
plan needed no simplification. The reviewer was dispatched with the ordinary
shared task brief; nothing in the brief asked for the notes.

`.agents/skills/plan-simplicity-review/SKILL.md` already carries a clean-run
rule at `:107-110` — "A clean result returns that `PASS` statement, that one
extension field, and the shared handoff lines. A clean run writes that `Temp/`
file only when its per-question judgment notes must travel; otherwise those
notes stay in the reviewer's own context." The route therefore exists, yet the
dispatched reviewer did not take it. The route that is written in imperative,
unmissable form is the findings-run one at `:96-99` ("the ... per-question
judgment notes travel in a gitignored `Temp/` file ... The handoff does not
restate them"); the clean-run sentence sits after it as a trailing qualifier,
and states where the notes may go rather than forbidding the inline copy in the
words the findings-run paragraph uses.

`.agents/skills/plan-simplicity-review/references/worker.md` is the file the
dispatched reviewer actually reads end to end, so whether the clean-run rule
reaches the reviewer at all is part of what a fix session must check.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 616368aa-4f28-451e-802e-f98a84986c30
- Worktree/branch UUID: 9ccc2bc0-a8f0-4b07-8732-27723934b85c
- Session branch: claude/9ccc2bc0-a8f0-4b07-8732-27723934b85c
- Worktree: .claude\worktrees\BrokenEngine\9ccc2bc0-a8f0-4b07-8732-27723934b85c
- Landing ref: claude/9ccc2bc0-a8f0-4b07-8732-27723934b85c
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/9ccc2bc0-a8f0-4b07-8732-27723934b85c` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below.

The author's recommendation: state the clean-run rule in the same prohibitive
form the findings-run paragraph uses — a PASS handoff is the `PASS` statement,
the `Steps reviewed:` extension field, and the shared handoff lines and nothing
else, with the Q1-Q9 judgment notes either dropped or written to the gitignored
`Temp/` file and cited under `Evidence`. Check while doing so whether the
clean-run rule needs to be reachable from `references/worker.md`, which is the
file the dispatched reviewer reads, and add the shortest pointer there if it is
not; do not duplicate the rule in both files.

If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/plan-simplicity-review/SKILL.md` — the clean-run rule at
  `:107-110` and the findings-run route at `:96-99` it must match in force
- `.agents/skills/plan-simplicity-review/references/worker.md` — the file the
  dispatched reviewer reads; only a pointer belongs here if one is missing

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the clean-run handoff wording in
  `plan-simplicity-review/SKILL.md` `## Handoff` and, if the investigation
  proves it necessary, one pointer to it in that skill's `references/worker.md`

## Out of scope
- The review's judgment content: the Q1-Q9 questions, the finding classes, the
  severity meanings, and the `plan-not-worth-executing` rule
- The findings-run finding row form, which
  `Documents/Plans/Engine/PlanSimplicityHandoffSingleCopy.md` records against
  the same `## Handoff` section
- `.agents/references/subagent-reporting.md` and the shared handoff form itself
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one review skill's returned contract);
escalate if the fix reaches build/bootstrap coordination. Invariants to
preserve: a PASS result still returns the `PASS` statement and the
`Steps reviewed:` extension field; the shared handoff's required lines are all
still returned, with `Residuals` last; findings-run behavior is unchanged.
Never embed transcript paths or home paths.

## Acceptance criteria
- The clean-run wording, read on its own, forbids an inline copy of the Q1-Q9
  judgment notes in the same terms the findings-run paragraph uses, and names
  the one place those notes may travel
- A reviewer reading only the files it is told to read reaches that rule
- `/validate-skill` and `/progressive-disclosure-review` pass on the changed
  files; plan validate exits 0

## Notes
`Documents/Plans/Engine/PlanSimplicityHandoffSingleCopy.md` targets the same
`## Handoff` section for a different recorded symptom (each finding listed
twice). A fix session for either Plan should re-read the other first: the
one-line finding entry form landed in commit `83ba4af9` may already have
settled part of that Plan's symptom, and the two boundaries touch adjacent
paragraphs of the same section.
