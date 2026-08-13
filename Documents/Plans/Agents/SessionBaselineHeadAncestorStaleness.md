<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-13T21:47:35.402Z","dependsOn":[]} -->
# Fix: session baseline stays stale after a rebase onto a normally advanced primary

## Context

`Get-AgentWorktreeSessionContext` in `.agents/scripts/AgentWorktreeSession.psm1`
resolves the session baseline at `:104-124`. A configured
`BROKEN_ENGINE_BASELINE` is discarded only when it is no longer an ancestor of
the live primary tip (`:116-117`) — the condition a primary history rewrite
produces. That guard landed in `cba1351fc9f2b844e883e345b8113da5c1f54b32`.

A second staleness case is not covered. When a session rebases itself onto a
primary that advanced normally — no rewrite — the recorded baseline is still
reachable from the primary tip, so it is accepted, but it no longer sits below
the session's own commits. Every diff and review scoped from that baseline then
includes other sessions' already-landed work, which silently widens session
change inventories, scope reviews, and compile change sets.

This case was raised during `/external-grill-plan` on the landed change and
deliberately declined as out of scope there, because it is not caused by history
rewriting and closing it would widen a code path that every session executes. It
is recorded under `## Out of scope` in that change's approved plan, so it is a
proven out-of-scope residual rather than an in-scope acceptance failure.

The remedy is decided: the baseline block additionally requires the configured
`BROKEN_ENGINE_BASELINE` to be an ancestor of the session's own `HEAD`,
alongside the existing condition that it be an ancestor of the live primary tip.
A configured baseline failing either condition is discarded and the existing
fallback chain (`--fork-point`, then plain `merge-base`) resolves the baseline,
so the function still answers with a usable commit rather than failing closed.

## Design

Confirm the failure by rebasing a session branch onto a normally advanced
primary and observing that `Get-AgentWorktreeSessionContext` still returns the
pre-rebase configured baseline while that commit is no longer an ancestor of the
session `HEAD`. Then add the session-`HEAD` ancestor requirement to the
configured-baseline acceptance condition at `:116-117`, and update the comment
above it to state both staleness cases the guard now covers.

## Critical files

- `.agents/scripts/AgentWorktreeSession.psm1` — the baseline resolution block in
  `Get-AgentWorktreeSessionContext` (`:104-124`), including the ancestor guard
  at `:116-117` and the `--fork-point` and `merge-base` fallbacks at `:120-124`.

## In scope

- The configured-baseline acceptance condition inside
  `Get-AgentWorktreeSessionContext`'s baseline block (`.agents/scripts/AgentWorktreeSession.psm1:116-117`),
  which additionally requires the configured baseline to be an ancestor of the
  session's own `HEAD`, plus the comment above it that states which staleness
  cases the guard covers.

## Out of scope

- `.agents/skills/compile/scripts/Resolve-CompileContext.ps1` (`:183-191`) is
  deliberately not changed. It reads `BROKEN_ENGINE_BASELINE` straight from the
  process environment and never calls `Get-AgentWorktreeSessionContext`. That
  divergence is already accepted and documented: both values scope the same
  session diff, so unifying them is not required and would widen the change.
  This is a decided exclusion, not an open question — do not reopen it.
- The landed rewritten-history recovery behavior in
  `cba1351fc9f2b844e883e345b8113da5c1f54b32` beyond the acceptance condition
  named above.
- The `--fork-point` and plain `merge-base` fallbacks themselves, the primary
  identity, branch, sidecar, and worktree-record resolution earlier in the same
  function, and the returned object's shape.
- WorktreeCli scheduler, claim, and landing behavior; every consumer script that
  merely receives the resolved baseline.
- Changing how `BROKEN_ENGINE_BASELINE` is set by the wrapper scripts.

## Risk tier and invariants

Change Workflow Tier 2 (scoped tool behavior). The narrowed scope is one
acceptance condition and its comment inside a single function, and it reaches no
build or bootstrap coordination surface. Baseline resolution must never fail
closed: an unusable configured baseline falls through to the existing fallbacks,
and the function still returns a resolvable commit for a session that has not
rebased. No determinism/CRC, serialization, replay, wire, threading,
allocation, or shader surface is exposed.

## Coordination

`Documents/Plans/Agents/NextPlanListGitIdentityMismatch.md` names
`Get-AgentWorktreeSessionContext` (`:66-117`) in its boundary, but only for the
session and primary identity resolution behind its `git-identity-mismatch`
symptom, and only if its review proves the caller contract requires it. The two
regions are disjoint: that Plan owns identity resolution, this Plan owns the
configured-baseline acceptance condition. Whichever lands second rebases onto
the first and re-reads the function before editing.

## Acceptance criteria

- After a session rebases onto a normally advanced primary, the resolved
  baseline is an ancestor of the session `HEAD` and the session diff no longer
  contains other sessions' already-landed commits.
- A session that has not rebased still resolves to its recorded baseline, and a
  session whose primary history was rewritten still recovers the true fork point
  as the landed behavior does.
- Baseline resolution still always returns a commit; no path fails closed.
- `Resolve-CompileContext.ps1` is unchanged.
- WorktreeCli `plan validate` exits `0` with `status: valid` and `code: ok`.

## Notes

This Plan is keyed to the pair (`Get-AgentWorktreeSessionContext` baseline
block, a configured baseline surviving a rebase onto a normally advanced
primary). No existing Plan under `Documents/Plans/Agents/` covers baseline
staleness; `SessionChangeInventoryUnstagedBlob.md` concerns blob identity
hashing inside a different script and is not a duplicate.
