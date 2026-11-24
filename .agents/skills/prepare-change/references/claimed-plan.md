# Claimed-Plan Preparation

`mode: claimed-plan` additions for [`worker.md`](worker.md); main dispatches in
[the `/next-plan` steps](../../next-plan/SKILL.md#steps) step 4.

## Snapshot

Write the resolved Plan — mechanism, corrections, the claimed Plan's
`## In scope` and `## Out of scope` first copied verbatim as top-level headings
(or from the existing snapshot on difference re-entry),
`## Unresolved decisions` (`none` when all resolved), and the execution card —
into one gitignored `Temp/` file, each under its own `##` heading. Write later
accepted Plan review refinements and user-approved scope changes into the
snapshot's copies of those two scope sections.

Fill card `Roles` from the brief's Change Workflow step and role assignments,
citing the brief as evidence and never reading the workflow itself. Under
[`worker.md`](worker.md)'s Leftovers rule, include each small proven leftover
in `Slices`.

## Investigation portion

When the claimed Plan carries an investigation portion
(`Documents/Plans/AGENTS.md` `## Plan files`), confirm its root cause from
current code and the supplied evidence, including any diagnosis handoff the
continuation capsule carries, before writing the snapshot, and record that root
cause with its evidence in the snapshot. Then bound the card's scope by the fix
that root cause requires inside the Plan's named region: a fix outside it
returns as a scope-widened `Verification` row, and a choice between fixes that
only the user can make becomes an `## Unresolved decisions` entry with its
`unresolved decision` row. When current code and the supplied evidence cannot
confirm the root cause, stop before writing the snapshot and return the form
[`## Handoff`](../SKILL.md#handoff) declares for that stop.

## Harness criteria

When the card's `Acceptance checks` field turns an `/agent-harness` criterion
taken from the claimed Plan into a code reading, also return that criterion as a
weakened-criterion `Verification` row.

## Citation rule

```powershell
pwsh -NoProfile -NonInteractive -Command 'Import-Module ./.agents/scripts/AgentScriptCommon.psm1; Find-AgentLivePlanCitations -RepositoryRoot (Get-Location).Path -Plan <claimed Plan path> | ConvertTo-Json -Compress'
```

Put rewriting each returned citing line on the card as approved scope by this
rule, judging "landed" against the tree after the change, so the claimed Plan's
own change counts as landed on completion and as not landed on rejection: a
pending claim becomes the present-tense fact with its current `path:line` or
symbol when the work landed, and is removed when it did not, except a claim
about work a stage Plan carries after the split in `## Stage split` below, which
[`### Implementation approval`](../../next-plan/SKILL.md#implementation-approval)
handles; a `## Coordination` bullet that only orders against the claimed Plan is
removed, and a conditional in it resolves to the branch that tree shows,
updating the citing Plan's affected decisions to match; a past-tense record
keeps its fact and drops the file name; and a conditional neither branch of
which that tree settles returns to main as a user decision.

## Stage split

When the stage rule in `Documents/Plans/AGENTS.md` `## Plan files` cuts the
resolved work into two or more stages, write one `## Unresolved decisions` entry
and return the `unresolved decision` `Verification` row. The entry lists the
stages in landing order, each with:

- the snapshot items it owns, named by section and label or bullet, never by
  their text;
- the card acceptance checks and citation rewrites it settles: a check in the
  earliest stage whose landed tree can pass it, a rewrite in the stage that
  lands its work, and builds, static checks, and invariants in every stage;
- its rough hand-edited file and added-line counts.

The entry then lists every live Plan whose metadata `dependsOn` names the
claimed Plan, then asks "This is about N files across M stages. Do stage 1 now
and save stages 2 to M as new plans?", recommending yes when the total is over
about 30 hand-edited files or about 1,500 added lines and no otherwise. A no
follows [the `/next-plan` steps](../../next-plan/SKILL.md#steps) step 8.

Work forming one stage over that limit instead gets an entry stating its rough
counts and that no cut point exists, with exactly two options: proceed now
(recommended), or defer the Plan to a later session through
`Defer-NextPlan.ps1` under
[claim-exceptions.md](../../next-plan/references/claim-exceptions.md)
`## Deferral`.

## Rules

- Prepare the supplied claim and leave claim commands
  (`.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`) to the manager.
