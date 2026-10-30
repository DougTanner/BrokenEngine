# Create follow-up Plans — worker

## Steps

### 1. Prove and consolidate proposals

For every proposal:

1. State the false required condition and its originating criterion, accepted finding, or residual.
2. Confirm root cause and unresolved state from current source or direct logs. Record durable `path:line`, symbol, and observed behavior evidence. A problem proven and reproducible from that evidence whose root cause that evidence cannot confirm takes the symptom substitution in `## Rules` instead.
3. Prove it is pre-existing or outside the approved implementation boundary.
4. Preserve user decisions and the authority order in root `AGENTS.md` `### Diagnosis Discipline`; report contradictions.

Group items only when root cause, implementation boundary, invariant, and verification strategy all match. Split stages by the stage rule in `Documents/Plans/AGENTS.md` `## Plan files`, and split architectural decisions, separate subsystems, or meaningfully different risks.

Search all live plan files by symbols, paths, root-cause terms, outcome, and `## Coordination`. A plan is a duplicate when it owns the same root cause and implementation boundary. Map the proposal to it unless the proven unmet acceptance criterion requires extending that plan.

Done when every surviving proposal carries the evidence step 1 records — or its symptom substitute under `## Rules` — and a duplicate decision.

### 2. Draft and classify

Choose the owning area `Documents/Plans/AGENTS.md` `## Plan files` lists and a
concise PascalCase filename; never overwrite a collision. Draft the smallest
decision-complete plan with `# Title`, `## Context`, `## Design`,
`## Critical files`, `## In scope`, `## Out of scope`, `## Acceptance criteria`
when the diff is insufficient, and `## Notes`. Include verified root cause (for
a proposal whose root cause step 1's evidence cannot confirm, its symptom
substitute under `## Rules`, with the investigation portion
`Documents/Plans/AGENTS.md` `## Plan files` defines), originating gap,
implementation boundary, and applicable determinism/CRC,
serialization/`.pack`/`kiVersion`, replay, wire, affinity, threading,
allocation, shader, build, or live-verification exposure.
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
that trigger in the Plan. Put only directional prerequisites in `dependsOn`; put
mandatory nondirectional constraints in reciprocal standard `## Coordination`
sections in every affected Plan. A follow-up authored during a `/next-plan` run
never names the Plan that run is completing;
[`claim-results.md`](../../next-plan/references/claim-results.md#claim-exit-result-fields)
shows why the edge cannot survive completion. Do not add score, effort ranking, queue tier, queue
row, request file, or claim data.

For a tooling-friction proposal, draft the body from [`tooling-friction-plan-template.md`](tooling-friction-plan-template.md).

Create the file with the repository-owned `.agents/scripts/New-PlanFile.ps1`:

```powershell
pwsh -NoProfile -File .agents/scripts/New-PlanFile.ps1 -Area <area> -Name <PascalCase.md> -Body <body file path> -DependsOn <plan paths as one comma-separated token>
```

Its parameters, the `-DependsOn` single-token rule, its result shape, and its exit handling are in `../../../references/new-plan-file.md`.

Done when the Plan body exists and its Change Workflow tier trigger is recorded in it.

### 3. Apply exactly one case

| Case | Tracked Plan bytes | Completion route |
|---|---|---|
| New Plan | Run `New-PlanFile.ps1` (step 2) to write the final Plan under `Documents/Plans/<area>/`; it mints the marker and never overwrites an existing path. | Take the tree validation from the script's folded result; route through `/finalize-changes` only when a landing gate applies. |
| Existing Plan, prose only | Edit the live tracked Plan directly, including reciprocal Coordination prose; preserve its marker byte-for-byte. | Validate and finalize the tracked edit. |
| Existing Plan, dependency change | Edit only the marker's `dependsOn` array plus required reciprocal Coordination prose; never change `createdUtc`. | Validate and finalize the tracked edit. |

After editing an existing tracked Plan, run the scheduler-state check as [`Documents/Plans/AGENTS.md`](../../../../Documents/Plans/AGENTS.md) documents it. If it reports `worktree-cli.missing`, stop and report that authorized primary maintenance through `/compile` is required. Require `status: valid` and `code: ok`; record its `notices` and `healedClaims`. Never inspect machine-local claims.

Once the row's route is taken, run the static-checks runner as
[`change-workflow.md`](../../../references/change-workflow.md)
`#### Step 5 — Run targeted pre-review checks` documents it, with the brief's
`Baseline` as the baseline SHA; the runner owns the limit. Rewrap in place
every `line-length` failure it reports in a Plan you wrote or edited, leaving
the metadata line byte-for-byte, and rerun until that row reports no failure in
those Plans. Failures in other files and other rows stay main's Step 5 work.

Never create a request file, row, score, claim, or a commit that publishes a row.

Done when exactly one row's route was taken for each proposal and a runner run
after your last rewrap reports no `line-length` failure in a Plan you wrote or
edited.

## Rules

- This skill owns the decision on each proposed follow-up, grouping, duplicate detection, area and filename placement, collision handling, dependencies, metadata, and Coordination decisions; callers supply evidence, not those decisions.
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
- Read `Documents/AGENTS.md` and `Documents/Plans/AGENTS.md` completely. Their current Plan shape, metadata, dependency, and Coordination rules override this skill. This skill creates debt Plans only. Report a capability addition for main-agent routing by the deciding test in `Documents/AGENTS.md` `## Planning Trees`; do not disguise it as debt.
- Reject each proposal class [`../SKILL.md`](../SKILL.md) `## When to use` excludes, stating why.
- Tooling friction observed during a `/next-plan` run is a separate category.
- So is a script or documented tool invocation that returned an oversized result flooding the main context, cited by the context-efficiency envelope (tool, invocation, measured size, checkpoint).
- For a tooling-friction proposal, and for any other proposal whose problem is
  proven and reproducible from the supplied evidence but whose root cause that
  evidence cannot confirm, these substitutions replace the corresponding
  root-cause-based requirements in these rules and in `## Steps` — the proof
  steps, the grouping and duplicate rules, and the drafted Plan's verified root
  cause; every other requirement still applies, and a proposal whose root cause
  the evidence confirms keeps the full root-cause bar unchanged. The observed
  symptom with its evidence substitutes for the confirmed root cause wherever
  one is required, including in the drafted Plan, which carries the
  investigation portion `Documents/Plans/AGENTS.md` `## Plan files` defines. For
  a tooling-friction proposal, that evidence is the in-session citation — exact
  command or script path, observed output or malformed result, and the rework or
  workaround it forced — which also substitutes for the unmet acceptance
  criterion or accepted residual. For a context-efficiency proposal, the
  envelope fields — tool, invocation, measured size — plus the separately
  supplied checkpoint substitute for the observed-output citation and for the
  rework or workaround it forced.
- For a proposal under the substitution above, proving the root cause is
  deferred to the fix session, by the route the Plan's `## Design` states.
  Grouping and duplicate detection key on the observed symptom instead of root
  cause; re-observing an already-recorded symptom updates nothing and is
  rejected as a duplicate. For a tooling-friction
  proposal, the grouping key is (skill or script, observed symptom), and the
  pre-existing and out-of-scope proof verifies that the misbehaving skill or
  script is outside the claimed Plan's `## In scope`; when it is inside, the
  failure is an in-scope blocker of the active change and is rejected as a
  friction follow-up.
- Plan files and the metadata line that must be their very first bytes are ordinary tracked Git content; write them directly, with or without a live Plan claim. `Documents/Features` remains manual and is not an alternate executable-Plan store.
- Report a written Plan as tracked content; do not describe it as claimed or landed until that has actually happened.
