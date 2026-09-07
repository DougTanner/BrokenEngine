<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T14:43:24.752Z","dependsOn":[]} -->
# Fix: /repo-code-review — reviewer dispatch blocks because no text tells the manager to pre-run the targets script

## Context
Observed symptom during a `/next-plan` run's Review and resolve correctness step.
A fresh `reviewer` dispatched for `/repo-code-review` returned BLOCKED, stating
that the target selection input required to scope the review is absent. Main then
ran `.agents/scripts/Get-SessionChangeInventory.ps1 -EmitTargets` itself, saved the
`broken-engine-code-quality-targets/v1` output, put its path in the brief, and
re-dispatched the same review — one wasted reviewer dispatch plus one manager
round of script work per review.

Emitter evidence in the current tree:
- `.agents/skills/repo-code-review/SKILL.md`, `## Inputs` (lines 28-39), requires
  the brief to contain "a `broken-engine-code-quality-targets/v1` targets file
  produced by `.agents/scripts/Get-SessionChangeInventory.ps1 -EmitTargets`".
- `.agents/references/subagent-reporting.md` lines 75-83 say only that on a direct
  `reviewer` dispatch the targets file "come[s] from that documented run" of
  `Get-SessionChangeInventory.ps1`, without stating who performs the run. Neither
  file tells the dispatching manager that it must run the script before writing
  the brief and supply the resulting file path, so a manager composing the brief
  from the public files alone has no instruction to produce the input, and the
  worker cannot supply it for itself.

Cost: one blocked reviewer dispatch and one re-dispatch per C++ review, in every
session that reaches this step.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the session
`/next-plan-review` must reach — while the `Landing ref` line names a ref whose
tree actually contains this Plan:
- Client: claude
- Conversation session ID: 223c2800-572a-4c1d-9723-ba093e3ccb2c
- Worktree/branch UUID: 32ceb1c0-59a8-473b-a35e-db107b897125
- Session branch: claude/32ceb1c0-59a8-473b-a35e-db107b897125
- Worktree: .claude\worktrees\BrokenEngine\32ceb1c0-59a8-473b-a35e-db107b897125
- Landing ref: claude/32ceb1c0-59a8-473b-a35e-db107b897125
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the recorded
client and conversation session ID. Then make the smallest fix inside the
`## In scope` boundary below.

The author's recommendation, for the fix session to confirm or replace: state the
dispatcher's obligation once, at the layer that owns direct-dispatch brief
composition — `.agents/references/subagent-reporting.md`, in the paragraph at
lines 75-83 that already names the documented run — so that it says the
dispatching manager performs that run before writing the brief and supplies the
resulting file's path as a brief field, with `/repo-code-review` `## Inputs` left
to keep owning the file's required schema and content. Duplicating the same
obligation into `.agents/skills/repo-code-review/SKILL.md` would violate the root
AGENTS.md progressive-disclosure directive; if root-causing instead shows the
skill file is the owning layer, put it there and reference it from the shared
reference, but not in both.

If root-causing shows the fix lies outside the boundary below — for example that
the script itself should be invoked by the worker, which would change the
worker's read-only contract — surface it for re-planning instead of expanding
scope.

## Critical files
- `.agents/references/subagent-reporting.md`
- `.agents/skills/repo-code-review/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting prose fix, confined to the two files named above: the
  direct-`reviewer`-dispatch paragraph of `.agents/references/subagent-reporting.md`
  and the `## Inputs` section of `.agents/skills/repo-code-review/SKILL.md`

## Out of scope
- `.agents/scripts/Get-SessionChangeInventory.ps1` behavior, parameters, or output
  schema
- `/adversarial-review` and any other skill that consumes a changed-file inventory
- The landed change the observing session produced
- Unrelated skills and scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (documentation prose that changes no runtime or tool behavior).
Escalate to Tier 2 if root-causing concludes a script or a skill's worker contract
must change. Never embed transcript paths or home paths.

## Acceptance criteria
- A manager composing a `/repo-code-review` brief from the public files alone is
  told to run `Get-SessionChangeInventory.ps1 -EmitTargets` and supply the output
  path, so the recorded BLOCKED symptom no longer reproduces
- The obligation is stated at exactly one layer, not restated at the other
- `/validate-skill` passes wherever the root AGENTS.md Apply the triggered cleanup
  step triggers it; plan validate exits 0

## Notes
Recorded as tooling friction at a `/next-plan` claim exit; the observed symptom
substitutes for a confirmed root cause, which the fix session establishes.
