# Merging back-to-back implementer steps

Findings record for the Plan
`Documents/Plans/ChangeWorkflow/InvestigateMergingImplementerSteps.md`: should
the Change Workflow's back-to-back `implementer` steps in Candidate A and
Candidate B share one worker context? Answered from the repository at commit
72a61ce93. This record changes no workflow file; the user decides whether to.

- Candidate A: the preparation `implementer` (A1) and the Plan review step's
  fresh `implementer` that writes accepted Plan review findings into the plan
  snapshot (A2).
- Candidate B: the Implement and propagate step's `/implement-plan` slice
  `implementer` (B1) and `/update-affected-code` `implementer` (B2), and the
  Apply the triggered cleanup step's `/update-claude-docs` `implementer` (B3).

## Basis

Nothing in this record was measured from real runs. The Plan's transcript
research scanned the 60 most recent landings on `main` that delete a Plan file
and found almost nothing measurable:

- One landing (position 51 of 60) had a proven Claude transcript, and none of
  its 29 dispatches mapped to a candidate step: none of its briefs had a
  `Role:` line, which the mapping relied on, because older briefs lack it.
  Its dispatches also answered asynchronously, so each dispatch result held
  only a launch acknowledgment, and the query did not read where the handoff
  text arrives.
- 52 landings were skipped as not found.
  `.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1`
  searches only the transcript directories of worktrees that are still
  registered. Of 6 skipped landings checked directly, 4 were finder misses:
  their transcripts exist under worktrees that no longer are. The other 2 had
  no top-level transcript holding the hash that spans the commit.
- 4 had a Codex parent session, which the Claude-only transcript projection
  cannot read; for 2 the finder returned `needs-selection`; 1 had a transcript
  without the full landing hash.

Those counts come from the `## Counts` sections of the six research batch
indexes (untracked, under `Temp/merging-implementer-steps/`); the missing
`Role:` lines and the asynchronous answers come from
`Temp/merging-implementer-steps/batch-06/index.md` `## Blocked`, and the 6
direct checks from `Temp/merging-implementer-steps/finder-check/`. The user then
dropped the research and asked for an answer from the current repository
state instead. This record therefore uses, per candidate step:

1. Read set: the instruction files the step's worker is directed to read.
2. Carry: what its handoff or the next step's continuation capsule carries,
   and what the next step must re-read or re-derive.
3. Sizing: the read sets sized with `.agents/scripts/Measure-Tokens.ps1 -Json`
   (outputs under untracked `Temp/merging-implementer-steps/sizing/`, with the
   per-pair sums in `pairs.json`).
4. Constraint check: no resume, fresh context, role and effort, and Codex and
   OpenCode parity.
5. Decision rule: keep a pair separate when any constraint breaks; merge it
   when none breaks and its cold-start plus re-read overlap is at least a
   quarter of the later step's total; otherwise keep.

Read-set rule: a step's skill `SKILL.md` and `references/worker.md` whole, plus
each file or section they (or, for a step with no skill, its workflow text)
direct the worker to read, apply, or fill a field from. Conditional reads whose
condition the pair does not imply are excluded. The brief form is in every read
set because every brief is on that form, and so is the handoff form, which
every role definition and skill cites. A `##` section is sized up to the next
`##` heading.

Cold-start set, loaded by every fresh `implementer` before its brief:

| Client | Files and lines sized | Tokens |
| --- | --- | --- |
| Claude | `AGENTS.md` 1-115 (2,578); `.claude/agents/implementer.md` 1-11 (100) | 2,678 |
| OpenCode | `AGENTS.md` 1-115 (2,578); `.opencode/agent/implementer.md` 1-11 (100) | 2,678 |
| Codex | `AGENTS.md` 1-115 (2,578); `.codex/agents/opus.toml` 1-5 (157) | 2,735 |

Shared sections, named in the step tables by the short name in the first
column (bt-token-v1 tokens):

| Short name | Path and section | Lines | Tokens |
| --- | --- | --- | --- |
| brief form | `.agents/references/subagent-reporting.md` `## Task brief` | 23-112 | 1,222 |
| handoff form | `.agents/references/subagent-handoff.md` `## Handoffs` | 8-77 | 1,075 |
| card template | `.agents/references/execution-card.md` `## Template` | 8-48 | 578 |
| risk tiers | `.agents/references/risk-tiers.md` `### Risk tiers` | 3-17 | 296 |
| plan-audit inputs | `.agents/skills/plan-audit/SKILL.md` `## Inputs` | 33-50 | 298 |
| tier ceiling | `.agents/skills/verify-acceptance/references/worker.md` `## Tier evidence ceiling` | 32-45 | 186 |
| alternatives handoff | `.agents/skills/plan-alternatives/SKILL.md` `## Handoff` | 48-154 | 1,309 |
| workflow Step 2 | `.agents/references/change-workflow.md` `#### Step 2 — Prepare and explore alternatives` | 137-157 | 421 |
| workflow Step 8 | `.agents/references/change-workflow.md` `#### Step 8 — Verify the acceptance table` (Leftovers rule) | 256-283 | 550 |
| next-plan handoff | `.agents/skills/next-plan/SKILL.md` `## Handoff` | 201-320 | 1,912 |
| plan files | `Documents/Plans/AGENTS.md` `## Plan files` | 36-67 | 747 |
| C++ conventions | `.agents/references/cpp-conventions.md` (whole) | 1-57 | 1,603 |

The numbers below use the Claude and OpenCode cold-start set; `## Codex and
OpenCode` gives the Codex figures.

## Candidate A

Steps: A1 is the preparation `implementer`, either `/prepare-change` (Change
Workflow `#### Step 2 — Prepare and explore alternatives`) or the `/next-plan`
`## Steps` step 4 preparation `implementer`; A2 is the `#### Step 3 — Plan
review` fresh `implementer` that writes the Plan review findings main accepts
into the plan snapshot.

| Step | Instructed reads (lines, tokens) | Per-change inputs (not sized) | Carry |
| --- | --- | --- | --- |
| A1, `/prepare-change` | `.agents/skills/prepare-change/SKILL.md` 1-77 (956); its `references/worker.md` 1-67 (947); brief form; handoff form; risk tiers; plan-audit inputs; card template; tier ceiling; alternatives handoff; workflow Step 2; workflow Step 8; total 7,838 | Request text, the regions it names | Shared form plus `Prepared plan`, `Classification`, `Execution card`, `Candidate zero`, `Unresolved decisions` (its `## Handoff`); the plan file and card in `Temp/` |
| A1, `/next-plan` | `.agents/skills/next-plan/references/preparation-brief.md` 1-81 (1,079); next-plan handoff; brief form; handoff form; card template; alternatives handoff; workflow Step 2; workflow Step 8; plan files; total 8,893 | Claimed Plan, cited source | Shared form plus `Claim`, `Classification`, `Verification`, `Slices`, `Candidate zero`; the snapshot under `Evidence` with one `##` selector per section |
| A2 | brief form; handoff form; total 2,297; on the `/next-plan` route also the next-plan handoff's `Slices` bullet (lines 221-230, 167), the only field A2 fills there, total 2,464 | Capsule, snapshot or plan file, accepted findings | Receives a capsule naming the snapshot by path and selector, the accepted findings, and the next action, never A1's reasoning; re-reads what each finding's write-in needs; returns the shared form, plus `Slices` on the `/next-plan` route |

Sizing (bt-token-v1):

| Pair | Cold-start overlap | Re-read overlap | Later-step total | Share |
| --- | --- | --- | --- | --- |
| A1 (`/prepare-change`) to A2 | 2,678 | 2,297: brief form, handoff form | 4,975 | 100.0% |
| A1 (`/next-plan`) to A2 | 2,678 | 2,464: `Slices` bullet (inside A1's next-plan handoff), brief form, handoff form | 5,142 | 100.0% |

Constraint check, both routes:

- No resume: breaks. A1 ends with its handoff; the Plan review step's reviews
  then run on the snapshot A1 wrote, and A2 starts only after main accepts a
  finding (`change-workflow.md` lines 160-166). A merged worker would have to
  outlive that review gate, which root `AGENTS.md` `### Directives` (a
  finished subagent is never resumed) and `subagent-handoff.md` `## Handoffs`
  (no open-ended wait after the handoff) forbid. A merge would change that
  directive or the Plan review step's order.
- Fresh context: breaks. `change-workflow.md` line 166 requires a fresh
  `implementer` carrying a continuation capsule from the preparation for A2.
- Role and effort: same. Both are `implementer`: Claude Opus, effort high;
  Codex `opus.toml` (gpt-6.1-sol, medium); OpenCode `opencode/union-alpha`,
  no effort mapping (`change-workflow.md` lines 21, 43-44).
- Codex and OpenCode parity: both breaks are client-independent.

Recommendation: keep A1 and A2 separate on both routes. The overlap share
passes the quarter threshold, but the no-resume and fresh-context checks
break, so no section would be edited.

## Candidate B

Steps: B1 is the `/implement-plan` slice `implementer` and B2 the
`/update-affected-code` `implementer` (`#### Step 4 — Implement and
propagate`); B3 is the `/update-claude-docs` `implementer` (`#### Step 7 —
Apply the triggered cleanup`). B2 and B3 run only after a C++ or GLSL change,
so each pair implies one of the two but not C++ itself. B2 reads the C++
conventions unconditionally (its `references/worker.md` lines 7-9); B1 reads
them only for C++ changes (its `references/worker.md` lines 16-17), so B1's
read set excludes them.

| Step | Instructed reads (lines, tokens) | Per-change inputs (not sized) | Carry |
| --- | --- | --- | --- |
| B1 | `.agents/skills/implement-plan/SKILL.md` 1-94 (1,142); its `references/worker.md` 1-104 (1,522); brief form; handoff form; total 4,961 | Plan or snapshot, card, `AGENTS.md` files, sources | Shared form plus its five `## Handoff` fields, among them `Affected-site triggers` and `Propagation required` |
| B2 | `.agents/skills/update-affected-code/SKILL.md` 1-88 (938); its `references/worker.md` 1-76 (1,195); brief form; handoff form; C++ conventions; total 6,033 | B1's regions, handoff, and old/new contract notes (`## Inputs`) | Re-reads each changed region and its callers (worker step 1) and runs its own old/new searches (step 2), never inferring absence from B1's handoff (`SKILL.md` lines 44-48); returns `Trigger outcomes`, `Project membership trigger`, `Reviewer focus areas` |
| B3 | `.agents/skills/update-claude-docs/SKILL.md` 1-55 (681); its `references/worker.md` 1-82 (1,138); its `references/content-rules.md` 1-33 (1,057); brief form; handoff form; total 5,173 | Main's complete changed-file list (B1, B2, and any review-fix rounds), governing `AGENTS.md` chains | Takes only the file list and baseline from earlier steps (its `## Inputs`); re-reads every governing `AGENTS.md` its discovery script returns and compares it with current code (worker steps 2-3); returns the shared form with `AGENTS.md` sections under `Changed files` |

Sizing (bt-token-v1):

| Pair | Cold-start overlap | Re-read overlap | Later-step total | Share |
| --- | --- | --- | --- | --- |
| B1 to B2 | 2,678 | 2,297: brief form, handoff form | 8,711 | 57.1% |
| B1 to B3 | 2,678 | 2,297: brief form, handoff form | 7,851 | 63.4% |
| B2 to B3 | 2,678 | 2,297: brief form, handoff form | 7,851 | 63.4% |

Constraint check, B1 to B2:

- No resume: no break for a single slice; both run inside the Implement and
  propagate step with no gate between them. With several slices it breaks:
  that step's order (`change-workflow.md` line 172) runs the slices in parallel
  and `/update-affected-code` after all of them, so a merged slice worker would
  have to wait for its sibling slices or propagate before they finish, which
  changes that order.
- Fresh context: no break. Line 177 does not require a fresh or separate
  worker (compare line 166, fresh, and line 231, separate), and the
  independent-context rule in `subagent-handoff.md` `## Handoffs` covers
  review and verification, not propagation edits. The `SKILL.md` lines 44-48
  rule to construct its own searches is a method a same-context worker can
  still follow.
- Role and effort: same, as for Candidate A.
- Codex and OpenCode parity: no break; the merged step is one fresh dispatch
  under each client's mechanics (`subagent-reporting.md` `## Delegated
  execution context`). The brief form carries one `Skill:` line (line 29) in
  every client, so the merge belongs inside `/implement-plan`, not in a
  two-skill brief.

Constraint check, B1 to B3 and B2 to B3:

- No resume: breaks. The Run targeted pre-review checks and Review and resolve
  correctness steps sit between the Implement and propagate step and B3, and
  B3's changed-file list includes the review-fix rounds of the latter; a merged
  worker would outlive both gates. Moving B3 earlier would change the step
  order, including `#### Step 7`'s order that runs
  `/progressive-disclosure-review` after `/update-claude-docs` (line 249).
- Fresh context: no break; no text requires one.
- Role and effort: same, as for Candidate A.
- Codex and OpenCode parity: the break is client-independent.

Recommendation: merge B1 and B2 for a single-slice change: no constraint
breaks and the overlap share is 57.1%. Keep them separate for a multi-slice
change, where the step-order check breaks. Keep B1 and B3, and B2 and B3,
separate. The single-slice merge would edit:

- `.agents/references/change-workflow.md` `#### Step 4 — Implement and
  propagate`: the Order line and the `/implement-plan` and
  `/update-affected-code` bullets.
- `.agents/skills/next-plan/SKILL.md` `## Steps` step 8, which dispatches one
  `/implement-plan` `implementer` per `Slices` row.
- `.agents/skills/implement-plan/SKILL.md` `## When to use` and `## Handoff`
  (`Affected-site triggers`, `Propagation required`), and its
  `references/worker.md` `### Phase 1: Implement` (steps 5 and 7).
- `.agents/skills/update-affected-code/SKILL.md` `## When to use` and
  `## Inputs` (the implementation handoff input).

## Codex and OpenCode

Every recommendation holds in both. The Candidate A breaks and the B3 gates
come from the workflow and the no-resume directive, not from a client, and
each client dispatches a fresh context (Codex `fork_turns: "none"`, OpenCode
`task` without `task_id`). The B1 to B2 single-slice merge removes one
dispatch the same way in each client and needs no client-specific mechanism.
Both steps of every pair map to the same model and effort in each client.

Codex figures, with its 2,735-token cold-start set (re-read overlaps as in the
Candidate tables):

| Pair | Later-step total | Share |
| --- | --- | --- |
| A1 (`/prepare-change`) to A2 | 5,032 | 100.0% |
| A1 (`/next-plan`) to A2 | 5,199 | 100.0% |
| B1 to B2 | 8,768 | 57.4% |
| B1 to B3 | 7,908 | 63.6% |
| B2 to B3 | 7,908 | 63.6% |

OpenCode's role definition sizes the same as Claude's (100 tokens), so its
figures equal the Candidate tables.

## Limits

- Only instruction files in the current tree are sized. Per-change inputs
  (Plan, snapshot, card, changed sources, handoffs, findings, governing
  `AGENTS.md` chains) and the host's own system prompt are not, and in a real
  run they can outweigh the instruction files.
- A step's actual reads in a run may differ from its instructed reads.
  Conditional reads are excluded: the C++ conventions for B1,
  `update-claude-docs` `references/audit-mode.md`, the `add-collection-member`
  worker and
  `.agents/references/collection-layout-auditor.md` for B2, and
  `.agents/skills/next-plan/references/claim-exceptions.md` `## Deferral` for
  A1.
- The cold-start set alone is at least 30% of every later step's total (2,678
  of 8,711 at the smallest), so the quarter threshold passes for every pair and
  the constraint checks decide each recommendation.
- The value a later step adds was not observed: no site `/update-affected-code`
  updated that the slice missed, no `/update-claude-docs` edit, and no Plan
  review writer change was measured. Whether a same-context propagation pass
  misses sites a fresh one would catch is unknown, and the B1 to B2
  recommendation rests on the size rule and constraint checks alone.
- The unit is `Measure-Tokens.ps1`'s `bt-token-v1`: UTF-8 bytes of the
  LF-normalized text divided by four, rounded up
  (`.agents/scripts/AgentScriptCommon.psm1` `Measure-AgentTokenCount`), not a
  model tokenizer count; the shares are more reliable than the absolute
  numbers.
