<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T22:50:16.049Z","dependsOn":[]} -->
# Fix: next-plan — claim-exit prose names a `changedPaths` field the script's result does not carry

## Context

`.agents/skills/next-plan/SKILL.md:126` says a successful
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Complete-NextPlan.ps1`
"reports the `changedPaths` the landing commit must contain", and `:184` repeats
"the `changedPaths` the claim-exit script reported".
`.agents/skills/finalize-changes/SKILL.md:75` uses the same wording for the same
script.

In this session that documented invocation ran with no arguments from the
worktree root and succeeded, returning
`broken-engine-next-plan-completion-result/v3` with `status:"pass"`,
`code:"ok"`, `nextAction:"finalize-changes"`, and the deleted Plan path carried
as `changes.items[0].path`
(`Documents/Plans/Agents/CompileAbsoluteTargetPathResolution.md`) beside
`changes.totalCount`, `truncated`, `selector`, and `requery`. There is no
`changedPaths` key anywhere in that envelope. A consumer following the skill
literally looks up a field that does not exist and must reverse-engineer the
actual projection before it can name the paths the landing commit must contain.

The two field names are not simply a typo: `changedPaths` is the name the
underlying WorktreeCli scheduler emits
(`Tools/WorktreeCli/PlanScheduler.cpp:1011`), and `Complete-NextPlan.ps1:25`
consumes `$prepared.changedPaths` and re-projects it into the v3 `changes`
envelope. The skill prose describes the inner CLI result while the script's
documented output is the outer v3 one; the script's own regression test already
asserts the v3 shape
(`.agents/skills/next-plan/scripts/Test-NextPlanWorkflowScripts.ps1:146` reads
`$completion.changes.items | ForEach-Object { $_.path }`). Which side is
authoritative — the prose or the envelope — is the open question this Plan
defers to `/next-plan-review`; the expected answer is that the v3 envelope is
authoritative and the prose is stale.

The claimed Plan at the time,
`Documents/Plans/Agents/CompileAbsoluteTargetPathResolution.md`, was scoped to
the compile skill's build target invocations and `Tools/WorktreeCli`
`BuildCommand.cpp` target path handling, so the `/next-plan` and
`/finalize-changes` skill prose is outside it: this is tooling friction, not an
in-scope failure of the active change.

Only `.agents/skills/next-plan/SKILL.md` is tracked; the `.claude/skills/`
copy is an untracked generated mirror and is never edited directly.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: fe36f80e-d412-42e1-a666-ef41c25e7922
- Worktree/branch UUID: c0faf209-8637-4558-9c1f-4aa17b596692
- Session branch: claude/c0faf209-8637-4558-9c1f-4aa17b596692
- Worktree: .claude\worktrees\BrokenEngine\c0faf209-8637-4558-9c1f-4aa17b596692
- Landing ref: the session branch above, whose tip is the session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to this session alone
  (its diff limited to this session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact conversation session ID above.

## Design

In a new session, run `/next-plan-review <landing ref>` supplying the recorded
client and the recorded conversation session ID. Root-cause which side is
authoritative — the skill prose or the `broken-engine-next-plan-completion-result/v3`
envelope the script and its regression test both produce — then make the
smallest fix inside the `## In scope` boundary below: normally, correct the
prose in both skills so the field it names is the one a caller can actually read
from the script's result, keeping the meaning ("the paths the landing commit
must contain") intact. If root-causing instead proves the envelope is the
outlier, the change belongs in the script's result shape and its regression
test; that reaches claim-exit script behavior, so surface it for re-planning
rather than making it here.

## Critical files

- `.agents/skills/next-plan/SKILL.md` — `## Claim lifecycle` (`:122-128`) and
  the tooling-friction landing paragraph (`:183-189`).
- `.agents/skills/finalize-changes/SKILL.md` — the claim-exit preparation
  sentence at `:75` naming the same field for the same script.

## In scope

- Root-cause investigation via `/next-plan-review`, run with the recorded
  landing ref, client, and conversation session ID.
- The smallest resulting prose fix, confined to the two SKILL.md regions named
  above, so both name the same field a caller can read from
  `Complete-NextPlan.ps1`'s actual result.

## Out of scope

- The landed change this session produced.
- `Complete-NextPlan.ps1`, `NextPlanWorkflowCommon.psm1`,
  `Test-NextPlanWorkflowScripts.ps1`, and every other bundled script's behavior,
  result shape, or exit mapping.
- `Tools/WorktreeCli` and the scheduler's own `changedPaths` output.
- Claim lifecycle semantics, landing-lock behavior, and the landing
  confirmation contract.
- Unrelated skills/scripts; any transcript path or transcript text in the repo.

## Risk tier and invariants

Expected Tier 1 if the fix is documentation only; escalate to Tier 2 if it
reaches script or scheduler behavior, and re-plan before touching claim-exit
script results. The corrected prose must keep the same obligation — the landing
commit contains exactly the paths the claim-exit script reports — and must keep
`nextAction: finalize-changes` and the claim-held-until-landing rule unchanged.
Never embed transcript paths or home paths.

## Coordination

- The landed commit `e660f26` ("Forward the next-plan claim result into
  preparation dispatches") already rewrote a different region of the same
  `.agents/skills/next-plan/SKILL.md` (claim invocation and preparation
  dispatch, `:42-79`). Re-read that region before editing and preserve its
  result.

## Acceptance criteria

- Following `/next-plan` and `/finalize-changes` literally, a caller reads the
  landing commit's required paths straight from the documented
  `Complete-NextPlan.ps1` result with no field-name adaptation.
- The documented field name matches what a real `Complete-NextPlan.ps1` run
  returns, and the script's regression test still passes unchanged.
- `/validate-skill` passes for every changed SKILL.md; WorktreeCli
  `plan validate` exits `0` with `status:valid` and `code:ok`.

## Notes

This Plan is keyed to the pair (`/next-plan` claim-exit prose, documented
`changedPaths` field absent from `Complete-NextPlan.ps1`'s v3 result). A later
observation of that same pair is a duplicate, not a new residual. It is distinct
from the already-landed claim-evidence handoff into the preparation brief
(commit `e660f26`).
