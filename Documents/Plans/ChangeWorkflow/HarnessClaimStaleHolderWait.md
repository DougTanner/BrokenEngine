<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-11T23:43:54.854Z","dependsOn":[]} -->
# Fix: Invoke-HarnessClaim.ps1 — waits the full budget on a provably dead holder

## Context

Observed symptom. The harness worker ran the documented claim invocation,
`pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1`
with the default wait budget, against a `default` harness lock whose holder was
already dead: the holder record's claimant and heartbeat PIDs no longer existed
and its `heartbeatAt` was more than twelve minutes stale — far past the
five-minute staleness threshold in
`.agents/skills/agent-harness/references/worker.md:162-165`. The script polled
for its whole 500-second budget and then returned exit `2`,
`claim.foreign-owner`. The worker then had to perform the documented takeover
(`worker.md:167-207`: resolve listeners, validate them, `quit`, mint a token,
`lock steal --expect`) by hand anyway, so the wait produced no decision and
consumed most of a fifteen-minute run budget, delaying the run it was setting up.

Relevant current behavior, for the fix session's starting point:

- `Invoke-HarnessClaim.ps1:18-20` defaults `-WaitSeconds` to 500 and `:28` polls
  every 5000 ms; the loop at `:325-344` repeats only the claim attempt.
- The script's header (`:3-5`) states by design that a foreign owner is waited
  out, never stolen from, and that nothing touches the holder's processes,
  heartbeat, or claim.
- The holder record each refused `lock claim` prints is already in hand on the
  first attempt, and the expired-wait payload reads `heartbeatAt` from exactly
  that record (`:376`) before completing with `claim.foreign-owner`
  (`:380-381`). So the staleness the worker needed was available before the wait
  began, not only after it.
- `worker.md:121-127` tells the reader a `claim.foreign-owner` block means the
  wait expired and to reorder work or escalate rather than re-run the wait
  blind; it does not connect that block to the staleness rule at `:162-165`.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 534f7c50-0e53-4a42-81b3-1c1f464bf833
- Worktree/branch UUID: 8c269414-3237-4599-8707-74b31f174ae2
- Session branch: claude/8c269414-3237-4599-8707-74b31f174ae2
- Worktree: .claude\worktrees\BrokenEngine\8c269414-3237-4599-8707-74b31f174ae2
- Landing ref: claude/8c269414-3237-4599-8707-74b31f174ae2
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design

First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref
above — supplying the recorded client and the recorded conversation session ID.
Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author's recommended direction, to be confirmed by that root-causing: have
the first refused claim attempt classify the holder record's `heartbeatAt`
against the same five-minute threshold `worker.md:162-165` uses, and, when the
holder is already stale, complete immediately with the existing
`claim.foreign-owner` payload plus a field saying the holder is stale, instead
of spending the remaining budget. The script still never steals and never
touches the holder's processes, heartbeat, or claim, so its stated contract is
preserved and the documented takeover stays the worker's decision; only the
worker.md text describing when to expect the block would follow. A fix that
makes the script itself steal is out of scope.

## Critical files

- `.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1` — the wait loop
  and the `claim.foreign-owner` completion (`:3-5, :18-28, :325-344, :376-381`).
- `.agents/skills/agent-harness/references/worker.md` — the claim result step
  (`:121-127`) and the ownership and takeover steps (`:160-207`).

## In scope

- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the two files above: the staleness
  classification and early completion inside `Invoke-HarnessClaim.ps1`'s claim
  wait loop and its `claim.foreign-owner` result, and the matching wording in
  the `worker.md` claim-result and takeover steps

## Out of scope

- The landed change the session produced
- Making the script steal, quit a holder's processes, or touch a holder's
  heartbeat or claim; the AgentHarness `lock` command implementation; the
  provisioning, executable-existence, pack-version, and token steps of the
  script
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. The script is shared by every harness session, so
the existing exit codes and result schema must keep their meanings, and no
change may let two sessions hold the harness at once. Never embed transcript
paths or home paths.

## Acceptance criteria

- The recorded symptom no longer reproduces under the documented invocation: a
  holder whose heartbeat is older than the documented threshold produces the
  block promptly instead of after the full wait budget, and a fresh holder is
  still waited out for the full budget and never disturbed.
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing
