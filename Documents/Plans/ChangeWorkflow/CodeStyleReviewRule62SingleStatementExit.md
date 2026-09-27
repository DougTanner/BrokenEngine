<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T18:06:51.886Z","dependsOn":[]} -->
# Fix: /code-style-review — rule 62 auto-split applied to a guard whose exit is LOG plus return

## Context
Observed symptom: in a `/next-plan` run, the `/code-style-review` worker ran
`.agents/scripts/Test-StyleRuleJudgment.ps1`, which flagged a newly added guard in
`Projects/BrokenEngineSandbox/Source/ClientSettings.cpp` `LoadClientState` as
`rule62, probability 0.89`, and the worker recorded it `confirmed`. The guard is
one `if (A || B || C)` whose body is a `LOG(kDefault, kWarning, ...)` call
followed by `return`. The worker's step 11 auto-fix split it into three `if`
blocks, each repeating the identical `LOG` call plus `return`.

`Documents/C++StyleGuide.txt` rule 62 (line 299) applies only when the
conditions "guard the same single-statement exit"; a `LOG`-plus-`return` body is
two statements, so the construct was never a rule 62 violation and the split
only duplicated code. Main had to re-read the diff and rule 62, then dispatch a
`/resolve-findings` implementer to revert the split before the pre-review build
could start.

Current-tree observations for the fix session (not a proven root cause):
- `.agents/scripts/Test-StyleRuleJudgment.ps1:63-66` — the `rule62`
  instructions already say "whose body is a single exit statement", yet the
  model returned a 0.89 positive for a two-statement body.
- `.agents/skills/code-style-review/references/style-rule-judgment/cases.json`
  holds rule 62 positive cases (`v62-packed-guards`,
  `v62-two-packed-guards-continue`) and no negative case whose packed guard
  body is a log call plus an exit.
- `.agents/skills/code-style-review/references/worker.md` step 10 defines a
  flagged entry's `Judgment` row as `confirmed` "when the violation involves a
  session-changed line, otherwise `false flag`"; the step keys `confirmed`
  versus `false flag` on whether the line is session-changed, not on whether
  the flagged entry is actually a violation.
  `.agents/skills/code-style-review/SKILL.md` `## Handoff` `Judgment` mirrors
  the same two outcomes.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 04c78988-60eb-4522-b204-a0eff911ecf3
- Worktree/branch UUID: 797b1e03-49cd-4416-a349-9f0437911fb0
- Session branch: claude/797b1e03-49cd-4416-a349-9f0437911fb0
- Worktree: .claude\worktrees\BrokenEngine\797b1e03-49cd-4416-a349-9f0437911fb0
- Landing ref: claude/797b1e03-49cd-4416-a349-9f0437911fb0
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CodeStyleReviewRule62SingleStatementExit.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`:
decide whether the auto-split came from the worker's step 10 adjudication text
(the `confirmed`/`false flag` outcomes keyed on session-changed lines rather
than on rule 62's single-statement-exit condition), from the judgment script's
`rule62` criteria, or both. Only when the transcript is genuinely needed, in a
new session run `/next-plan-review claude/797b1e03-49cd-4416-a349-9f0437911fb0`
in bounded friction mode, supplying client `claude` and the conversation
session ID recorded above. Then make the smallest fix inside the `## In scope`
boundary below. If root-causing shows the fix lies outside that boundary,
surface it for re-planning instead of expanding scope.

The author recommends that the fix make a rule 62 finding (and therefore its
step 11 auto-split) require that every packed condition guards the same
single-statement exit, and that a flagged entry failing that test be recorded
`false flag` rather than `confirmed`; if the judgment script is also a
source, tighten its `rule62` criteria and add one negative `cases.json` case
for a packed guard whose body is a log call plus `return`.

## Critical files
- `.agents/skills/code-style-review/references/worker.md`
- `.agents/skills/code-style-review/SKILL.md`
- `.agents/scripts/Test-StyleRuleJudgment.ps1`
- `.agents/skills/code-style-review/references/style-rule-judgment/cases.json`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to: `worker.md` step 10 (flagged-entry
  adjudication and `Judgment` outcomes) and step 11 (auto-fix eligibility) as
  they apply to rule 62; `SKILL.md` `## Handoff` `Judgment` field only if its
  outcome set changes; the `rule62` entry in `Test-StyleRuleJudgment.ps1`; and
  rule 62 cases in `cases.json`

## Out of scope
- The landed change the session produced, including
  `Projects/BrokenEngineSandbox/Source/ClientSettings.cpp`
- `Documents/C++StyleGuide.txt` rule 62 text, which is already correct
- Judgment criteria and cases for rules other than 62
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: a skill worker's adjudication rule and a
judgment script's criteria); escalate if the fix reaches build/bootstrap
coordination. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a
  packed `if (A || B || C)` guard whose body is a log call plus `return` on a
  session-changed line is not split by `/code-style-review`, while a packed
  guard whose body is a single `return`, `continue`, or `break` still is
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
Friction recorded at a `/next-plan` claim exit; the Plan that run completed is
not a dependency.
