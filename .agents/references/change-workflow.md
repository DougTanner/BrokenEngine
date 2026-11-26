<!-- session-context-part: Context management, delegation, and main-session conduct -->
This file, `.agents/references/change-workflow.md`, carries the Change Workflow,
the delegation rules, and the main-session conduct rules for the main session,
delivered at session start.

## IMPORTANT: Context management and agent selection

### Subagents

- Main session is manager; subagents execute work to keep main context clean.
- Subagents must not spawn subagents. Only main-session skills request
  delegation; a subagent needing delegated work returns the requirement to its
  caller.
- Give subagents only the instructions and context their task needs; they return
  a short handoff that main routes. Handoff format:
  `.agents/references/subagent-handoff.md`. Task brief, waiting, and worker
  interruption/recovery: `.agents/references/subagent-reporting.md`.
- While workers run, continue useful independent work when available; avoid
  unnecessary status checks, duplicating their investigation or implementation,
  and context-heavy work unless it advances a useful independent task.
- Cite readable review outputs (`Temp/`, Codex `<out>`) in write-in,
  `/resolve-findings`, and re-review briefs once in the skill's named field (or
  `Scope` if none is named) by path/selector, finding IDs/locators, and
  dispositions only — never pasting or paraphrasing finding claims, evidence, or
  proposed fixes; pass the compact finding line only when no readable output
  holds a finding.

### Delegation roles

This table owns the work assignments. Each role is defined at
`.claude/agents/<subagent_type>.md` for Claude Code,
`.codex/agents/<subagent_type>.toml` for Codex, and
`.agents/agents/<subagent_type>.md` for Antigravity CLI; the definitions own
model and effort settings. Skills name a role and describe the work.

| `subagent_type` | Work |
| --- | --- |
| `planner` | Preparation, plans, design |
| `reviewer` | Every independent findings-only review and audit except `/comment-review`, including `/next-plan-review` (run directly in one fresh reviewer); adversarial review that tries to disprove the change |
| `implementer` | Implementation, propagation, docs, follow-up plans, finalization |
| `researcher` | Research requiring judgment; approach options for /plan-alternatives; `/code-quality-metrics` |
| `locator` | Exploration, search, log filtering, spec fetch, claim verification — returns file:line, quotes, or links |
| `builder` | `/compile`, which owns the return contract and, in `## Purpose`, its one exception |
| `runner` | `/agent-harness` runs dispatched on their own |
| `mechanic` | Checklist edits — `/code-style-review`, `/update-vcxproj`; the findings-only checklist review `/comment-review` |

- Delegate only by a `subagent_type` from the table above, including inside
  plan mode; an ad-hoc `model:` cannot lock in effort.
- Non-Codex hosts (Claude Code, Antigravity CLI) dispatch every skill listed by
  `pwsh -NoProfile -File .agents/scripts/Invoke-Codex.ps1 -ListRoutes` (only
  Step 11 lens reviewers for `/external-architecture-review`, keeping its
  orchestrator and `/verify-external-claims` in main) through that launcher,
  following `.agents/references/codex-bridge.md`; unrouted skills and modes
  (such as `/session-audit`, `/update-vcxproj`, and `/over-engineering-review`
  validator) and non-review roles stay native. Codex sessions use native Codex
  subagents and never use the Codex bridge.
- Do not follow review findings blindly. Use judgement on each one: accept it
  when the failure is real and reachable, and be especially careful with
  findings that add guards, options, or machinery for cases nobody has observed
  (YAGNI and over-engineering). Reject a finding only with evidence the
  reviewer could have read — a file and line, or a recorded user decision; that
  main prefers the plan, or wants to reject the finding, is not evidence. When
  the reason lies outside the repository and is not yet recorded, put the
  finding to the user instead of rejecting it. Add plan or document text only
  when a human reader or the implementer needs it; a reviewer's missing context
  is not that need.

## Main-session conduct

### User Interaction

- IMPORTANT: Every question or decision request aimed at the user must be
  answerable from the current message without hidden reasoning or remembered
  scrollback. Provide the FULL context needed to understand as rendered message
  text the user is guaranteed to see — text emitted before a question-tool call
  might not be displayed, so present first and ask only after the context is
  visible — and explain what the answer changes or blocks. When relevant, give
  options, trade-offs, and a recommendation. The user has NOT read the source
  code or plan file.
- AVOID jargon: the user is NOT a domain expert. Use everyday words and short
  sentences. Define an unavoidable technical term in one clause the first time
  it appears.
- Explain fully when asked; use headings and bullet points so a longer
  explanation stays skimmable.
- Reporting work: state what was built and what was verified separately, and
  name every required check still open; "done" means those checks have closed. A
  defect is stated with its evidence and effect.
- Comparing options: give each the same criteria, evidence, detail, and tone;
  recommend one without selling the favorite by benefit and the alternative by
  risk; prefer removing a cause to suppressing a symptom. Pros and cons cover
  only how the option leaves the codebase (less code, root cause removed, better
  structure, runtime cost, invariants kept), never review status, departure from
  a drafted plan or current documents or code, touched wire, trust, or invariant
  surfaces as a cost in themselves, amount of change, or effort; a plan's scope
  or author's choice counts only when the user set it
  (`.agents/references/authority-order.md`). State shared facts and constraints
  once above the options, leaving only unique points under each. In explanatory
  prose, state outcomes, not code mechanics, omitting file paths, function
  names, and flags unless choosing names; keep declarations and representative
  usage for interface decisions. Open each option with `Option 1:`, `Option 2:`,
  and so on, and reuse those labels in any question naming options. Put **Pros**
  and **Cons** in separate bullet lists, with a blank line before and after
  every heading, label, and list.
- Footer: only the main-session message that asks the `/finalize-changes`
  landing confirmation question carries it; that message ends with
  `Follow-up Plans created:` and the single word `none`, or one line per Plan
  created since the session's last landing: its repo-relative path; a tag,
  `unlanded in worktree` or `executed in this change` (folded in by the
  Leftovers do-now rule); and whether to do it this session while its context
  is fresh: `do now: yes` when it is Tier 1 or 2, bounded, and needs only files
  and facts already read or changed this session (so it adds little new
  context), otherwise `do now: no` with the failed condition in a few words.
  Example:
  `Documents/Plans/<area>/<Plan>.md — unlanded in worktree — do now: no (needs files not read this session)`.
  The verdict answers; the user decides.

### Resolving Ambiguity

- Non-trivial ties (two viable approaches, neither architectural): fan out
  `researcher` subagents to validate each, compare pros/cons, then pick the
  simplest good solution.
- Architectural decisions (new system shape, public API, data layout, threading
  model), and any reading of the request that changes observable behavior and
  that neither the request nor a build, an existing harness command, or a code
  read settles: stop and ask the user, presenting the problem, proposed
  solutions, and pros/cons of each.
- Races: a race is reachable when an existing actor — another thread, another
  process, or a repository tool such as DataPacker — can perform the
  interleaving under supported use. Remove a reachable race by construction
  (ordering, a lock, an exclusive handle, an atomic publish); rarity is no
  reason to keep it, because a rare timing failure is one a single user is
  unlikely ever to see or report. Treat a race no existing actor can perform as
  impossible or ASSERT it.
- A race no existing actor can perform, and a hand-edited file state nobody has
  seen and no named trigger produces, count for nothing when comparing options
  and are settled without asking the user. Trust-boundary validation
  (`.agents/references/cpp-conventions.md`) still applies to such a file.
- Questions a build, an existing harness command, or a code read settles: settle
  them that way, through the role that runs it.

<!-- session-context-part: Change Workflow, Steps 1-3 (continues in the next part) -->
## IMPORTANT: Change Workflow (YOU MUST follow this when changing anything tracked in this repository)

This workflow governs every tracked artifact. An artifact type a step does not
name is a case this workflow does not assign to anyone: resolve it with the
user.

A sweep run, defined by `/sweep`, does not follow this workflow; `/sweep` owns
what it keeps.

For Tier 1 and Tier 2, the user's request is the approval. Classify the work,
make the smallest complete change, run the checks that fit its size, and report
changed files, the checks that settled it, and residuals. No approval round-trip
or report file unless a landing gate applies.

Definitions:

- Execution card — the pre-implementation record the Step 2 preparation drafts
  (`/prepare-change`, using `mode: claimed-plan` for a claimed Plan) and
  `/plan-audit` audits; after approval it carries the approved scope before any
  dispatch cites the card. Field template: `.agents/references/execution-card.md`.
- Landing gate — the finalizer's acceptance table plus the `/finalize-changes`
  landing flow with one explicit user confirmation; applies whenever primary
  will be changed: landing a session's work, or executable Plan completion or
  rejection. Shared AgentTools promotion and Tier-3 integration always land
  through it.
- Executable Plan — tracked `Documents/Plans/**/*.md` with the byte-zero
  scheduler metadata marker (`Documents/Plans/AGENTS.md`
  `## Git-backed scheduler`). `Documents/Features` is manual.
- Wrapper session — session started through
  `.agents/scripts/Start-AgentWorktreeSession.ps1`, owning an isolated
  worktree. A retained wrapper session reattaches only through its
  `-ReattachWorktree` input, whose command `/eod` gives. If the user opens a
  session directly on main, work in that checkout and do not use the worktree
  flow.
- Primary — the shared main checkout and its main branch that finished session
  work lands into.
- Step and stage — a step is one of the nine numbered Change Workflow steps
  below; outside this file a step is cited by its heading name
  (`the Plan review step`), so renumbering here changes nothing elsewhere; a
  stage is one approved unit of session work that can complete or land
  independently.
- Residual — a known leftover problem reported at the end of a task instead of
  fixed inside it.

Risk tiers: `.agents/references/risk-tiers.md`.

### Steps

`/next-plan` owns executable Plan selection and claim lifecycle.

#### Step 1 — Approve and classify

- `planner` runs `/prepare-change` — Tier 2+, or any tier where classifying the
  work needs repository evidence.

From user intent and any such preparation, main locks in the objective, the
approved stage decisions, the tier and its triggers, the roles, and the
acceptance checks. Tier 1 and Tier 2 authorize implementation; Tier 2, Tier 3,
and a Plan claim also require an execution card, which `/plan-audit` needs.

#### Step 2 — Prepare and explore alternatives

Order: `/prepare-change` first at Tier 2+, because the alternative
investigations need the drafted plan's objective and mechanism; at Tier 1 there
is no plan file, so main briefs from the Step 1 preparation where that step's
`/prepare-change` bullet applied, and from the request otherwise. Then
`/plan-alternatives` when its trigger fires. A chosen alternative returns to
`/prepare-change` for a redraft before Step 3; the skill owns the claimed-Plan
and Tier-1 routes.

- `planner` runs `/prepare-change` to prepare the plan — Tier 2+.
- main dispatches one `researcher` per axis for `/plan-alternatives`,
  concurrently and blind — every tier, on `/plan-simplicity-review`'s trigger
  (that skill's `## When to use`): Tier 1 axis 1 (Reuse), Tier 2 axes 1-2 (adds
  Remove the need), Tier 3 axes 1-3 (adds Reshape). Each brief is the shared
  task-brief form with `Skill: /plan-alternatives`, the objective, evidence
  paths under `Governing paths:`, the tier, fixed user decisions, the assigned
  axis, and one candidate-zero line naming the drafted mechanism. When a
  preparation handoff returned a `Candidate zero` field other than `none`, main
  writes that line from its `Mechanism` row, read inline or in the file section
  the handoff's `Evidence` cites for it, and reads no other part of the plan
  snapshot or plan file. Blindness: no field, in any form, positive or
  negative, carries the plan's rationale, its `## In scope`/`## Out of scope`
  sections, or the plan file's path; `Governing paths:` names no path under
  `Temp/`, `Documents/Plans/`, `Documents/Features/`, or
  `Documents/Investigations/` except those three `Documents/` trees'
  `AGENTS.md`, and main restates a fact held only in such a path as brief text
  under the same exclusions. Main compares the handoffs per that skill's
  `## Handoff` and presents per that skill's `### User presentation`.

#### Step 3 — Plan review

Order: `/plan-audit` and `/plan-simplicity-review` in parallel alongside any
eligible `before the first edit` unedited-tree checks below; then
`/verify-external-claims` for the requests the audit handoff raises; then, at
Tier 3, `/external-grill-plan` rounds until the plan is decision-complete, with
`/verify-external-claims` between rounds; then, when it triggers, the difference
`/plan-simplicity-review` after the accepted findings are last written into the
plan snapshot.

- `reviewer` runs `/plan-audit` — Tier 2+; Tier 1 skips it.
- fresh `reviewer` runs `/plan-simplicity-review` on the same plan snapshot —
  every tier, when the plan adds new code or changes non-documentation behavior
  (both defined in that skill's `## When to use`); when unsure whether a plan
  triggers it, dispatch it.
- for eligible `before the first edit` checks on the unedited tree (if the card
  has any): search that literal in `Acceptance checks` alone (`- Acceptance
  checks:` to `- Roles:`, matched text only), then read each match to its end
  for executor, build (project, configuration, `Local` form per
  `.agents/skills/compile/references/runtime-data-mode.md` `## Mode selection`,
  or none), and steps. Run `builder` `/compile` if needed (in that `Local`
  form), then `runner` `/agent-harness` or `implementer` for non-harness checks,
  recording baseline SHA (and Coretide `HEAD` for `Projects/Coretide/`) in each
  brief. Defer to Step 4 checks interleaving edits with builds (`/compile`
  `## Purpose`), needing unauthorized flags (`-AllowGaeaExport`, PREfast,
  clang-tidy per `/compile` `## Inputs`), or running `Local` `export` or
  tracked-input-rewriting builds (Step 3 stays non-mutating: `Shared`, `Local`
  `no-effect`, or non-build). Live Step 3 pre-edit workers (and `Local`
  `no-effect` temp cleanup) finish before `/next-plan` step 6
  (`Invoke-NextPlanClaim.ps1`) and Step 4 slices; non-`none` `Changed files`
  (`/compile` `## Handoff`) from a Step 3 `builder` blocks before claim refresh.
- `planner` owns `/external-grill-plan` repository evidence and short written
  decision summaries, updated round by round — Tier 3 only.
- main runs `/verify-external-claims`, dispatching one `locator` as its evidence
  worker — Tier 2+, for the external claims a `/plan-audit` handoff raises, and
  at Tier 3 also for the claims a grill round raises.
- only the fresh `planner` with a preparation continuation capsule writes
  accepted Plan review findings into the plan snapshot — Tier 1-2 with a plan
  snapshot, after main accepts a finding; Tier 3 follows
  `.agents/skills/next-plan/references/tier3-workflow.md` `## Plan review`.
- fresh `reviewer` runs `/plan-simplicity-review` over only the difference those
  findings added to the plan snapshot — Tier 2+, once per Plan review, when the
  findings main accepted from `/plan-audit` or a Tier-3 grill round add
  mechanism (a new guard, check, option, or code path) rather than only
  correcting statements.

At Tier 3 main only decides and interviews from the `planner` and `locator`
handoffs, then presents the resolved plan for approval per `/next-plan`
`### Implementation approval`; the brief and iteration contract is in
`.agents/skills/next-plan/references/tier3-workflow.md`.

<!-- session-context-part: Change Workflow, Steps 4-6 (continued from the previous part; continues in the next part) -->
#### Step 4 — Implement and propagate

Order: after Step 3's pre-edit join, run (or rerun) on the unedited tree any
deferred, unstarted (e.g., Tier 1, via Step 3's lookup), or stale `before the
first edit` check — stale if its definition, `Local` form, engine baseline SHA,
or (`Projects/Coretide/`) Coretide `HEAD` changed during Plan review,
`/external-grill-plan`, approval, or claim refresh — reusing unchanged Step 3
results; then slices run in parallel.

- remaining or stale `before the first edit` checks use Step 3's executor rule;
  edit-interleaved checks follow `/compile` `## Purpose` (executor runs builds).
- `implementer` runs `/implement-plan` for each disjoint slice — every tier.

When the slices return, main runs
`pwsh -NoProfile -File .agents/scripts/Get-ChangeWorkflowSteps.ps1 -RepositoryRoot '<worktree root>' -Baseline <baseline SHA> -Tier <tier>`,
adding `-Landing` when the stage lands in this session and `-HarnessChecks`
when the execution card's `Roles` field names `runner` (main reads that field
alone). Main dispatches rows per the script header's `Output:` contract. A
zero-dispatch row satisfies non-harness dependents immediately, never
`runner /agent-harness`, which requires the emitted `/compile` row's
matching-build condition. Waiting on a Step 6 review row waits until that
review settles (disposition, `/verify-external-claims`, accepted fixes,
affected rebuilds, and re-reviews), not merely a first-round `NEEDS_ACTION`.
Main reruns the script as the Review and resolve correctness step states. The
script owns triggered dispatches, roles, scopes, and order, and each brief
carries its skill's `## Inputs`.

Main splits the work into disjoint slices where possible. Review-fix exceptions
belong to `/resolve-findings`.

#### Step 5 — Run targeted pre-review checks

Focused implementation self-checks remain inside the implementation slices.
The static-checks runner is
`pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot '<worktree root>' -Baseline <baseline SHA> -IncludeUntracked`;
it selects its own checks, and its header documents the result and options.
Main has an `implementer` rewrap every `line-length` failure it reports,
whether or not the change added the line; dependent rows wait until that rewrap
finishes.

Each `Build required` handoff compiles as it arrives, waiting only for its
row's `after` clause.

Full landing-gate builds and acceptance sign-off remain acceptance-table work
(the step script schedules post-edit `/agent-harness`).

A path nothing reaches today (a rejection, ASSERT, or recovery path no existing
harness command or ordinary save file triggers) is verified by code reading,
plus the build when code changed; the execution card, when one exists, says so.
Never propose a new harness command or a hand-edited file only to observe it.
Temporary verification edits (`kTemp` logs, log-level changes, other temporary
code) are always allowed, with no permission asked and no backup; wrap each in
a block that opens with a `// TEMP START` line and closes with a `// TEMP END`
line. Removing a block also restores any pre-existing line its edit changed to
its baseline text, which Git holds.

#### Step 6 — Review and resolve correctness

At any tier main may add a `reviewer` running `/adversarial-review` on the
trigger its `## When to use` states.

Main runs `/verify-external-claims`, dispatching one `locator`, on the external
claims a review or `/resolve-findings` handoff raises, before deciding the
dependent finding or resolution. A separate `implementer` runs
`/resolve-findings` for each finding main accepts; then only the affected
regions are re-reviewed and retested, each `/resolve-findings` and re-review
brief following its skill's `## Inputs`, and a reviewer with no accepted
finding whose region a fix changed reruns as a fresh first-round pass. A second
round needs a reproducible blocker. Before any further `/resolve-findings`
attempt on a cause whose fix failed its retest, an `implementer` runs
`/external-diagnose-bug`; its proven cause returns to `/resolve-findings` or a
plan. When the `/over-engineering-review` validator or a round changes C++,
`/code-style-review` over the newly changed ranges, then `/compile` of its
`Build required`, run before `/repo-code-review` and `/comment-review` start
(after a round, as its re-reviews); no other review waits for them.
`/over-engineering-review` runs once per stage; a later round's re-review covers
its own fix. While a post-edit `runner /agent-harness` is live, hold any
`/compile` targeting the same executables or writing its Shared/Local data packs
(including Local ModelViewer or sibling builds sharing the data owner per
`.agents/skills/compile/references/runtime-data-mode.md`
`## Build invocation and derived properties`) and any writer to
`Common/DataFile.h` until `runner` releases its claim
(`.agents/skills/compile/references/worker.md` step 4, `/agent-harness`
`## When to use`), keeping `.agents/references/codex-bridge.md` `## Rules` for
`workspace-write` runs; after held edits or Step 6/7 C++, GLSL, or data-input
rebuilds, rebuild as needed, rerun `/agent-harness` (or affected checks per
`/agent-harness` `## Handoff`), and route `NEEDS_ACTION` to `/resolve-findings`.
Main reruns the step script after `/update-affected-code`,
`/update-claude-docs`, the validator, `/create-follow-up-plans`, and each
file-changing round, dispatching every newly triggered row; for post-Step-6
`/create-follow-up-plans` or `/update-claude-docs` non-C++ edits, pick the
`/coherence-review` pass per its `## When to use` (overlapping remaining
landing-gate or recovery `/compile` builds), resolve accepted findings on
affected regions, and run final `Invoke-StaticChecks.ps1` after those builds,
late writes, and fix rounds finish, before `/finalize-changes` preparation.

Main decides each finding once:

- A finding a later round's reviewer raises again at the same site for the
  same issue: when main rejected it, it stays rejected and is not decided
  again; when it was fixed, the fix failed its retest, and main decides the
  re-raise as a new finding.
- Churn, a new finding at the site of an applied fix that undoing that fix
  would resolve: first consider undoing or reworking that fix rather than
  patching on top of it.
- `/over-engineering-review` candidates are not findings: the validator's
  re-proof, not main, decides each one; the landing reconciliation records the
  pair as one review row from the validator handoff, or the finder's when the
  validator is skipped; the later reviews check the applied removals like any
  other changed bytes.
- Main puts each design suggestion from either pass to the user once before
  Step 9 under `### User Interaction` (accepted follows Step 8 Leftovers;
  unaccepted logs at Step 9), except under standing confirmation, which files
  it via `/create-follow-up-plans` instead of presenting or logging it.

Under standing confirmation (`/finalize-changes` `### Landing confirmation`),
outside-repository reviewer questions (`### Delegation roles`) likewise route
to `/create-follow-up-plans` while that section's continuation,
fix-and-retest, and stop policy governs.

<!-- session-context-part: Change Workflow, Steps 7-9, Convergence, and Risk tiers (continued from the previous part) -->
#### Step 7 — Apply the triggered cleanup

Dispatches: the step script's rows (Step 4).

#### Step 8 — Verify the acceptance table

- `implementer` runs `/create-follow-up-plans` — for every proven out-of-scope
  leftover not fixed inside the current change.

Leftovers:

- A proven leftover that is itself small (Tier 1 or 2, bounded, decidable from
  evidence already in hand), such as the same defect at a sibling site, is
  fixed inside the current change without asking: record it as approved scope
  (on the execution card when one exists), reclassify the whole change at the
  highest applicable tier, run every step that tier triggers for the touched
  regions (plan review included), and report the expansion in the message
  already being sent. This governs over a plan's `## Out of scope` entry that
  would route it to residuals or a follow-up Plan, except an entry the user's
  own words chose (`.agents/references/authority-order.md`).
- A follow-up Plan the footer marks `unlanded in worktree` as `do now: yes`,
  when the user directs it before the landing confirmation, is executed as
  approved scope of the current change regardless of the current change's
  tier, with the same recording, reclassification, and triggered steps; its
  Plan file is deleted from the worktree, and its support folder through
  `Remove-PlanSupportFolder.ps1` (`Documents/Plans/AGENTS.md` `## Plan files`),
  instead of landing, so nothing is claimed or completed.
- Every other proven out-of-scope leftover, including one the user directs to
  defer, goes through `/create-follow-up-plans`. One whose root cause is not
  yet confirmed need not be diagnosed first; its Plan's investigation portion
  (`Documents/Plans/AGENTS.md` `## Plan files`) leaves diagnosis to the
  executing session. Record every follow-up Plan and its support-folder files
  as approved scope regardless of when proven; author one proven before Step 6
  first so that round's reviews cover it, and route any added or edited after
  Step 6 (Step 8, `/next-plan` checkpoint, or `/finalize-changes` recovery)
  through Step 6's rerun and final static-checks rule.

#### Step 9 — Verify and land

- main adds to `Documents/Investigations/Engine/RefactorCandidates.md` the rows
  its header lists (excluding Step 6 suggestions filed via
  `/create-follow-up-plans`), from handoffs and context, reading no further
  code — before `/finalize-changes`; a changed log joins the landing set.
  Skipped for `/save-plan` or standalone Plan rejection (`/finalize-changes`
  `### Landing confirmation`).
- `implementer` runs `/finalize-changes` — at a landing gate.

Exactly one explicit user confirmation authorizes changing primary; after it
the finalizer advances primary under the landing lock. The confirmation
contract, the acceptance table, and what a later diff change re-triggers live
in `/finalize-changes`. A landing completes the stages it carries; the session
ends only when every stage is complete or explicitly deferred, with a tracked
follow-up Plan where required.

### Convergence

Once a stage's required checks pass, stop changing it: advance to the next
stage, approval gate, blocker, or session end without adding untriggered tests,
reviews, or process steps. Every step this workflow routes, and every valid
build, lock, or harness deadline, still applies; an unclear trigger is an
ambiguity to surface, not grounds to call a check untriggered.
