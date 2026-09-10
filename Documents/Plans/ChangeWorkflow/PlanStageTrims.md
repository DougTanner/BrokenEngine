<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-10T18:10:08.196Z","dependsOn":["Documents/Plans/ChangeWorkflow/PlanAlternativesClaimTriggerGate.md"]} -->
# Trim the plan-stage ceremony: alternatives fan-out, duplicated citation check, and the unread judgment file

## Context
A repository survey of Change Workflow ceremony found three mechanisms in the
plan preparation and plan review steps that cost work on every planned change
while catching nothing a surviving mechanism would miss:

1. The `/plan-alternatives` blind researcher fan-out (survey rank B5):
   `.agents/references/change-workflow.md:85` dispatches one `researcher` per
   axis at every tier on `/plan-simplicity-review`'s trigger
   (`.agents/skills/plan-simplicity-review/SKILL.md:28-36`, `:47-54`). The axes
   look for a materially better mechanism
   (`.agents/skills/plan-alternatives/references/worker.md:14-23`), which is
   real only when something is being built; the trigger as written also fires on
   a pure deletion and on a rule rewording. Observed in the session that
   recorded this Plan: two researchers dispatched on a pure deletion returned
   nothing, and the skill itself concedes that "An empty axis says nothing about
   whether candidate zero is the simplest approach"
   (`.agents/skills/plan-alternatives/SKILL.md:84-87`).
2. `Test-PlanCitations.ps1` inside `/plan-audit` (survey rank B7):
   `.agents/skills/plan-audit/references/worker.md:10-35` spends steps 2-4 on a
   script run that "renders no verdict" by the skill's own rule (`:32-35`), and
   step 10 (`:62-64`) takes heading presence from it — after
   `/prepare-change` already ran the same script and gated on the same
   `headings.*` and `card.missingFields` values
   (`.agents/skills/prepare-change/references/worker.md:34-42`).
3. The mandatory `Temp/` judgment record in `/plan-simplicity-review` (survey
   rank B9): `.agents/skills/plan-simplicity-review/SKILL.md:97-100` requires an
   authored gitignored file per findings run, while the problem, alternative,
   and disposition a decision needs are already on the `Findings` row
   (`.agents/skills/plan-simplicity-review/SKILL.md:75-90`).

The survey rested on current text and one session's observations, not on
measured outcome rates, so this Plan validates each claim before changing
anything.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: 87c59217-31e1-486d-a91d-84088032264f
- Worktree/branch UUID: 2a97729c-6824-417f-863f-c6dbb60b910d
- Session branch: claude/2a97729c-6824-417f-863f-c6dbb60b910d
- Worktree: .claude/worktrees/BrokenEngine/2a97729c-6824-417f-863f-c6dbb60b910d
- Originating step: user instruction "Create followup plans to validate then fix
  these", given after the Verify the acceptance table step of the session that
  removed the delegated handoff `Executor` self-report line.

## Design
Two stages. Stage 2 for a mechanism runs only when its stage-1 checks hold; a
mechanism whose check fails is left exactly as it is — stop and report it, with
no partial edit — while the others may still proceed.

### Stage 1 — validation (read-only)
- Mechanism 1: read all three `/plan-alternatives` axes in
  `.agents/skills/plan-alternatives/references/worker.md` and confirm each one
  searches for a mechanism that replaces or reshapes something being added, so a
  plan whose net effect is deletion or a rule rewording gives every axis an empty
  input. Then confirm `/plan-simplicity-review` still asks whether the plan is
  worth executing at all
  (`.agents/skills/plan-simplicity-review/SKILL.md:92-95`), so the narrowed
  trigger leaves a removal-shaped plan still challenged. The claim holds when
  both are true; it fails, and mechanism 1 stops, if any axis has a defined
  input on a net-deletion plan.
- Mechanism 2: confirm `/prepare-change` runs
  `Test-PlanCitations.ps1` on the drafted plan and gates on
  `headings.inScopePresent`, `headings.outOfScopePresent`, and
  `card.missingFields`, and confirm `/prepare-change`'s handoff carries those
  values to main. Confirm the claimed-Plan route: a tracked Plan claimed by
  `/next-plan` is verified by a `/prepare-change` run
  (`.agents/skills/next-plan/SKILL.md` preparation step), so the snapshot
  `/plan-audit` audits is normally one `/prepare-change` already checked. The
  claim fails, and the run is retained rather than removed, for any route where
  no `/prepare-change` result covers the audited snapshot.
- Mechanism 3: search `.agents/` for any consumer of the
  `/plan-simplicity-review` `Temp/` judgment file — a rule that reads it, cites
  it in a later step, or requires it in a handoff other than as an `Evidence`
  locator. The claim holds when the only reference is the requirement to write
  it.

### Stage 2 — trims
- Mechanism 1: narrow the trigger so the fan-out fires only when the plan adds a
  tracked file, function, class, system, script, or configuration surface,
  excluding a net-deletion plan and an instruction-prose behavior edit, and drop
  the Tier-1 axis-1 dispatch at `.agents/references/change-workflow.md:85`.
  Recommended placement: the narrowed condition is stated once in
  `.agents/skills/plan-simplicity-review/SKILL.md` `## When to use`, which owns
  the trigger today, and `.agents/references/change-workflow.md` and
  `/plan-alternatives` keep referencing it rather than restating it. State the
  accepted cost: a removal that should have been a reshape goes unexplored.
- Mechanism 2: remove steps 2-4 from
  `.agents/skills/plan-audit/references/worker.md`, take heading presence in
  step 10 from the `/prepare-change` handoff instead, and retain the script run
  only for a claimed-Plan snapshot no `/prepare-change` run produced. State the
  accepted cost: a plan hand-edited between preparation and audit loses its
  heading check.
- Mechanism 3: remove the mandatory `Temp/` write from
  `.agents/skills/plan-simplicity-review/SKILL.md:97-100`, keep the single
  `Evidence` locator, and allow a `Temp/` file only when a cost comparison
  genuinely cannot fit its `Findings` row — which is the `user-judgment` case
  the survey named.

## Critical files
- `.agents/references/change-workflow.md` — `## Main-session conduct` Step 2's
  `/plan-alternatives` row
- `.agents/skills/plan-simplicity-review/SKILL.md` — `## When to use` and the
  `Temp/` judgment-record paragraph
- `.agents/skills/plan-alternatives/SKILL.md` — `## When to use`, where the
  trigger is referenced
- `.agents/skills/plan-audit/references/worker.md` — steps 2-4 and step 10

## In scope
- The stage-1 validation reads above
- In `.agents/references/change-workflow.md`: the Step 2 `/plan-alternatives`
  dispatch row, including its Tier-1 axis-1 clause
- In `.agents/skills/plan-simplicity-review/SKILL.md`: the `## When to use`
  trigger condition, and the paragraph requiring the `Temp/` judgment file
- In `.agents/skills/plan-alternatives/SKILL.md` `## When to use`: the reference
  to the narrowed trigger, only where the current wording would otherwise
  contradict it
- In `.agents/skills/plan-audit/references/worker.md`: steps 2-4, step 10's
  heading-presence source, and the renumbering those removals force

## Out of scope
- `.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1` itself and its
  result schema
- `/prepare-change`'s own run of that script and its gate
- The `/plan-simplicity-review` questions, classes, and `Findings` row form
- The `/plan-alternatives` axis definitions, blindness rule, and comparison
  criteria
- The Step 5 through Step 8 review dispatches, which
  `Documents/Plans/ChangeWorkflow/ChangeWorkflowReviewFanOut.md` owns
- Any transcript path, transcript text, or machine-local path in the repository

## Coordination
`Documents/Plans/ChangeWorkflow/ChangeWorkflowReviewFanOut.md` also edits
`.agents/references/change-workflow.md` `## Main-session conduct`, in the Step 5
through Step 8 rows this Plan leaves untouched. The two Plans must not be
implemented in the same worktree at the same time; whichever lands second
re-reads that file and confirms its own cited line ranges before editing.

## Risk tier and invariants
Expected Tier 2: scoped behavior of the plan-stage dispatch routing and of three
skill packages' own workflow steps, under `.agents/references/risk-tiers.md`'s
"one subsystem's ... tool behavior". Escalate to Tier 3 if the fix also changes
`/prepare-change`'s gate or the citation script, which would span independently
owned tooling. Invariants: the `/plan-alternatives` trigger keeps exactly one
owning location and is referenced everywhere else; every plan that adds new code
still reaches the fan-out; a plan snapshot no `/prepare-change` run checked
still gets its heading check; a `/plan-simplicity-review` finding still carries a
resolvable evidence locator.

## Acceptance criteria
- A plan whose net effect is a deletion or an instruction-prose rewording
  triggers no `/plan-alternatives` dispatch at any tier, and a plan that adds a
  tracked file, function, class, system, script, or configuration surface still
  does
- The narrowed trigger is stated exactly once and every other document
  references it
- `/plan-audit` runs `Test-PlanCitations.ps1` only for a plan snapshot no
  `/prepare-change` result covers, and its heading requirement is still enforced
  on every audited plan
- `/plan-simplicity-review` requires no `Temp/` file for an ordinary findings
  run, and each finding still carries one resolvable evidence locator
- `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1` reports the
  `validate-skill` and `markdown-links` rows passing for every changed package
- A mechanism whose stage-1 check failed is unchanged in the diff and named in
  the session's report
