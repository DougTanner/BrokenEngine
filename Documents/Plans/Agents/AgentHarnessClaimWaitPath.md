<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T12:28:37.841Z","dependsOn":[]} -->
# Fix: agent-harness / Invoke-HarnessClaim.ps1 — a `claim.foreign-owner` block has no documented wait-until-free path

## Context

Harness verification run from the session worktree root as

```
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1
```

returned `status: blocked` with `code: claim.foreign-owner` against a concurrent
session's live, non-stale claim on the harness lock.

`.agents/skills/agent-harness/SKILL.md:36` states the outcome and then hands the
decision back to the caller: the script "never steals, never waits for the lock",
and the payload's `currentOwner` carries `claimedAt` and `holdSeconds` so the
caller can "decide between waiting and reordering non-harness work instead of
polling blind". Waiting is therefore an endorsed outcome, but nothing in the
package says how to wait. `.agents/skills/agent-harness/scripts/` contains
`Invoke-HarnessClaim.ps1`, `Invoke-HarnessRelease.ps1`, `Wait-HarnessPing.ps1`,
`Wait-IslandSceneReady.ps1`, `Test-PrivateLanFirewallReadiness.ps1`,
`Test-Wait-IslandSceneReady.ps1`, and the `rdc_*` analysis scripts — none waits
for claim availability. `Wait-HarnessPing.ps1` (SKILL.md:77-83) waits for a
launched port to answer, which is a different condition and is only reachable
after a claim already succeeded.

The forced rework: with no documented wait and an explicit instruction not to
poll blind, the worker hand-wrote a throwaway polling driver under `Temp/`
(`Wait-HarnessLockFree.ps1`), invented its own poll interval and deadline, and
polled for roughly 84 seconds until the concurrent owner released. That
reconstruction — including the choice of interval, timeout, and what counts as
"free" — is repeated from scratch by every future session that meets a live
foreign owner, and the SKILL's own rule at `:83` ("Never hand-write a ping loop
or invent a sleep window in its place") shows hand-written waiting is exactly
what this package intends to prevent elsewhere.

The claimed Plan for this run was
`Documents/Plans/Agents/AgentHarnessModifiedReplayStaging.md`, whose `## In scope`
covers only the `#### Replay manifest v3 integrity matrix` section of
`Projects/BrokenEngineSandbox/Documents/AgentHarness.md` plus, conditionally, one
new agent-harness staging script — a condition that was not taken. The claim
path named below is outside that boundary, so this is `/next-plan` tooling
friction rather than an in-scope acceptance failure.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Session: 7b66a9dc-883e-4dd2-a8a1-10814ed90ee4
- Subagent task: a1f49c4126461f1a0
- Session branch: claude/68da05af-c6e0-4e32-979d-87ba545ba868
- Worktree: .claude\worktrees\BrokenEngine\68da05af-c6e0-4e32-979d-87ba545ba868
- Landing commit: `git log --diff-filter=A --format=%H -- <this plan path>`
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact session id above.

## Design

In a new session, run `/next-plan-review <landing commit>` supplying the
recorded client and session id, root-cause the friction from the proven
transcript, then make the smallest fix inside the `## In scope` boundary below.
If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The outcome to deliver: a session that receives `claim.foreign-owner` and decides
to wait can do so from the package alone, with a bounded deadline and a typed
outcome, without hand-writing a poll loop or inventing an interval. Two candidate
shapes are visible from the symptom and root-causing decides between them — they
are alternatives, not a set to implement together: a bundled waiter script under
`.agents/skills/agent-harness/scripts/` that polls the claim to a caller-supplied
deadline and returns a typed pass/blocked result the way `Wait-HarnessPing.ps1`
does, or an opt-in bounded wait parameter on `Invoke-HarnessClaim.ps1` itself
that keeps its existing `broken-engine-harness-claim/v1` result shape. Either
shape keeps the existing no-steal rule and the five-minute staleness rule in
`## Ownership and takeover` untouched.

## Critical files

- `.agents/skills/agent-harness/SKILL.md` — the `## Provision and claim` section
  (`:30-36`), which owns the claim invocation and the `claim.foreign-owner`
  outcome and must name whatever wait path is adopted
- `.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1` — the claim
  script producing the blocked result; relevant only if root-causing selects the
  wait-parameter shape
- `.agents/skills/agent-harness/scripts/Wait-HarnessPing.ps1` — read-only
  reference for the bundled-waiter result contract and exit-code convention a new
  waiter would mirror

## In scope

- Root-cause investigation via `/next-plan-review` with the recorded provenance
- The smallest resulting fix, confined to the `## Provision and claim` section of
  `.agents/skills/agent-harness/SKILL.md` plus exactly one of: one new waiter
  script under `.agents/skills/agent-harness/scripts/`, or a bounded wait
  parameter added to `.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1`

## Out of scope

- The landed change this session produced, and its claimed Plan
- The no-steal rule, the five-minute staleness definition, and the
  `## Ownership and takeover` takeover procedure, which stay exactly as they are
- The harness lock implementation in WorktreeCli or AgentHarness (`lock token`,
  `lock claim`, `lock steal`, `lock status`, `lock release`) and any change to
  lock storage, lease duration, or heartbeat semantics
- `Invoke-HarnessRelease.ps1`, launch, verification, and any game command or
  response schema
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior in the harness skill package); Tier 1 if
the accepted fix is documentation only. Escalate if the fix reaches the lock
implementation or shared build/bootstrap coordination — that is outside the
boundary above and is surfaced for re-planning instead. Waiting must never
become stealing: a wait that expires reports blocked and leaves the foreign
owner untouched, and no wait path may disturb a fresh owner's processes or
lock state. Never embed transcript paths or home paths.

## Acceptance criteria

- The recorded symptom no longer reproduces: a session blocked by
  `claim.foreign-owner` waits for the lock using only the documented package
  invocation, with no hand-written poll loop and no invented sleep window
- The wait is bounded by a caller-supplied deadline and reports a typed result
  distinguishing "claim acquired" from "deadline expired, foreign owner still
  holds"
- An expired wait leaves the foreign claim and its processes untouched, and a
  fresh foreign owner is never stolen from
- `/validate-skill` passes for the changed `SKILL.md`; WorktreeCli
  `plan validate` exits `0` with `status: valid` and `code: ok`

## Notes

This Plan is keyed to the pair (`.agents/skills/agent-harness/` claim path, no
documented or bundled wait-until-free path for `claim.foreign-owner`). A later
observation of the same pair is a duplicate, not a new residual.
`Documents/Plans/Agents/AgentHarnessModifiedReplayStaging.md` and
`Documents/Plans/Agents/AgentHarnessOwnerTokenReclaimLoop.md` also target the
agent-harness package but cover different symptoms — replay-artifact staging and
owner-token bookkeeping across a stop/stage/re-claim loop — and are not
duplicates.
