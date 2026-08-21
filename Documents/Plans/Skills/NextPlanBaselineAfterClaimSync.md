<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-20T22:07:06.764Z","dependsOn":[]} -->
# Fix: next-plan calls the pre-claim baseline authoritative although the claim can move it

## Context

`.agents/skills/next-plan/SKILL.md:21-24`, in `## Preconditions and selection`,
says to "derive the authoritative primary, baseline, owner, and provisioned
WorktreeCli from `Get-NextPlanContext`". That call runs before the Plan is
claimed.

`## Claim lifecycle` then documents, at `SKILL.md:56-59`, that the claim script
`.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1` "brings the session
branch up to the primary tip by fast-forward only when the session is behind,
reporting a `sync` object that names the old and new commits". The script sets
that object at `Invoke-NextPlanClaim.ps1:56` as
`sync = @{fastForwarded=$true; from=$context.SessionHead; to=$context.PrimaryTip}`.

So whenever the session is behind primary, the commit `Get-NextPlanContext`
reported as the baseline stops being the session's baseline the moment the claim
fast-forwards, seconds later — the correct post-claim baseline is the claim
result's `sync.to`. Observed in a real run: the context reported one baseline
commit, and the claim's `sync` object reported that same commit advancing to a
different one nine seconds later. That run used the post-claim commit in every
downstream subagent brief, so nothing went wrong, but the word "authoritative"
attached to the pre-claim value invites a manager to copy the stale commit into
briefs, where `.agents/references/subagent-reporting.md:26-27` makes the
baseline the commit every delegated diff is measured against.

## Design

Reword `## Preconditions and selection` in `.agents/skills/next-plan/SKILL.md`
so the pre-claim `Get-NextPlanContext` values stay authoritative for primary,
owner, and provisioned WorktreeCli, while the baseline is stated as
provisional until the claim runs: the session baseline is the claim result's
`sync.to` when the claim fast-forwarded, and the `Get-NextPlanContext` baseline
otherwise. Reference the already-documented `sync` object in `## Claim
lifecycle` rather than restating what fast-forward does.

Wording only, in that one section (plus the minimal pointer in `## Claim
lifecycle` if the rule cannot be stated in the preconditions section alone). No
script changes: `Get-NextPlanContext` and `Invoke-NextPlanClaim.ps1` already
return every value the corrected rule needs.

## Critical files

- `.agents/skills/next-plan/SKILL.md` — `## Preconditions and selection` (the
  authoritative-values sentence), and `## Claim lifecycle` only if the corrected
  rule cannot live in the first section alone.
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1` and
  `.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1` — read-only
  evidence for the returned shapes; not edited.

## In scope

- The sentence in `## Preconditions and selection` that calls the
  `Get-NextPlanContext` baseline authoritative, corrected to take the baseline
  from the claim result's `sync.to` after a fast-forward.
- A minimal reciprocal pointer in `## Claim lifecycle`, only if needed.

## Out of scope

- Any script or module under `.agents/skills/next-plan/scripts/` — no result
  shape, field, or exit-code change.
- The selection rules, claim/defer/complete transitions, retained-work resume,
  and every other section of the skill.
- Other skills that consume a baseline, including
  `.agents/references/subagent-reporting.md`.

## Risk tier and invariants

Expected Tier 2 (scoped skill behavior: which commit a session records as its
baseline). The invariant that must survive: every delegated brief and every
diff-scoped review in the session measures against the same single commit the
session actually starts from after the claim.

## Acceptance criteria

- `## Preconditions and selection` no longer presents a pre-claim baseline as
  authoritative, and a reader following it records `sync.to` as the baseline
  when the claim fast-forwarded.
- No file under `.agents/skills/next-plan/scripts/` is changed.
- /validate-skill passes for `.agents/skills/next-plan/SKILL.md`; plan validate
  exits 0.

## Notes

Depends on nothing: the separate `Get-NextPlanContext` invocation-line fix in
the same section lands with the session that recorded this Plan, so an
implementer will find that line already present.
