<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-10T18:09:48.525Z","dependsOn":[]} -->
# Trim unconsumed `/next-plan-review` measurement machinery

## Context
A repository survey of workflow mechanisms that cost work on every run but change
no decision found four such mechanisms inside `/next-plan-review`. All four live
in the same three files, so one change can settle them together:

1. Execution-model routing (survey rank A2). The concern is defined in
   `.agents/skills/next-plan-review/references/measurement.md:107-142`, returned
   as the `Model-routing evidence` handoff field
   (`.agents/skills/next-plan-review/SKILL.md:58-61`), rendered as the routing
   table `.agents/skills/next-plan-review/references/report.md:24-25` and the
   executive-verdict line `.agents/skills/next-plan-review/references/report.md:11`,
   and listed as concern 5 in
   `.agents/skills/next-plan-review/references/concerns.md:61-63`. For an
   ordinary Claude child, compliance requires "child-session execution metadata
   naming the actual executor/model and effort"
   (`.agents/skills/next-plan-review/references/measurement.md:120-123`); when
   that cannot be proved "the verdict is `unverified`"
   (`.agents/skills/next-plan-review/references/measurement.md:126-128`) and
   "Every `violation` or `unverified` row is a cited finding"
   (`.agents/skills/next-plan-review/references/measurement.md:140`), while "no
   routing result is automatically P0"
   (`.agents/skills/next-plan-review/references/measurement.md:141-142`). The
   headless `/codex-review` chain
   (`.agents/skills/next-plan-review/references/measurement.md:123-126`) is a
   different case: it is provable from the commit-time
   `.codex/codex-review.ps1` model and effort pins.
2. Quantitative control-work time measurement (survey rank A3):
   `.agents/skills/next-plan-review/references/measurement.md:73-87`, the
   `Control-work evidence` handoff field
   (`.agents/skills/next-plan-review/SKILL.md:55-57`), the nine-column table
   `.agents/skills/next-plan-review/references/report.md:26-28`, and the share
   line `.agents/skills/next-plan-review/references/report.md:12`. The
   consuming ranking rule
   (`.agents/skills/next-plan-review/references/report.md:57-62`) keys only on
   the qualitative `candidate removable` label, and
   `.agents/skills/next-plan-review/references/measurement.md:89-90` states
   outright that "The control decision is independent of the time category";
   the numeric side degrades to `unverified` whenever `T = 0`
   (`.agents/skills/next-plan-review/references/measurement.md:86-87`).
3. The rendered markdown retrospective report (survey rank A4):
   `.agents/skills/next-plan-review/references/worker.md:133-135` tells the
   worker to write the report on the template
   `.agents/skills/next-plan-review/references/report.md:3-46`, but the handoff
   is the whole return (`.agents/references/subagent-handoff.md:23-32`), no step
   writes the report to a file, and the handoff fields
   (`.agents/skills/next-plan-review/SKILL.md:40-74`) already carry the same
   sections.
4. The fixed alternative-explanation question (survey rank A9):
   `.agents/skills/next-plan-review/references/worker.md:131-132`, answered into
   the `Assessment` field
   (`.agents/skills/next-plan-review/SKILL.md:52-54`). Its answer may be `none`
   and no step branches on it.

The survey rested on reading current text, not on measured run outcomes, so this
Plan validates each claim against the tree before removing anything.

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
Two stages. Stage 2 runs only after every stage-1 check for the mechanism it
edits holds. A mechanism whose stage-1 check fails is left exactly as it is: stop
and report that mechanism, make no partial edit to it, and continue with the
mechanisms whose checks did hold.

### Stage 1 — validation (read-only)
For each of the four mechanisms, run these named checks against the tree at
implementation time.

- Consumer check (all four): search `.agents/` and `Documents/` for the
  mechanism's own names — `Model-routing evidence`, `Control-work evidence`,
  `Execution-model routing`, `Control-work share`, `report.md`, and the
  alternative-explanation question's wording. The claim holds when every hit is
  inside the `/next-plan-review` package itself (definition, handoff field,
  report template, concern list) and no other skill, reference, or script
  consumes the value or branches on it. The check ends when that search result
  is in hand.
- Unprovability check (mechanism 1 only): confirm that no repository text
  documents a Claude child-session execution-metadata record naming the actual
  executor model and effort — search `.agents/` and `.claude/` for such a
  record. The claim holds when none exists, so the allowed evidence chain at
  `.agents/skills/next-plan-review/references/measurement.md:120-123` cannot be
  satisfied for an ordinary Claude child and the verdict rule at `:126-128`
  forces `unverified`. Confirm at the same time that the headless
  `/codex-review` chain is still provable, by reading the explicit model and
  effort pins in `.codex/codex-review.ps1`.
- Independence check (mechanism 2 only): re-read
  `.agents/skills/next-plan-review/references/report.md:57-62` and confirm the
  removal ranking consumes only the three-way label plus burden, frequency,
  unique signal, and safety risk, and consumes no measured time, share,
  coverage, or bound.
- Non-emission check (mechanism 3 only): confirm no step in
  `.agents/skills/next-plan-review/` or in
  `.agents/references/subagent-reporting.md` writes the rendered report to a
  file or hands it anywhere other than the handoff.

### Stage 2 — removal and downgrade
Recommended edits, each the smallest that removes the mechanism without
weakening a check that still has a consumer.

- Mechanism 1: keep the headless `/codex-review` proof and delete the ordinary
  Claude routing verdict. Recommended shape: reduce
  `.agents/skills/next-plan-review/references/measurement.md`
  `## Verify execution-model routing` to the headless route only, drop the
  `Model-routing evidence` handoff field, the routing table
  `.agents/skills/next-plan-review/references/report.md:24-25`, and the routing
  executive-verdict line, and reword concern 5 in
  `.agents/skills/next-plan-review/references/concerns.md` to the headless
  route. Rationale: the removed half cannot reach any verdict but `unverified`,
  and every `unverified` row is a mandatory cited finding, so the mechanism
  manufactures findings rather than detecting them.
- Mechanism 2: keep the three-way control label and its ranking rule; delete the
  numeric measurement — the time/share/coverage/bound paragraphs, the
  `Control-work evidence` handoff field, the nine-column table, and the share
  line. Keep the safeguard sentence "one quiet run is not removal evidence"
  (`.agents/skills/next-plan-review/references/measurement.md:92-94`) and keep
  concern 6's classification duty in
  `.agents/skills/next-plan-review/references/concerns.md:64-66`, reworded to
  the label only. Rationale: the skill's own text says the decision does not
  depend on the number.
- Mechanism 3: delete the rendered-report obligation and the template, and
  relocate — never delete — the ranking and recommendation rules at
  `.agents/skills/next-plan-review/references/report.md:48-62` into the
  `/next-plan-review` handoff declaration in
  `.agents/skills/next-plan-review/SKILL.md`, so they keep exactly one owner.
  Whether `.agents/skills/next-plan-review/references/report.md` is deleted
  outright or kept as the file holding
  those relocated rules is left to the fix session as the smaller edit of the
  two; if it is deleted, every link to it must go with it.
- Mechanism 4: delete the question clause and the trailing-answer requirement in
  the `Assessment` field description.

## Critical files
- `.agents/skills/next-plan-review/SKILL.md` — `## Handoff`
- `.agents/skills/next-plan-review/references/measurement.md` —
  `## Measure control-work share`, `## Verify execution-model routing`
- `.agents/skills/next-plan-review/references/report.md` — whole file
- `.agents/skills/next-plan-review/references/worker.md` — step 7's
  alternative-explanation and write-the-report clauses
- `.agents/skills/next-plan-review/references/concerns.md` — concerns 5 and 6

## In scope
- The stage-1 validation reads above, over the files this Plan cites
- In `.agents/skills/next-plan-review/references/measurement.md`: the numeric
  paragraphs of `## Measure control-work share` and the ordinary-Claude half of
  `## Verify execution-model routing`
- In `.agents/skills/next-plan-review/SKILL.md` `## Handoff`: the
  `Control-work evidence` and `Model-routing evidence` fields, the trailing
  alternative-explanation clause of `Assessment`, and the relocated ranking and
  recommendation rules
- In `.agents/skills/next-plan-review/references/report.md`: the routing table,
  the control-work table, the two executive-verdict lines those tables feed, the
  rendered template, and the relocation of the ranking rules
- In `.agents/skills/next-plan-review/references/worker.md` step 7: the
  alternative-explanation question clause and the write-the-report clause
- In `.agents/skills/next-plan-review/references/concerns.md`: concerns 5 and 6,
  and the concern count wherever this file or `worker.md` states it
- Link and reference repair inside the `/next-plan-review` package caused by the
  edits above

## Out of scope
- The headless `/codex-review` routing proof and
  `.codex/codex-review.ps1`
- `/next-plan-checkpoint-review` and
  `.agents/skills/next-plan/references/run-checkpoint.md`
- `.agents/references/subagent-handoff.md` and
  `.agents/references/subagent-reporting.md`
- The remaining seven concerns and their measurement rules
- Any transcript path, transcript text, or machine-local path in the repository

## Risk tier and invariants
Expected Tier 2: scoped behavior of one skill package's own workflow, handoff
contract, and reference files. Highest applicable trigger from
`.agents/references/risk-tiers.md` is "one subsystem's ... tool behavior". No
determinism, wire, serialization, threading, or trust surface is touched.
Escalate to Tier 3 only if the fix reaches root `AGENTS.md`,
`.agents/references/change-workflow.md`, or another skill package. Invariants:
each surviving concern keeps exactly one owning location; the removal ranking
rule survives relocation intact; no reference link is left dangling.

## Acceptance criteria
- Every removed mechanism's name no longer appears anywhere under `.agents/` or
  `Documents/` except in this Plan
- `/next-plan-review`'s remaining concerns are internally consistent: the concern
  list, the measurement rules, the handoff fields, and the worker steps name the
  same set with the same count
- The removal ranking rule and the "one quiet run is not removal evidence"
  safeguard are still stated exactly once and are reachable from
  `.agents/skills/next-plan-review/SKILL.md`
- The headless `/codex-review` routing proof still exists and still names the
  commit-time model and effort pins
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports the `validate-skill` and `markdown-links` rows passing
  for the changed package
- Any mechanism whose stage-1 check failed is unchanged in the diff and named in
  the session's report
