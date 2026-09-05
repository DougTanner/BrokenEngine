# Claim, Listing, and Claim-Exit Results

Read this reference when a `/next-plan` script result needs interpreting: a
claim, a queue listing, a completion, a deferral, or a retained-work resume.
It describes the result fields the agent acts on, never script internals, and
the workflow that runs those scripts stays in [`/next-plan`](../SKILL.md).

## Session baseline and the sync object

`Get-NextPlanContext`, exported by the bundled `NextPlanWorkflowCommon.psm1`
module, resolves the authoritative primary, owner, and provisioned WorktreeCli
path. Its baseline is provisional until a claim runs: every claim result
carrying the `sync` object records that result's `sync.to` as the session
baseline, and a result without one leaves the recorded baseline unchanged, so
the `Get-NextPlanContext` baseline holds only until the first claim reports a
`sync` object.

A session that already holds a Plan claim is normally reported as `reused` with
`nextAction: prepare` before the tree is examined. That early report holds in
any tree state and whether or not `-Plan` was passed, and a `-Plan` value is
not resolved at all for a held claim, so no path or pattern result can surface;
it carries neither a `sync` nor a `retained` object and the recorded baseline
is unchanged. When the held-claim lookup is instead busy or unreadable, the run
falls through into the ordinary flow below, so that `reused` result can carry a
`sync` object and the baseline rule above then applies to it.

Any other claim run, bare or targeted, is refused with `stop-report-to-user`,
never `resume-with-flag`, when the session worktree holds an uncommitted
`Documents/Plans` path; that result's `message` names those paths and the route
that works.

Otherwise, before it validates and claims, the claim script brings the session
branch up to the primary tip by fast-forward only when the session is behind,
reporting a `sync` object that names the old and new commits; a session holding
any commit the primary tip lacks cannot be fast-forwarded, so it stops with
`claim.session-diverged` and leaves the branch untouched. Because selection
therefore reads a tree at the primary tip as of that invocation,
`none-available` means the Plan is genuinely ineligible rather than merely
absent from a stale worktree; a Plan that lands on primary afterwards is picked
up by the next invocation. A `claim.session-diverged` result carries no
`divergence` object: its `message` lists every commit the session holds and
primary lacks, and under `-ResumeRetained` it may also carry the `retained`
projection.

## Reading the queue listing

`Get-NextPlanList.ps1` runs only when [worker.md](worker.md) step 2 calls for
it. It reports a bounded projection: per-state counts, then the first Plans in
selection order (default 5) as `path`/`state` rows, with `blockedBy` on a
blocked row and `diagnostic` on an excluded one. It never emits the whole tree,
so a large Plan queue cannot flood the session. It deliberately reads the
session worktree's own tree and never moves it, so a Plan landed on primary
after this session started shows up only after the claim script fast-forwards
the session. Its eligible and claimed states are likewise an unguarded
point-in-time snapshot of the machine-local claim store, so a bare claim run
moments later can skip Plans the listing showed as eligible because concurrent
sessions claimed them in between. That is normal scheduler behavior rather than
a selection defect, and the claim result does not name the passed-over Plans.

For a tier-constrained request, read the `Risk tier` prose of the top eligible
candidates in that order until one matches, then claim that path.

An exact `Documents/Plans/...` path that is absent, blocked, excluded, or
claimed by another session yields `none-available`. Its message names that
current cause: blocked results list at most 10 sorted prerequisite paths and an
omitted count, excluded results carry the matching row or Plan diagnostic,
claimed results expose no claim identity, and absent results say the path is
absent from the session tree. Because the listing is an unguarded later
snapshot, an eligible or otherwise unclassifiable result instead directs a
retry. A bare `none-available` result skips this diagnosis. A partial pattern
matching several validated executable Plans yields `plan-name-ambiguous`, whose
`message` names no match; the matching Plan paths are in the result's
`candidates` array.

## Claim-exit result fields

`Complete-NextPlan.ps1` reports the changed paths the landing commit must
contain as `changes.items[].path`, counted by `changes.totalCount`, with
`changes.truncated` flagging more paths than the listed items, and returns
`nextAction: finalize-changes` only on the `ok` result; every other completion
result returns `stop-report-to-user`.

`Defer-NextPlan.ps1` reports the uncommitted work it leaves in place as
`retained`: a bounded `paths` list with the full `count`. The claim script's
`-ResumeRetained` switch never stages, stashes, or reverts anything, leaves
every retained file byte-identical, and reports those paths as `retained` on the
pass result.

## `nextAction`

Every claim, deferral, and completion result carries a `nextAction` naming the
one thing to do next, drawn from these five values:

- `prepare` — this session holds the claim; continue the preparation workflow.
- `stop-report-to-user` — the run stops here; report the result and let the user
  decide what happens next, after the [worker.md](worker.md) step 8 checkpoint.
- `resume-with-flag` — a `-Plan` run found an unclean worktree whose dirty paths
  are all outside `Documents/Plans`; the rerun with `-ResumeRetained` is gated
  by the [worker.md](worker.md) resume rule.
- `retry-later` — tell the user, and the same command can be run again later.
- `finalize-changes` — the Plan terminal state is prepared; land the change
  through `/finalize-changes`.
