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
`claim.session-diverged` and a `divergence` object. When that object reports
`verdict: safe-reset`, `divergence.recovery` then names the exact
`git reset --hard <primary tip>` command to run once from the session worktree
root before rerunning the claim script, and that reset is the only case in which
a session branch may be moved by hand. `verdict: unlanded-work` names the
commits carrying work primary does not have: never reset, and report them to the
user. A null `divergence` means the classification could not run, which is
treated exactly like `unlanded-work`. The `sync` and `divergence` field detail is
in [references/claim-results.md](references/claim-results.md).

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
question as the last line of that same message and no tool call — question tools
included — after it. The user's next message is the decision. A question UI is
allowed only in a later turn, after the presentation is already visible, and
only for short follow-up choices.

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

`/finalize-changes` deletes the claim after primary advances. Run
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Test-NextPlanWorkflowScripts.ps1 -Executable '<worktree-cli-path>'`
only when `Complete-NextPlan.ps1`, `Defer-NextPlan.ps1`, `Get-NextPlanList.ps1`,
`Invoke-NextPlanClaim.ps1`, `NextPlanWorkflowCommon.psm1`, or
`Test-NextPlanWorkflowScripts.ps1` itself changes; substitute the provisioned
`WorktreeCli` path that `Get-NextPlanContext` returns for `<worktree-cli-path>`
and never pass the placeholder literally.

## Tooling friction follow-ups

At the end of the run, whichever way it ends, main reviews the whole run for
tooling friction: a bundled script errored, returned a malformed or
contradictory result, or could not be run as documented; a workaround or
deviation was needed; or work was repeated because a skill's instructions were
unclear, wrong, or contradicted repository state. Ordinary review findings about
the change, user-driven iteration, and documented normal stops such as
`none-available` are not friction, and neither is a failure in a skill or script
the claimed Plan itself changes — that is a blocker of the active change. The
review covers friction observed at any point, including a stop before or
without a claim, and the claim-exit scripts
above are themselves in scope: review once before running them, and again inside
`/finalize-changes` once the landing commit is prepared and before the landing
`/verify-changes` acceptance review is dispatched.

Root [AGENTS.md](../../../AGENTS.md) Step 7 owns this routing timing split. In a
`/next-plan` run, friction first observed before the Step 5 review dispatch is
routed at that point, ahead of the dispatch; friction first observed at or after
that dispatch is routed at the checkpoints above.

For each distinct issue that Step 7's rule does not fix inside this session, an
`implementer` routes it through `/create-follow-up-plans` as a tooling-friction
proposal, supplying the observed symptom with its citation plus the provenance
block, sourced per
[references/follow-up-provenance.md](references/follow-up-provenance.md). On
Claude, main reads the conversation session ID from `CLAUDE_CODE_SESSION_ID` in
its own session shell — a subagent shell reports that subagent's own ID — and
passes it to the `implementer`.

On completion or rejection, a friction Plan authored at that second checkpoint
joins the landing commit alongside the changed paths the claim-exit script
reported in `changes.items[].path`: one further candidate commit for that Plan
path and one further approval-preparation run carrying `-CommitMessageFile` so
the rebuilt commit's message describes the enlarged content, both invoked
exactly as `/finalize-changes` documents them and with no hand-run Git. Root
[AGENTS.md](../../../AGENTS.md) Step 8 owns what that join costs in re-review
and `/verify-changes` runs. Friction first observed after that second
checkpoint's friction review — including friction the join mechanics themselves
produce — is recorded through `/create-follow-up-plans` by an `implementer` and
lands at a later gate as its own content, exactly as the deferral case does. On
deferral, or when the run ends without a claim, the friction Plan is itself the
landed content and the landing gate applies to it.

## Context-efficiency review

Main also measures how much tool output and subagent handoff text entered its
own context, at the same two checkpoints as the friction review above, in Claude
sessions only. A Codex session skips this review and records `none (codex)`;
Codex coverage of the same concern stays with `/next-plan-review`'s post-landing
token-efficiency lens.

Main reads `CLAUDE_CODE_SESSION_ID` in its own session shell, subject to the
subagent-ID warning under Tooling friction follow-ups, and runs
`pwsh -NoProfile -File .agents/skills/context-efficiency-review/scripts/Measure-SessionContext.ps1 -SessionId <id>`
from the session worktree root. Run it at both checkpoints even when the first
one passed, because the second also measures the finalization phase's output.

The envelope's verdict decides what follows. On `pass`, main records `none` and
dispatches no reviewer. On `needs-review`, main dispatches one fresh `reviewer`
for `/context-efficiency-review` through `/codex-review`, with the envelope text,
the checkpoint name, and the claimed Plan path or `no claim` in the scope file. A blocked or error exit
records `blocked (<code>)` on the handoff line instead, and a
`breachRowsTruncated: true` envelope records `blocked (breach-rows-truncated)`;
both dispatch no reviewer and need no recovery machinery of their own, because
each is itself tooling friction handled by the Tooling friction follow-ups
review.

For each accepted fixable-defect finding that Step 7's rule does not fix inside
this session, an `implementer` routes it through `/create-follow-up-plans` as a
tooling-friction proposal carrying the same provenance block
([references/follow-up-provenance.md](references/follow-up-provenance.md)), and
a Plan authored at the second checkpoint joins the landing commit exactly as the
friction section above documents. An
`active-change-blocker` finding never becomes a follow-up Plan: it returns to the
current change as a blocker.

## Preparation handoff

```text
Claim: <Plan path or none; resolved state when claimed>
Classification: Tier 1 | Tier 2 | Tier 3 and trigger
Approval pause: skipped (proven Tier-1) | required
Residuals: <blocker or none>
Friction follow-ups: <Plan path(s) or none>
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
