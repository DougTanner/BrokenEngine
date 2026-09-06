<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T00:48:52.659Z","dependsOn":[]} -->
# Fix: /plan-audit — a Tier-2 audit's `API Verification Requests` field has no routed consumer

## Context
`.agents/skills/plan-audit/SKILL.md:64` makes `API Verification Requests:` a
mandatory extension field of every audit handoff, at Tier 2 and Tier 3 alike,
and `:76` says the flow continues "after the manager decides on findings and
external verdicts" — so a Tier-2 audit is expected to produce external verdicts.
Nothing tells the manager how to obtain one at Tier 2. The root `AGENTS.md` Plan
review step routes the answering skill at one tier only: `AGENTS.md:98` reads
"main runs `/verify-external-claims`, dispatching one `locator` as its evidence
worker — Tier 3 only, for the external claims a grill round raises", and
`AGENTS.md:93` likewise binds that skill to the `/external-grill-plan` rounds,
which Tier 2 never runs.

Observed in this session's Tier-2 `/next-plan` run. The `/plan-audit` handoff
returned `API Verification Requests: PA-F-003 — under PowerShell 7 with
Set-StrictMode -Version Latest, does reading a property that does not exist on a
[pscustomobject] raise a terminating PropertyNotFoundException?`. Main resolved
the six returned findings and no Change Workflow step consumed the request; the
question stayed unanswered through implementation. It became moot only by
accident, because the implementation chosen for another finding removed the
property reads the claim concerned. Had it not, a Tier-2 plan would have been
implemented on an unverified external assumption with no owner for the check.

Note that `/verify-external-claims` itself carries no tier restriction — its
description at `.agents/skills/verify-external-claims/SKILL.md:3-8` names "a
Broken Engine review, plan audit, grill, or finding-resolution pass". The gap is
in the routing, not in the answering skill.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 40d5ea23-9635-484d-99d0-a21b2c0f9667
- Worktree/branch UUID: 75e972a1-4851-444c-b0a0-9d5076f1cf48
- Session branch: claude/75e972a1-4851-444c-b0a0-9d5076f1cf48
- Worktree: .claude\worktrees\BrokenEngine\75e972a1-4851-444c-b0a0-9d5076f1cf48
- Landing ref: claude/75e972a1-4851-444c-b0a0-9d5076f1cf48
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/PlanAuditTier2ApiVerificationRouting.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/75e972a1-4851-444c-b0a0-9d5076f1cf48` in bounded
friction mode, supplying the recorded client `claude` and the recorded
conversation session ID above. Then make the smallest fix inside the `## In
scope` boundary below.

Two resolutions were weighed when this Plan was written, and the evidence in
`## Context` favours the first:

- The author's recommendation: give the Tier-2 request a routed consumer.
  Amend the root `AGENTS.md` Plan review step so the `/verify-external-claims`
  bullet is no longer Tier-3-only — main dispatches it, with its one `locator`
  evidence worker, for the external claims a Tier-2 or Tier-3 `/plan-audit`
  handoff raises in `API Verification Requests`, and additionally at Tier 3 for
  the claims a grill round raises. Rationale: the field, its single-checkable
  request shape, and the answering skill all already exist and already accept a
  plan-audit request at any tier; the missing piece is one routing clause, so
  this is the smaller change and it leaves the audit handoff form untouched.
  Also confirm whether the step's `Order:` line at `AGENTS.md:93` needs the
  Tier-2 dispatch stated, since that line currently sequences
  `/verify-external-claims` only between grill rounds.
- The alternative, if root-causing shows the first is wrong: drop the field at
  Tier 2 and have `/plan-audit` fold an unverifiable external assumption into
  the finding row itself — a `Required` finding naming the check to run — so the
  manager decides it like any other finding. This needs no root `AGENTS.md`
  change, but it costs the distinction between a defect in the plan and an open
  external question, and it makes an unanswered question look resolved once the
  manager rules on the finding.

The implementing session picks one and states which, and updates
`.agents/skills/plan-audit/SKILL.md` in the same change only where the chosen
resolution makes its current handoff wording wrong. If root-causing shows the
fix lies outside the boundary below, surface it for re-planning instead of
expanding scope.

## Critical files
- `AGENTS.md` — the Plan review step's `Order:` line and its
  `/verify-external-claims` bullet (`:93`, `:98`), which state the tier at which
  the skill is routed
- `.agents/skills/plan-audit/SKILL.md` — the `API Verification Requests`
  extension field and the external-verdict wording (`:57`, `:64`, `:73`, `:76`)
- `.agents/skills/verify-external-claims/SKILL.md` — read-only reference for the
  request shape the field must satisfy; not expected to change

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the Plan review step's
  `/verify-external-claims` routing wording in root `AGENTS.md` and to the
  `API Verification Requests` / external-verdict wording in
  `.agents/skills/plan-audit/SKILL.md`

## Out of scope
- The audit's finding grammar, severities, and every other extension field
  (`Traceability checked`, `Required next step`)
- `/external-grill-plan` and the Tier-3 grill-round routing itself
- The internals of `/verify-external-claims`: its inputs, verdict form, worker
  contract, and its own `references/`
- `.agents/skills/session-audit/SKILL.md`, which carries a separate
  `API Verification Requests` section
- The landed change the observing session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Change Workflow Tier 2, on the root `AGENTS.md` trigger "one
subsystem's runtime or tool behavior" — the routing of one review-support skill
at an existing workflow step, with no new step and no new role; escalate if the
fix reaches build/bootstrap coordination or spans independently owned
subsystems. Invariants to preserve: `/plan-audit` stays findings-only and
delegates nothing further; `/verify-external-claims` stays read-only and keeps
its single `locator` evidence worker; the Change Workflow step is cited by its
heading name, never by number; progressive disclosure keeps the routing in root
`AGENTS.md` and the request shape in the skills. Never embed transcript paths or
home paths.

## Acceptance criteria
- A Tier-2 `/plan-audit` handoff returning a non-`none`
  `API Verification Requests` value has exactly one routed consumer readable
  from the changed text, and the recorded symptom — a request that no step owns
  — no longer reproduces
- `/validate-skill` and `/progressive-disclosure-review` pass wherever the root
  `AGENTS.md` Apply the triggered cleanup step triggers them; plan validate
  exits 0
