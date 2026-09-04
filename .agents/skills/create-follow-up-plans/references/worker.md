# Create follow-up Plans — worker

## Steps

### 1. Prove and consolidate proposals

For every proposal:

1. State the false required condition and its originating criterion, accepted finding, or residual.
2. Confirm root cause and unresolved state from current source or direct logs. Record durable `path:line`, symbol, and observed behavior evidence.
3. Prove it is pre-existing or outside the approved implementation boundary.
4. Preserve user decisions and the authority order among user direction, approved plan, documentation, and current behavior; report contradictions.

Group items only when root cause, implementation boundary, invariant, and verification strategy all match. Split independently landable work, architectural decisions, separate subsystems, or meaningfully different risks.

Search all live plan files by symbols, paths, root-cause terms, outcome, and `## Coordination`. A plan is a duplicate when it owns the same root cause and implementation boundary. Map the proposal to it unless the proven unmet acceptance criterion requires extending that plan.

Done when every surviving proposal carries the evidence step 1 records — or its tooling-friction substitute under `## Rules` — and a duplicate decision.

### 2. Draft and classify

Choose the existing owning area and a concise PascalCase filename; never overwrite a collision. Draft the smallest decision-complete plan with `# Title`, `## Context`, `## Design`, `## Critical files`, `## In scope`, `## Out of scope`, `## Acceptance criteria` when the diff is insufficient, and `## Notes`. Include verified root cause, originating gap, implementation boundary, and applicable determinism/CRC, serialization/`.pack`/`kiVersion`, replay, wire, affinity, threading, allocation, shader, build, or live-verification exposure. Pre-stage architectural choices instead of deciding them. Phrase every choice you do make as the author's recommendation with its rationale rather than in binding language, per [`../../../references/authority-order.md`](../../../references/authority-order.md). Do not add unit tests or unsupported implementation detail.

Derive the future implementation's Change Workflow Tier 1/2/3 from the highest root `AGENTS.md` risk trigger and record that trigger in the Plan. Put only directional prerequisites in `dependsOn`; put mandatory nondirectional constraints in reciprocal standard `## Coordination` sections in every affected Plan. A follow-up authored during a `/next-plan` run never names the Plan that run is completing; the claim-exit step in [`../../next-plan/references/worker.md`](../../next-plan/references/worker.md) shows why the edge cannot survive completion. Do not add score, effort ranking, queue tier, queue row, request file, or claim data.

For a tooling-friction proposal, draft the body from [`tooling-friction-plan-template.md`](tooling-friction-plan-template.md).

Create the file with the repository-owned `.agents/scripts/New-PlanFile.ps1`:

```powershell
pwsh -NoProfile -File .agents/scripts/New-PlanFile.ps1 -Area <existing area> -Name <PascalCase.md> -Body <body file path> -DependsOn <plan paths as one comma-separated token>
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

Never create a request file, row, score, claim, or a commit that publishes a row.

Done when exactly one row's route was taken for each proposal.

## Rules

- This skill owns the decision on each proposed follow-up, grouping, duplicate detection, area and filename placement, collision handling, dependencies, metadata, and Coordination decisions; callers supply evidence, not those decisions.
- Inspect missing facts; never invent evidence or behavior.
- Read `Documents/AGENTS.md` and `Documents/Plans/AGENTS.md` completely. Their current Plan shape, metadata, dependency, and Coordination rules override this skill. This skill creates debt Plans only. Report a capability addition for main-agent routing to manual `Documents/Features/`; do not disguise it as debt.
- Reject an in-scope acceptance failure, including required structural work: it remains a blocker in the active change.
- Also reject stale, disproven, fixed, stylistic-only, and evidence-free proposals, stating why. Evidence-free proposals — "the skill felt awkward", no citation — stay rejected in the tooling-friction categories below too.
- Tooling friction observed during a `/next-plan` run is a separate category.
- So is a script or documented tool invocation that returned an oversized result flooding the main context, cited by the context-efficiency envelope (tool, invocation, measured size, checkpoint).
- For tooling-friction proposals only, these substitutions replace the corresponding root-cause-based requirements in these rules and in `## Steps` — the proof steps, the grouping and duplicate rules, and the drafted Plan's verified root cause; every other requirement still applies, and ordinary proposals keep the full root-cause bar unchanged. The observed in-session symptom with its citation — exact command or script path, observed output or malformed result, and the rework or workaround it forced — substitutes for the unmet acceptance criterion or accepted residual, and for the confirmed root cause wherever one is required, including in the drafted Plan. For a context-efficiency proposal, the envelope fields — tool, invocation, measured size — plus the separately supplied checkpoint substitute for the observed-output citation and for the rework or workaround it forced.
- Proven root cause is deferred to the fix session, by the route the Plan's `## Design` states. For the pre-existing and out-of-scope proof, verify that the misbehaving skill or script is outside the claimed Plan's `## In scope`; when it is inside, the failure is an in-scope blocker of the active change and is rejected as a friction follow-up. Grouping and duplicate detection key on (skill or script, observed symptom) instead of root cause; re-observing an already-recorded symptom updates nothing and is rejected as a duplicate.
- Plan files and the metadata line that must be their very first bytes are ordinary tracked Git content; write them directly, with or without a live Plan claim. `Documents/Features` remains manual and is not an alternate executable-Plan store.
- Report a written Plan as tracked content; do not describe it as claimed or landed until that has actually happened.
