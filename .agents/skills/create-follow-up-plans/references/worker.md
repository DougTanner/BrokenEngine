# Create follow-up Plans — worker

## Steps

### 1. Prove and consolidate proposals

For every proposal:

1. State the false required condition and its originating criterion, accepted
   finding, or residual.
2. Confirm root cause and unresolved state from current source or direct logs.
   Record durable `path:line`, symbol, and observed behavior evidence (or use
   the applicable `## Rules` substitution for unconfirmed root causes,
   standing-landing, or `/save-plan` proposals).
3. Prove it is pre-existing or outside the approved implementation boundary
   (unless exempt under `## Rules`).
4. Preserve user decisions and the authority order in root `AGENTS.md`
   `### Diagnosis Discipline`; report contradictions.

Group items only when root cause, implementation boundary, invariant, and
verification strategy all match. Split stages by the stage rule in
`Documents/Plans/AGENTS.md` `## Plan files`, and split architectural decisions,
separate subsystems, or meaningfully different risks.

Search all live plan files by symbols, paths, root-cause terms, outcome, and
`## Coordination`. A plan is a duplicate when it owns the same root cause and
implementation boundary. Map the proposal to it unless the proven unmet
acceptance criterion requires extending that plan.

Done when every surviving proposal carries the evidence step 1 records — or its
applicable substitute under `## Rules` — and a duplicate decision.

### 2. Draft and classify

Choose the owning area `Documents/Plans/AGENTS.md` `## Plan files` lists and a
concise PascalCase filename; never overwrite a collision. Draft the smallest
decision-complete plan with `# Title`, `## Context`, `## Design`,
`## Critical files`, `## In scope`, `## Out of scope`, `## Acceptance criteria`
when the diff is insufficient, and `## Notes`. Include verified root cause (or
the applicable symptom, standing-landing, or `/save-plan` substitute under
`## Rules`), originating gap, implementation boundary, and applicable
determinism/CRC, serialization/`.pack`/`kiVersion`, replay, wire, affinity,
threading, allocation, shader, build, or live-verification exposure.
When an acceptance criterion is settled by a live `/agent-harness` run, name
the setup that reproduced the observed state (the harness commands or UI
actions, in order, as the observing run recorded them), or state that the
supplied evidence records none. Pre-stage
architectural choices instead of deciding them. Phrase every choice you do make
as the author's recommendation with its rationale rather than in binding
language, per
[`../../../references/authority-order.md`](../../../references/authority-order.md).
Do not add unit tests or unsupported implementation detail.

Derive the future implementation's Change Workflow Tier 1/2/3 from the highest
risk trigger in [`risk-tiers.md`](../../../references/risk-tiers.md) and record
that trigger in the Plan. Pass dependencies via `-DependsOn` per
[`Documents/Plans/AGENTS.md`](../../../../Documents/Plans/AGENTS.md)
`## Plan files` and
[`new-plan-file.md`](../../../references/new-plan-file.md#invocation), omitting
`-DependsOn` for a `## Unresolved decisions` follow-up. Put other mandatory
nondirectional constraints in reciprocal `## Coordination` sections. Never name
a completing `/next-plan` Plan in the body or pass it via `-DependsOn`
([`claim-results.md`](../../next-plan/references/claim-results.md#claim-exit-result-fields)).
Do not add score, effort ranking, queue tier, queue row, request file, or claim
data.

For a tooling-friction proposal, draft the body from
[`tooling-friction-plan-template.md`](tooling-friction-plan-template.md).

Create the file with the repository-owned `.agents/scripts/New-PlanFile.ps1`:

```powershell
pwsh -NoProfile -File .agents/scripts/New-PlanFile.ps1 -Area <area> -Name <PascalCase.md> -Body <body file path> [-DependsOn <plan paths as one comma-separated token>]
```

Its parameters, the `-DependsOn` single-token rule, its result shape, and its
exit handling are in `../../../references/new-plan-file.md`. A Coretide Plan
takes `-Area 'Projects/Coretide/Documents/Plans/<area>'`, per
`Documents/Plans/AGENTS.md` `## Plan files`.

Done when the Plan body exists and its Change Workflow tier trigger is recorded
in it.

### 3. Apply exactly one case

| Case | Tracked Plan bytes | Completion route |
|---|---|---|
| New Plan | Run `New-PlanFile.ps1` (step 2) to write the final Plan under `Documents/Plans/<area>/`; it mints the marker and never overwrites an existing path. | Take the tree validation from the script's folded result; route through `/finalize-changes` only when a landing gate applies. |
| Existing Plan, prose only | Edit the live tracked Plan directly, including reciprocal Coordination prose; preserve its marker byte-for-byte. | Validate and finalize the tracked edit. |
| Existing Plan, dependency change | Edit only the marker's `dependsOn` array plus required reciprocal Coordination prose; never change `createdUtc`. | Validate and finalize the tracked edit. |

After editing an existing tracked Plan, run the scheduler-state check as
[`Documents/Plans/AGENTS.md`](../../../../Documents/Plans/AGENTS.md) documents
it. If it reports `worktree-cli.missing`, stop and report that authorized
primary maintenance through `/compile` is required. Require `status: valid` and
`code: ok`; record its `notices` and `healedClaims`. Never inspect machine-local
claims.

Once the row's route is taken, run the static-checks runner as
[`change-workflow.md`](../../../references/change-workflow.md)
`#### Step 5 — Run targeted pre-review checks` documents it, with the brief's
`Baseline` as the baseline SHA; the runner owns the limit. Rewrap in place
every `line-length` failure it reports in a Plan you wrote or edited, leaving
the metadata line byte-for-byte, and rerun until that row reports no failure in
those Plans. Failures in other files and other rows stay main's Step 5 work.

Never create a request file, row, score, claim, or a commit that publishes a
row.

Done when exactly one row's route was taken for each proposal, any incident rows
deleted, and a runner run after your last rewrap reports no `line-length`
failure in a Plan you wrote or edited.

## Rules

- This skill owns the decision on each proposed follow-up, grouping, duplicate
  detection, area and filename placement, collision handling, dependencies,
  metadata, and Coordination decisions; callers supply evidence, not those
  decisions.
- Inspect missing facts; never invent evidence or behavior.
- Never cite a `Temp/` file in a Plan body you write or edit — a new Plan, a
  tooling-friction body, or a prose edit to an existing Plan: a session that
  claims the Plan works in another worktree and cannot read this session's
  gitignored scratch files. Restate such evidence as the durable facts it
  holds — tracked `path:line`, symbol, and observed behavior.
- When the Plan needs the file itself, copy it into the Plan's support folder
  under [`Documents/Plans/AGENTS.md`](../../../../Documents/Plans/AGENTS.md)
  `## Plan files`, stage the copy with `git add --`, which refuses an ignored
  path, and cite the copy. The copy meets each of these conditions:
  - it carries no absolute path;
  - it carries no transcript path or transcript text;
  - its relative links resolve from the support folder;
  - a text copy ends with a newline;
  - a text copy carries no trailing whitespace.

  Content that breaks a condition is not copied: restate it in the Plan body as
  tracked facts instead.
- Read `Documents/AGENTS.md` and `Documents/Plans/AGENTS.md` completely. Their
  current Plan shape, metadata, dependency, and Coordination rules override this
  skill. This skill creates debt Plans and the standing-landing or `/save-plan`
  follow-up Plans below. Report a capability addition for main-agent routing by
  the deciding test in `Documents/AGENTS.md` `## Planning Trees`; do not
  disguise it as debt.
- Reject each proposal class [`../SKILL.md`](../SKILL.md) `## When to use`
  excludes, stating why.
- A proposal that arrives with the rows of one key of
  `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` is a
  tooling-friction proposal, a separate category; this worker deletes those
  rows as that file's header directs.
- For a tooling-friction proposal or any proven reproducible problem whose
  evidence cannot confirm root cause (confirmed root causes keep the full bar):
  observed symptom and evidence substitute for confirmed root cause across
  `## Steps` and these rules, using the investigation portion in
  `Documents/Plans/AGENTS.md` `## Plan files` and deferring root-cause proof to
  the fix session via `## Design`. Grouping and duplicate detection key on the
  symptom (rejecting re-observed duplicates). For tooling-friction, the incident
  rows (owner, symptom, evidence) also substitute for the unmet criterion or
  residual, the grouping key is the incident key, and pre-existing/out-of-scope
  proof verifies the owner is outside any claimed Plan's `## In scope` (inside
  is an in-scope blocker, rejected as a friction follow-up).
- For a standing-landing or `/save-plan` follow-up (`../SKILL.md`
  `## When to use`), durable event evidence — affected paths, reviewed source
  revision if any, and observed change, pending check, or open question (no
  unborn landing hash; `/save-plan` companions require the selected
  repository-relative target path in brief and Plan before the file exists) —
  substitutes for step 1's proofs, keyed on paths and item. Draft a debt Plan
  if a fix is known; otherwise (user checks/judgment or `/save-plan`
  companions) draft `## Unresolved decisions` per `Documents/Plans/AGENTS.md`
  `## Plan files`: a standalone user check carries its procedure and expected
  result there and requires the user's observed result as completion evidence
  (never defaulted by generic `/next-plan` approval).
- Plan files and the metadata line that must be their very first bytes are
  ordinary tracked Git content; write them directly, with or without a live Plan
  claim. `Documents/Features` remains manual and is not an alternate
  executable-Plan store.
- Report a written Plan as tracked content; do not describe it as claimed or
  landed until that has actually happened.
