---
name: next-plan
description: Validates and deterministically claims one Git-backed Documents/Plans Plan through WorktreeCli, resolves it against current code, and presents the resolved Plan and execution card for implementation approval; preparation-proven Tier-1 work continues without that pause. Use only when the latest user request explicitly invokes `/next-plan` or `$next-plan`.
disable-model-invocation: true
argument-hint: "[Documents/Plans/... | partial pattern]"
allowed-tools: [Read, Write, Grep, Glob, Agent, Edit, PowerShell, AskUserQuestion]
---

# Next Plan

Use only for a current explicit `/next-plan` or `$next-plan` invocation. Main
retains user intent and dispatches fixed roles; workers never delegate.
WorktreeCli alone validates metadata, selects, claims, prepares final state,
and releases claims. `Documents/Features` is never scheduler input. The
cross-skill stage order lives in root [AGENTS.md](../../../AGENTS.md) Step 8,
and the landing confirmation belongs to `/finalize-changes`.

## Preconditions and selection

Require a clean wrapper-created session worktree, except for the retained-work
resume documented under Claim lifecycle, and derive the authoritative primary,
owner, and provisioned WorktreeCli from `Get-NextPlanContext`, run from the
session worktree root as one shell call:
`Import-Module ./.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1; Get-NextPlanContext`
The context baseline is provisional until a claim reports a `sync` object;
[references/claim-results.md](references/claim-results.md) owns how each claim,
listing, and claim-exit result is read. Missing tooling requires explicitly
authorized primary maintenance through `/compile`. Never create/adopt a
worktree or inspect machine-local claims directly.

- Bare invocation selects the oldest eligible Plan by immutable `createdUtc`,
  then normalized UTF-8 path.
- A normalized `Documents/Plans/...` argument selects that Plan.
- Any other argument is a case-sensitive partial match against executable Plan
  paths relative to `Documents/Plans/`; exactly one match selects that Plan,
  and zero or multiple matches block.

See the queue before selecting, whether the invocation is bare or names a Plan:
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Get-NextPlanList.ps1`
It is read-only and reports a bounded point-in-time projection — widen with
`-Top <n>` only when a decision needs more. Its snapshot limits and the
tier-constrained reading procedure are in
[references/claim-results.md](references/claim-results.md).

Use the root AGENTS.md canonical invocation form. For bare selection, run this
command with no `-Plan` argument:
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`
For a requested normalized path or partial pattern, append `-Plan` and quote
that value, for example:
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan 'Documents/Plans/example.md'`
(`-Plan 'example.md'` forwards a partial pattern.) Run the bundled script as its
own shell call, never combined with other commands, so its single JSON object
stays parseable and the mutation-capable script is never re-run just to
disambiguate its output. Do not reconstruct the script's transitions.

The script fast-forwards the session branch to the primary tip first, and a
session holding any commit primary lacks instead stops with
`claim.session-diverged`. The script never moves or resets a session branch in
that case: report the named commits to the user, who decides how to resolve it.
The `sync` and `claim.session-diverged` result fields are detailed in
[references/claim-results.md](references/claim-results.md).

On `status: pass`, act on the code: `ok` and `reused` both mean this session
holds the named claim. For a bare selection, `none-available` is a normal
whole-skill stop with nothing to claim, and selection is not re-run to look
again. For a `-Plan`-targeted invocation it is the same whole-skill stop,
reported to the user: the requested Plan is ineligible and the manager never
selects or claims a different candidate in that run. Any other status stops the
skill without repair, reordering, or retry.

## Preparation and execution card

One preparation `implementer` verifies every Plan statement against current code;
the Plan is immutable. Current code wins on contradiction, which is returned as
a card correction or meaningful delta rather than forced into the tree. If the
problem is gone, main asks whether to retain it or explicitly authorize obsolete
final cleanup.

The execution card begins with `### What does this plan do?` and `### Why this
is good for the codebase`, each 2-4 plain sentences, then records goal, out of
scope, tier trigger, interfaces/invariants, acceptance checks with expected
observations, and required/conditional roles. Tier 1 skips plan audit. Tier 2
uses one fresh `/plan-audit` reviewer. At every tier, a plan that adds new code
or modifies non-documentation behavior per Step 2's trigger in root
`AGENTS.md` also gets one fresh `/plan-simplicity-review` reviewer on the same
snapshot, parallel to `/plan-audit` where that runs. Tier 3 follows
`/external-grill-plan`, whose authoritative workflow reference owns its
iterative preparation. Missing a mandatory reviewer blocks.

Invoke the claim script idempotently immediately before the final preparation
handoff; skip that re-invocation on the straight-through path below, where
preparation continues into implementation, so the tree already carries the
implementation edit by that handoff and a claim run refuses a dirty tree — the
selection claim still holds. The preparation worker must never run that
mutation-capable claim script. Every delegation uses the single task brief in
`../../references/subagent-reporting.md` and states that Plan and card
statements are hypotheses: return contradictions to main.

## Implementation approval

Preparation and claim do not require approval. Present the complete resolved
Plan and execution card before implementation: scope, invariants, role
assignments, acceptance criteria, and unresolved decisions.

On Codex, present that complete spec as exactly one `<proposed_plan>` markdown
block — opening and closing tags each on their own line, at most one block per
turn, and only when the spec is complete — then end the turn without asking an
approval question, because the Codex client's own "Implement this plan?" prompt
collects the decision. Any revision is a new complete replacement block.

On Claude Code and every other host, deliver the full presentation per the root
AGENTS.md User Interaction rule: rendered message text, with the approval
question immediately before the mandatory `Follow-up Plans created:` footer, and
no tool call — question tools included — after it. The user's next message is
the decision. A question UI is allowed only in a later turn, after the
presentation is already visible, and only for short follow-up choices.

An affirmative response approves only the latest unchanged presentation. A
meaningful Plan, card, scope, invariant, acceptance, or decision change requires
a new complete presentation.

Per the claimed-executable-Plan paragraph after the Change Workflow steps in
root [AGENTS.md](../../../AGENTS.md), preparation that proves the Plan Tier 1,
decision-complete, and current continues straight into implementation without
this pause; main records those facts. The landing confirmation in
`/finalize-changes` always applies.

## Claim lifecycle

Before landing-commit creation, an `implementer` runs
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Complete-NextPlan.ps1`
with no arguments for completion or appends `-Reject` only after explicit
user-authorized rejection. Success removes only direct-child dependency edges,
deletes the selected Plan in the worktree, reports the changed paths the landing
commit must contain and returns `nextAction: finalize-changes`; the result-field
shapes are in [references/claim-results.md](references/claim-results.md). The
claim stays held until landing succeeds.

Deferral uses
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Defer-NextPlan.ps1`
and only an ordinary live claim. After final preparation has run, deferral
requires an explicit user instruction given in the current session, recorded in
the handoff; nothing else unlocks it. Deferral never touches the worktree, so
uncommitted implementation work stays exactly as it is.

Resuming retained work needs an explicit user resume instruction given in the
current session, recorded in the handoff, and then the targeted claim with
`-ResumeRetained` appended:
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan 'Documents/Plans/example.md' -ResumeRetained`
That switch is valid only with `-Plan` and changes no file; its retained-path
guarantees and the `claim.worktree-dirty` cases that still block are in
[references/claim-results.md](references/claim-results.md).

`/finalize-changes` deletes the claim after primary advances.

## Run checkpoint

At the end of the run, whichever way it ends, one external review covers the run
for tooling friction and, on Claude, for context efficiency. It runs exactly
once: after the change's own acceptance checks, before the claim-exit script
above, and before `/finalize-changes` prepares the landing commit — so a Plan it
produces is an ordinary new worktree file that rides the same squash and needs no
landing-commit join. Main never performs either review itself.

The review therefore covers friction observable in the transcript up to its own
dispatch. Running the claim-exit script and `/finalize-changes` comes after it,
so friction in either follows the post-checkpoint rule at the end of this
section, and `/next-plan-review` covers both after landing.

The reviewer's run evidence is main's own conversation transcript. Main resolves
its absolute path in its own session shell, with
[references/follow-up-provenance.md](references/follow-up-provenance.md) owning
which session ID applies:
`(Get-ChildItem "$env:USERPROFILE/.claude/projects/*/$env:CLAUDE_CODE_SESSION_ID.jsonl").FullName`
Then main runs, from the session worktree root:
`pwsh -NoProfile -File .agents/skills/next-plan-checkpoint-review/scripts/Measure-SessionContext.ps1 -SessionId <id>`
and dispatches one fresh `reviewer` for `/next-plan-checkpoint-review` through
`/codex-review`, putting in the scope file the transcript path, the envelope
text, and the claimed Plan path or `no claim`. That path is scope-file content
only; no transcript path or transcript text ever enters a Plan body.

A Codex session runs neither half and records `none (codex)` on both handoff
lines: the measurement reads Claude transcripts only, and this repository
documents no way for a Codex main to name its own live transcript. Codex
coverage of both concerns stays with `/next-plan-review` after landing.

If the transcript path cannot be resolved, there is no run evidence to review:
main dispatches no reviewer, records `blocked (transcript-unavailable)` on both
handoff lines, and routes that failure itself through the post-checkpoint rule
below, so an `implementer` records it via `/create-follow-up-plans`.

The measurement's own state never blocks the friction lens. On a `pass` envelope
main supplies `pass` and records `none` on the `Context-efficiency follow-ups:`
line, because a passing measurement leaves nothing to follow up. A blocked or
error measurement exit supplies and records `blocked (<code>)`, and a
`breachRowsTruncated: true` envelope supplies and records
`blocked (breach-rows-truncated)`; that line's `blocked (<code>)` form covers
those codes and `transcript-unavailable`. The reviewer skips the context lens in
every one of these cases. All of these blocked and error states are themselves
tooling friction the same reviewer sees in the transcript it is already reading,
so none of them needs recovery machinery of its own.

For each accepted finding that Step 7's own fix-it-here rule in root
[AGENTS.md](../../../AGENTS.md) does not fix inside this session, an
`implementer` routes it through `/create-follow-up-plans` as a tooling-friction
proposal, supplying the observed symptom with its citation plus the provenance
block sourced per
[references/follow-up-provenance.md](references/follow-up-provenance.md). An
`active-change-blocker` finding never becomes a follow-up Plan: it returns to the
current change as a blocker. Step 7 in root [AGENTS.md](../../../AGENTS.md) names
this Plan the instance of a leftover routed after the Step 5 review dispatch and
owns how it is authored and verified.

Friction first observed after this checkpoint — including friction in running the
claim-exit script and in `/finalize-changes` itself — is recorded through
`/create-follow-up-plans` by an `implementer` and lands at a later gate as its own
content. On deferral, or when the run ends without a claim, the checkpoint's
Plans are themselves the landed content and the landing gate applies to them.

## Preparation handoff

```text
Claim: <Plan path or none; resolved state when claimed>
Classification: Tier 1 | Tier 2 | Tier 3 and trigger
Approval pause: skipped (proven Tier-1) | required
Residuals: <blocker or none>
Friction follow-ups: <Plan path(s) or none | none (codex) | blocked (transcript-unavailable)>
Context-efficiency follow-ups: <Plan path(s) or none | none (codex) | blocked (<code>)>

Execution card:
### What does this plan do?
<2-4 plain sentences>
### Why this is good for the codebase
<2-4 plain sentences>
- Goal: <result>
- Out of scope: <boundary>
- Tier trigger: <trigger or none>
- Interfaces and invariants: <contracts>
- Acceptance checks: <check and expected observation>
- Roles: <required and conditional assignments>
```
