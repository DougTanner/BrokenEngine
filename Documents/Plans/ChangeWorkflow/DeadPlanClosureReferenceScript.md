<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-04T00:06:36.526Z","dependsOn":[]} -->
# Delete the uncalled `Find-PlanClosureReferences.ps1` script

## Context

`.agents/scripts/Find-PlanClosureReferences.ps1` has no caller anywhere in the
repository. At baseline `56816149` a repository-wide search for
`PlanClosureReferences` (excluding `Temp/`) matched exactly one file,
`Documents/Plans/Engine/InstructionValidatorExtensions.md`, which is a Plan
document rather than a caller, and which the session that recorded this
follow-up completed and deleted. No skill, script, module, or reference document
invokes the script or names its `broken-engine-plan-closure-references/v1`
output schema.

The script takes mandatory `-Worktree`, `-Baseline`, and `-CompletedPlan`
parameters and scans `Documents/Plans`, `Documents/Features`, and
`Documents/Investigations` for prose mentions of a completed Plan's path or
name, emitting `broken-engine-plan-closure-references/v1` at `:56`. That is a
plan-closure stale-reference scan, but no workflow runs it: neither
`/finalize-changes` nor `/next-plan` nor their scripts mention it, and no prose
in `.agents/` or `Documents/` describes such a scan as a step anyone performs.

History gives no evidence of an intended caller either. The script first appears
in the squashed repository baseline commit `e571f6f1`, and the only later commit
mentioning it, `b047dc9a`, merely added the Plan document cited above. It has
therefore never had a caller in recorded history.

The adjacent scheduler behaviour is already owned elsewhere:
`Documents/Plans/AGENTS.md` states that plan completion removes direct child
metadata edges and that a missing dependency is a satisfied stale edge reported
as a notice, so the metadata side of plan closure is WorktreeCli's, not this
script's.

## Design

The author recommends deleting `.agents/scripts/Find-PlanClosureReferences.ps1`,
because the root `AGENTS.md` minimum-sufficient-change directive keeps only
mechanisms something uses, and no evidence identifies the caller this script was
meant to serve. Wiring it into a workflow instead would add an unrequested step
to plan closure, which no rule asks for.

Before deleting, the implementing session re-runs the caller search at its own
baseline: a repository-wide search for `PlanClosureReferences` excluding `Temp/`.
If that search shows a real caller has appeared since, the correct outcome is to
close this Plan as stale rather than delete the script.

`.agents/scripts/FinalizeWorkflowCommon.psm1` stays. Its
`Get-FinalizeGitIdentity`, `Test-FinalizeGitSuccess`, and `Invoke-FinalizeGit`
helpers are also used by `.agents/skills/finalize-changes/scripts/*`,
`.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1`, and
`.agents/skills/compile/scripts/Resolve-CompileContext.ps1`, so removing the
script's `Import-Module` line orphans nothing.

## Critical files

- `.agents/scripts/Find-PlanClosureReferences.ps1` — the file to delete
- `.agents/scripts/FinalizeWorkflowCommon.psm1` — read-only; must remain

## In scope

- Deleting `.agents/scripts/Find-PlanClosureReferences.ps1` in full

## Out of scope

- Any change to `.agents/scripts/FinalizeWorkflowCommon.psm1` or to any other
  script or module
- Adding a plan-closure stale-reference scan to `/finalize-changes`,
  `/next-plan`, or any other workflow
- Any duplicate-search or `-Terms` extension of this script; that extension was
  Design 4 of `InstructionValidatorExtensions.md` and the user declined it
- Any change to `.agents/skills/create-follow-up-plans/references/worker.md`

## Acceptance criteria

- The file `.agents/scripts/Find-PlanClosureReferences.ps1` no longer exists.
- A repository-wide search for `PlanClosureReferences`, excluding `Temp/`,
  returns no matches.
- No other tracked file is changed by this Plan.
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` and `code: ok`.

## Notes

Change Workflow tier: Tier 1 (mechanical). Trigger: the root `AGENTS.md` Tier-1
definition — local behaviour-preserving work with no public signature or
invariant exposure. Deleting a script nothing calls changes no observable
behaviour, touches no determinism/CRC, serialization, `.pack`/`kiVersion`,
replay, wire, threading, allocation, shader, or build surface, and requires no
live verification. The script is not a member of any Visual Studio project.
