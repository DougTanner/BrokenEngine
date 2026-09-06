<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T14:31:51.389Z","dependsOn":[]} -->
# Fix: `/next-plan` — step 4 names no skill for the preparation dispatch, so main improvises a `/prepare-change` brief that skill refuses

## Context
`.agents/skills/next-plan/references/worker.md:26-28` (step 4) says only
"Dispatch one preparation `implementer` to verify every Plan statement against
current code, on the single task brief in
[`../../../references/subagent-reporting.md`]" — it names no skill for that
dispatch, and it does not say what the brief's `Skill:` field carries. The
shared task-brief form does allow `Skill: none`
(`.agents/references/subagent-reporting.md:25` reads
`Skill: <skill to run, or none>`), but step 4 says so nowhere, and the only
preparation skill in the repository declines the job:
`.agents/skills/prepare-change/SKILL.md:29-30` says "Not for a claimed
executable Plan: [`../next-plan/SKILL.md`] keeps its own preparation route."

Observed in this run: main improvised the brief field as
`Skill: /prepare-change (preparation for a claimed executable Plan under
/next-plan — verify, do not implement)`, and then had to override that skill's
own `## Handoff` extension fields (`Prepared plan`, `Unresolved decisions`,
`.agents/skills/prepare-change/SKILL.md:52-55`) with the different extension
fields `/next-plan` declares for the same handoff (`Claim`, `Classification`,
`Execution card`, `Findings`, `Build required`, `Residuals`;
`.agents/skills/next-plan/SKILL.md:35-51`) by listing them in the brief's
`Required sections`. The workaround cost: a hand-written parenthetical that
contradicts the named skill's own `## When to use`, and a preparation worker
holding two conflicting handoff contracts at once, which returned a handoff
deviating from the declared form (two `Findings` fields, multi-sentence rows).

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 6bfec60e-b442-4699-9834-5c47831921ab
- Worktree/branch UUID: 72b90f6a-03c3-42c2-9126-73abe30e50ea
- Session branch: claude/72b90f6a-03c3-42c2-9126-73abe30e50ea
- Worktree: .claude\worktrees\BrokenEngine\72b90f6a-03c3-42c2-9126-73abe30e50ea
- Landing ref: claude/72b90f6a-03c3-42c2-9126-73abe30e50ea
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/NextPlanPreparationDispatchContract.md`, but a
  periodic Plan-history squash can make it return an unrelated aggregate commit,
  so review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref
above — supplying the recorded client and the recorded conversation session ID.
Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author's recommendation, for the fix session to confirm or replace: say in
`/next-plan`'s own step 4 what the preparation brief carries, so the brief stops
borrowing `/prepare-change`. Concretely, step 4 of
`.agents/skills/next-plan/references/worker.md` names the `Skill:` value main
writes — `Skill: none`, which the shared form already permits, or the
`/next-plan` preparation route itself — and names the `## Handoff` extension
fields `.agents/skills/next-plan/SKILL.md` already declares as the worker's
return contract, so exactly one handoff contract binds the preparation worker.
That is smaller than widening `/prepare-change` to accept claimed Plans or
adding a new reference file: `/next-plan` already owns and declares the
preparation handoff fields, so step 4 only needs to point at them, and
`/prepare-change`'s exclusion line can then stay as written. A companion
touch-up of that exclusion line, to say where the
`/next-plan` route is written down, is inside scope only if the fix changes what
a reader of that line must do next.

## Critical files
- `.agents/skills/next-plan/references/worker.md`
- `.agents/skills/next-plan/SKILL.md`
- `.agents/skills/prepare-change/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the regions enumerated here: step 4 of
  `.agents/skills/next-plan/references/worker.md`, the `## Handoff` section of
  `.agents/skills/next-plan/SKILL.md` only if step 4's pointer needs a matching
  anchor there, and the "Not for a claimed executable Plan" lines of
  `.agents/skills/prepare-change/SKILL.md` only if the fix changes what that
  line tells a reader to do

## Out of scope
- The landed change the session produced
- The content of the `## Handoff` extension fields themselves and the execution
  card template in `.agents/skills/next-plan/SKILL.md`, which this Plan does not
  reopen
- The shared task-brief and handoff form in
  `.agents/references/subagent-reporting.md`
- The delegation role table in the root `AGENTS.md`, including which role
  performs the preparation
- Any other `/next-plan` step, its scripts under
  `.agents/skills/next-plan/scripts/`, and the claim lifecycle
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 by the root `AGENTS.md` Tier-2 trigger for one subsystem's tool
behavior — the scoped instruction behavior of one skill package at an existing
boundary, with no determinism, wire, serialization, threading, or trust-boundary
exposure; escalate if the fix reaches build/bootstrap coordination. Invariants to
preserve: `/next-plan` keeps its own preparation route rather than delegating to
`/prepare-change`; the preparation worker binds to exactly one declared handoff
form; the public/private skill split, so `SKILL.md` keeps only what a parent
needs to dispatch and the steps stay in `references/`; progressive disclosure, so
the contract is written once and referenced elsewhere. Never embed transcript
paths or home paths.

## Acceptance criteria
- A `/next-plan` run's step 4 brief names its `Skill:` value and its handoff
  contract from repository text alone, with no hand-written parenthetical
  redefining a skill's `## When to use` and no `Required sections` override of
  another skill's declared handoff fields
- The preparation worker's handoff fields are declared in exactly one place
  reachable from the brief
- /validate-skill passes wherever the root AGENTS.md Apply the triggered
  cleanup step triggers it; plan validate exits 0
