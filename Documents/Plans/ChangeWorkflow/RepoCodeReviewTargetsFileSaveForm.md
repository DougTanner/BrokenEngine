<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-11T22:27:57.010Z","dependsOn":[]} -->
# Fix: repo-code-review — `## Inputs` names no compliant single-call save form or path for the targets file

## Context
`.agents/skills/repo-code-review/SKILL.md` `## Inputs` tells the dispatching
manager that it "saves one read-only run to a file" of
`pwsh -NoProfile -File .agents/scripts/Get-SessionChangeInventory.ps1
-RepositoryRoot <absolute repository toplevel> -Baseline <full 40-character SHA>
-EmitTargets`, but names neither a save path convention nor a save form that
satisfies the root `AGENTS.md` bundled-scripts rule: one script per shell call
with nothing chained before or after it, no working-directory change, and no
absolute script path. The section stops at the script invocation, so the
redirection that turns its stdout into the required file is left to the caller
to invent.

In the observing session main produced the targets file through a non-canonical
chained call — a directory creation before the script and a content probe after
it in the same shell call — which the root rule forbids. The workaround was
avoidable: the root rule already permits using a script call's own output from
the PowerShell tool, so redirecting the script's own stdout to a file in the
same single call is compliant and would have needed no chaining, had the section
said so and named where the file belongs.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 1e48cbdc-67da-4277-bcf1-3548f3ae04e4
- Worktree/branch UUID: a75970c8-ed2b-4c66-a830-960f0204d3ac
- Session branch: claude/a75970c8-ed2b-4c66-a830-960f0204d3ac
- Worktree: .claude\worktrees\BrokenEngine\a75970c8-ed2b-4c66-a830-960f0204d3ac
- Landing ref: the session branch above. The recording session had not landed
  its change when this Plan was written, so the branch tip named here contains
  this Plan only once that session's work is committed to it; until then the
  Plan exists as tracked content in the worktree above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the
recorded client and conversation session ID above. Then make the smallest fix
inside the `## In scope` boundary below. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

The author's recommendation is to state, in `## Inputs` alongside the existing
invocation, the compliant save form — the script's own stdout redirected to the
save path within that same single shell call, which the root rule allows as
using the call's own output — and the gitignored `Temp/` save-path convention
for the resulting targets file, so callers neither invent a location nor chain
setup commands around the script. Whether the reference to the root rule is
restated or linked is a wording choice for the fix session, and the fix must not
weaken the existing `status`/exit-code handling the section already states.

## Critical files
- `.agents/skills/repo-code-review/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `## Inputs` section of
  `.agents/skills/repo-code-review/SKILL.md`

## Out of scope
- The landed change the observing session produced
- `.agents/scripts/Get-SessionChangeInventory.ps1` itself and its result shape
- The root `AGENTS.md` bundled-scripts rule
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 1 (documentation-only skill prose with no behavior or invariant exposure);
escalate only if the fix reaches script behavior or the root bundled-scripts
rule. Never embed transcript paths or home paths. The stated save path must stay
gitignored so no targets file enters tracked content.

## Acceptance criteria
- `## Inputs` states a save form a manager can follow in one shell call that
  runs the script alone, and names the save-path convention
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing
