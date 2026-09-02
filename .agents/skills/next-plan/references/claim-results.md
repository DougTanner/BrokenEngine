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

Before it validates and claims, the claim script brings the session branch up to
the primary tip by fast-forward only when the session is behind, reporting a
`sync` object that names the old and new commits; a session holding any commit
the primary tip lacks cannot be fast-forwarded, so it stops with
`claim.session-diverged` and leaves the branch untouched. Because selection
therefore reads a tree at the primary tip as of that invocation,
`none-available` means the Plan is genuinely ineligible rather than merely
absent from a stale worktree; a Plan that lands on primary afterwards is picked
up by the next invocation.

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

A plain named run skips the listing, so the claim result names every stop: a
partial pattern matching nothing blocks with `plan-name-not-found`, several
matches block with `plan-name-ambiguous` and list `candidates`, and an exact
path that is absent, blocked, excluded, or claimed by another session reports
`none-available`; [worker.md](worker.md) rules own how each stop is handled.

## Diverged sessions

A `claim.session-diverged` result carries no `divergence` object: its `message`
lists every commit the session holds and primary lacks. Under `-ResumeRetained`
the blocked result may also carry the `retained` projection, because the script
builds it before the divergence check
(`Invoke-NextPlanClaim.ps1:44,53-57`). Report both the unlanded commits and any
retained work to the user, who decides how to resolve it.

## Claim-exit result fields

`Complete-NextPlan.ps1` reports the changed paths the landing commit must
contain as `changes.items[].path`, counted by `changes.totalCount`, with
`changes.truncated` flagging more paths than the listed items, and returns
`nextAction: finalize-changes`.

`Defer-NextPlan.ps1` reports the uncommitted work it leaves in place as
`retained`: a bounded `paths` list with the full `count`.

## Retained work and `-ResumeRetained`

Any dirty session worktree other than deferral-retained work blocks a claim with
`claim.worktree-dirty`, naming the dirty paths and leaving the tree and the
scheduler untouched.

The `-ResumeRetained` switch never stages, stashes, or reverts anything, leaves
every retained file byte-identical, and reports those paths as `retained` on the
pass result. It still blocks with `claim.worktree-dirty` when a dirty path is
under `Documents/Plans/`, because selection reads that scheduler input from this
tree, or when the fast-forward to the primary tip would touch a retained path.
