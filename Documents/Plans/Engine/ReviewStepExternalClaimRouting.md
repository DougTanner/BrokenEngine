<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T01:21:54.631Z","dependsOn":[]} -->
# Fix: the Review and resolve correctness step routes no consumer for a review's external-claim verification requests

## Context
Four skills the Change Workflow Review and resolve correctness step dispatches
require their worker to emit a `/verify-external-claims` request, and in all four
a pending verdict blocks the result:

- `.agents/skills/repo-code-review/references/worker.md:26-30` — step 7 emits one
  single-claim request for every candidate finding that depends on a non-obvious
  external API, language, specification, OS, or library fact, and step 8
  withholds each such candidate from the confirmed findings until the caller
  returns the verdict. The handoff field is declared at
  `.agents/skills/repo-code-review/SKILL.md:84` (`API verification requests`),
  and `:100` makes a pending external verdict a `NEEDS_ACTION` cause.
- `.agents/skills/glsl-review/references/worker.md:24` — step 12; "A pending
  verdict makes the review `NEEDS_ACTION` and keeps the dependent finding
  unconfirmed". Field declared at `.agents/skills/glsl-review/SKILL.md:48`
  (`External claim verification requests`).
- `.agents/skills/adversarial-review/references/worker.md:25-29` — one
  single-claim request per kept finding that depends on such a claim. Field
  declared at `.agents/skills/adversarial-review/SKILL.md:56`
  (`API verification requests`); `:72` makes pending external verification a
  `NEEDS_ACTION` cause.
- `.agents/skills/resolve-findings/references/worker.md:34-38` — "A pending
  verdict keeps the item unresolved and the handoff `NEEDS_ACTION`". Field
  declared at `.agents/skills/resolve-findings/SKILL.md:70`
  (`External/API verification requests`).

No step routes a consumer for those requests. Root `AGENTS.md:120-132` — the
Review and resolve correctness step, its `Order:` line at `:122`, its bullets at
`:124-130`, and its closing prose at `:132` — names `/verify-external-claims`
nowhere. The only routing of that skill in root `AGENTS.md` is at `:93` and
`:98`, both inside the Plan review step, where the landed bullet covers the
claims a `/plan-audit` handoff raises at Tier 2+ and the claims an
`/external-grill-plan` round raises at Tier 3. The shared reporting reference's
"What main does with each field" table
(`.agents/references/subagent-reporting.md`, `## Handoffs`) covers only the
eight shared fields and has no row for a skill-declared verification-request
field, so it does not supply the missing route either.

Effect: a review handoff whose `NEEDS_ACTION` is caused solely by a pending
external verdict reaches a main session with no step that obtains the verdict.
Main can only decide a finding the reviewer deliberately withheld as
unconfirmed, or leave it open — the same failure mode the Plan review step's
landed bullet fixed for the `/plan-audit` field, now one step later in the
workflow and across four skills instead of one.

`/verify-external-claims` itself needs no change: its description at
`.agents/skills/verify-external-claims/SKILL.md:3-8` already names "a Broken
Engine review, plan audit, grill, or finding-resolution pass" and carries no
tier restriction. The gap is routing only. The Plan review step's landed clause
at `AGENTS.md:98` is the phrasing and dispatch shape to match, so root
`AGENTS.md` states one routing convention rather than two.

Proven out of scope of the change that observed it: this Plan was authored from
a `/plan-audit` residual in a session whose approved change was confined to the
Plan review step's `/verify-external-claims` routing wording in root `AGENTS.md`
and to `/plan-audit` itself, so neither the Review and resolve correctness step
nor any of the four skills above lies inside that boundary.

## Design
The author's recommendation: give the requests a routed consumer at the step
that produces them, mirroring the Plan review step's existing dispatch shape.
Add one bullet to root `AGENTS.md`'s Review and resolve correctness step:

- main runs `/verify-external-claims`, dispatching one `locator` as its evidence
  worker — for the external claims a review or `/resolve-findings` handoff
  raises.

and state its place in that step's `Order:` line at `AGENTS.md:122`: the
dispatch runs when a handoff raises requests, before main decides the dependent
finding and before `/resolve-findings` acts on it, because the reviewer withheld
that finding pending the verdict.

Rationale: the request shape, the handoff fields, and the answering skill all
exist already and already accept a review or finding-resolution request at any
tier; the missing piece is one routing clause. Wording the bullet by what the
handoff raises, rather than by a field name, avoids touching the four skills,
whose labels for the field differ today (`API verification requests`,
`External claim verification requests`, `External/API verification requests`).

The alternative weighed and not recommended: state the route once in the shared
reporting reference's field table
(`.agents/references/subagent-reporting.md`, `## Handoffs`) and drop the
per-step bullets. It reads as the better progressive-disclosure fit, but that
table is keyed by field name and the four labels above are not one name, so it
would force a rename across the four skills' public files; and dropping the Plan
review step bullet at `AGENTS.md:98` would discard the Tier-2+ `/plan-audit`
routing that bullet now carries. It is the larger change and the more coupled
one.

If the implementing session's own reading shows the recommendation is wrong,
surface it for re-planning rather than expanding the boundary below.

## Critical files
- `AGENTS.md` — the Review and resolve correctness step's `Order:` line and
  bullet list (`:120-132`), the only place this routing belongs
- `.agents/skills/verify-external-claims/SKILL.md` — read-only reference for the
  request shape and the `locator` evidence worker; not expected to change
- `.agents/references/subagent-reporting.md` — read-only reference for the
  handoff field table the alternative would have used; not expected to change

## In scope
- One added routing clause in root `AGENTS.md`'s Review and resolve correctness
  step, plus the minimal `Order:`-line wording that places it

## Out of scope
- Renaming or unifying the verification-request field labels in
  `.agents/skills/repo-code-review/SKILL.md`,
  `.agents/skills/glsl-review/SKILL.md`,
  `.agents/skills/adversarial-review/SKILL.md`, and
  `.agents/skills/resolve-findings/SKILL.md`, and each of those skills' worker
  reference
- The Plan review step's `/verify-external-claims` routing (`AGENTS.md:93`,
  `:98`) and `/plan-audit`'s own handling of the requests it raises
- The internals of `/verify-external-claims`: inputs, verdict form, worker
  contract, and its own `references/`
- `.agents/references/subagent-reporting.md` and its field table
- `.agents/skills/session-audit/SKILL.md`, which carries a separate
  `API Verification Requests` section
- Any transcript path or transcript text in the repository

## Risk tier and invariants
Expected Change Workflow Tier 2, on the root `AGENTS.md` trigger "one
subsystem's runtime or tool behavior" — the routing of one review-support skill
at an existing workflow step, adding no step and no role. Escalate if the fix
reaches build/bootstrap coordination or spans independently owned subsystems.
Invariants to preserve: `/verify-external-claims` stays read-only with its single
`locator` evidence worker; the four review skills stay findings-only and spawn
nothing; a Change Workflow step is cited by its heading name, never by its
number; progressive disclosure keeps the routing in root `AGENTS.md` and the
request shape in the skills.

## Acceptance criteria
- A review or `/resolve-findings` handoff that raises an external-claim
  verification request has exactly one routed consumer readable from root
  `AGENTS.md`'s Review and resolve correctness step, and the ordering relative to
  deciding the dependent finding is stated
- `/progressive-disclosure-review` and, if any skill package changed,
  `/validate-skill` pass; `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1`
  reports `status: valid` with `code: ok`

## Notes
Attribution only; no transcript review is expected, because the gap is fully
provable from the tree text cited in `## Context`.
- Client: claude
- Conversation session ID: 40d5ea23-9635-484d-99d0-a21b2c0f9667
- Worktree/branch UUID: 75e972a1-4851-444c-b0a0-9d5076f1cf48
- Session branch: claude/75e972a1-4851-444c-b0a0-9d5076f1cf48
- Worktree: .claude\worktrees\BrokenEngine\75e972a1-4851-444c-b0a0-9d5076f1cf48
- Landing ref: claude/75e972a1-4851-444c-b0a0-9d5076f1cf48
