# Preparation Brief

The contract the `/next-plan` preparation `implementer` follows, cited by path
in its brief. The step that dispatches it, and what main writes into that
brief, stay in [the `/next-plan` steps](../SKILL.md#steps) step 4.

## Snapshot

Write the complete resolved Plan—its mechanism, corrections, the claimed Plan's
`## In scope` and `## Out of scope` sections first copied verbatim as top-level
headings, and the complete execution card on the
template in [`execution-card.md`](../../../references/execution-card.md)
`## Template`—into one gitignored `Temp/` file addressed by its path from the
worktree root, each under its own `##` heading, with a `## Unresolved decisions`
section (`none` when every decision is resolved). Cite that path under
`Evidence` with one `##` selector per section for the Plan review reviews and
the approval presentation. Later accepted Plan review refinements and
user-approved scope changes are written into the snapshot's copies of those
two sections.

Fill the card's `Roles` from the Change Workflow step and role assignments the
brief supplies, citing the brief as the evidence and never reading the workflow
itself.

Before returning the handoff, run
`pwsh -NoProfile -File .agents/skills/plan-audit/scripts/Invoke-PlanCitationsCheck.ps1 <snapshot path>`
and report that result's `headings.inScopePresent`,
`headings.outOfScopePresent`, and `card.missingFields` values as one
`Decisive checks` row. Of that result, only those values gate step 4's Done
condition; every other record it carries, including a `citations.items` entry
with `pathExists` or `lineExists` false, is an advisory lead, never a defect to
clear and rerun.

## Leftovers

Apply the Change Workflow Verify the acceptance table step's Leftovers rule in
[change-workflow.md](../../../references/change-workflow.md) to every leftover
you prove, such as the same defect at a sibling site: each small one goes on the
snapshot's execution card as approved scope, named in the card's
`### What does this plan do?`, and into `Slices`, so the Plan review step
reviews it, and every other one returns under `Residuals`.

## Citation rule

Find every live Plan other than the claimed one that names the claimed Plan's
file name after its metadata line, and put rewriting each such line on the card
as approved scope by this rule, judging "landed" against the tree after the
change, so the claimed Plan's own change counts as landed on completion and as
not landed on rejection: a pending claim becomes the present-tense fact with its
current `path:line` or symbol when the work landed, and is removed when it did
not, except a claim about work a stage Plan carries after the split in
`## Stage split` below, which
[`### Implementation approval`](../SKILL.md#implementation-approval) handles; a
`## Coordination` bullet that only orders against the claimed Plan is
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
about 30 hand-edited files or about 1,500 added lines and no otherwise.

Work forming one stage over that limit instead gets an entry stating its rough
counts and that no cut point exists, with exactly two options: proceed now
(recommended), or defer the Plan to a later session through
`Defer-NextPlan.ps1` under [claim-exceptions.md](claim-exceptions.md)
`## Deferral`.
