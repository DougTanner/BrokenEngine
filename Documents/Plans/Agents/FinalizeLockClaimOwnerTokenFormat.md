<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-16T20:34:10.931Z","dependsOn":[]} -->
# Fix: finalize-changes landing lock — the claim script accepts an owner token the landing script rejects

## Context

During `/finalize-changes`, immediately after the affirmative user confirmation,
the finalizer claimed the landing lock with
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeLockClaim.ps1` passing
`-LandingOwner '<uuid>-landing'` — a non-GUID token. The claim succeeded and
issued a live lease under that exact token. The documented next step,
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1 ... -OwnerToken '<uuid>-landing'`,
then exited `1` with code `input.owner-token-invalid` and message "OwnerToken
must be a canonical WorktreeCli lock owner token.". Nothing advanced and no
lease was orphaned; the finalizer released the lease with
`Invoke-FinalizeLockClaim.ps1 -Release`, re-claimed under the canonical GUID,
and landed. Cost: one extra release/re-claim round on the landing path, after
the user's confirmation was already given.

The asymmetry is in the two scripts:

- `Invoke-FinalizeLockClaim.ps1:11` declares `[string] $LandingOwner` with no
  validation attribute, and the only test applied to it anywhere in the script
  is `[string]::IsNullOrWhiteSpace(...)` at `:54` and `:78`, which mints a token
  through WorktreeCli `lock token` only when the parameter is absent. Any
  non-empty string is passed straight through to the claim at `:86`.
- `Invoke-FinalizeLanding.ps1:524-525` rejects any supplied `-OwnerToken` that
  does not match `^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$`
  with `Throw-Landing 1 'input.owner-token-invalid' 'OwnerToken must be a
  canonical WorktreeCli lock owner token.'`.

Nothing documents the format for a caller-supplied token.
`.agents/skills/finalize-changes/references/scripts.md` shows the placeholder
`-LandingOwner '<owner-token>'` only on the `-Release` command (`:19`) — the
claim command (`:18`) omits the parameter entirely — and the landing
command with `-OwnerToken '<owner-token>'` (`:20`), and its prose about
claiming with `-LandingOwner` (`:49-77`) and `-OwnerToken` (`:94-99`) covers
timeouts, orphaning, and the same-actor continuation rule but never states that the value
must be a canonical lowercase GUID.
`.agents/skills/finalize-changes/SKILL.md:51-68` likewise describes passing "that
owner token" between the two scripts without stating its format. The mismatch is
therefore invisible until landing time, when a lease is already live.

The misbehaving surfaces are the finalize-changes landing-lock scripts and their
reference, which were outside the `## In scope` boundary of the work this session
landed, so this is `/next-plan` tooling friction and not an in-scope acceptance
failure of that change.

Verify every cited line number against the working tree before editing — the
numbers above are from this session and the files may have moved since.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: 3e9eb8fe-a459-4eb1-9b42-614e3db7bd4a
- Worktree/branch UUID: 2476210d-562c-4f3a-92ba-aa7f638c717c
- Session branch: claude/2476210d-562c-4f3a-92ba-aa7f638c717c
- Worktree: .claude\worktrees\BrokenEngine\2476210d-562c-4f3a-92ba-aa7f638c717c
- Landing ref: the session branch above, whose tip is the session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to this session alone
  (its diff limited to this session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact conversation session ID above.

## Design

In a new session, run `/next-plan-review <landing ref>` supplying the recorded
client and the recorded conversation session ID; root-cause the friction from
the proven transcript, then make the smallest fix inside the `## In scope`
boundary below. If root-causing shows the fix lies outside that boundary,
surface it for re-planning instead of expanding scope.

The outcome to deliver: a `-LandingOwner` value the claim script accepts is
always a value the landing script accepts, or the caller learns the required
format before a lease exists. Two candidate shapes are visible from the symptom,
and root-causing decides between them; they are alternatives, not a set to
implement together:

1. Validate in the claim script: reject a supplied `-LandingOwner` that does not
   match the same canonical pattern `Invoke-FinalizeLanding.ps1` enforces,
   before any lease is claimed, with a typed blocked/error result naming the
   required format. Fail-closed and no lease issued, so the mismatch costs no
   release/re-claim round. Rejecting is preferred over silently normalizing,
   because a normalized token would no longer be the string the caller recorded
   and could be released under the wrong value.
2. Document the format: state in
   `.agents/skills/finalize-changes/references/scripts.md`, where
   `-LandingOwner` and `-OwnerToken` are documented, that a caller-supplied
   owner token must be a canonical lowercase GUID as produced by WorktreeCli
   `lock token`.

If the review proves shape 1, the existing fixture coverage in
`.agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1`
(which drives `-LandingOwner` with GUID values throughout, for example `:512`
and `:519`) gains one case for the rejected non-GUID token and must keep passing
otherwise.

## Critical files

- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLockClaim.ps1` — the
  `-LandingOwner` parameter declaration (`:11`) and the mint-when-absent branches
  (`:54`, `:78-86`); this is where a format check would live
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1` —
  read-only reference for the enforced pattern and its code and message
  (`:524-525`); not edited
- `.agents/skills/finalize-changes/references/scripts.md` — the `-LandingOwner`
  and `-OwnerToken` documentation (`:19-20`, `:49-77`, `:94-99`)
- `.agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1` —
  the lock-claim fixtures, extended only if the chosen shape adds validation

## In scope

- Root-cause investigation via `/next-plan-review`, run with the recorded
  landing ref, client, and conversation session ID
- The smallest resulting fix, confined to the files named above: an owner-token
  format check in `Invoke-FinalizeLockClaim.ps1` before the claim, and/or the
  owner-token format statement in `references/scripts.md`, plus the matching
  fixture case

## Out of scope

- Any change to `Invoke-FinalizeLanding.ps1` behavior, its owner-token pattern,
  its `input.owner-token-invalid` code or message, and the compare-and-swap
  advance, rollback, and lock-release logic
- Lease duration, wait and poll bounds, expiry recovery, the same-actor
  continuation and no-steal rules, and the `-Release` path's semantics
- `Tools/WorktreeCli/**` lock storage, `lock token` output, and lock commands
- The landed change the session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior on the landing path); escalate to Tier 3
if the fix reaches the landing advance, lock storage, or build/bootstrap
coordination. The landing lock must stay fail-closed: a rejected token must
leave no lease claimed, an accepted token must still be the exact string the
caller can later release with `-Release`, and no invocation may steal or disturb
a foreign lease. The canonical invocation forms stay byte-identical. Never embed
transcript paths or home paths.

## Acceptance criteria

- The recorded symptom no longer reproduces: a non-GUID `-LandingOwner` either
  is refused before any lease is claimed, with a typed result naming the
  required format, or is ruled out by documentation the caller reads before
  invoking — in neither case does a live lease exist under a token
  `Invoke-FinalizeLanding.ps1` will reject
- A canonical GUID token continues to claim, continue into landing under
  `-OwnerToken`, and release exactly as it does today
- `Test-FinalizeWorkflowFixtures.ps1` passes, including any added case
- `/validate-skill` passes for any changed `SKILL.md`; WorktreeCli
  `plan validate` exits `0` with `status: valid` and `code: ok`

## Coordination

`Documents/Plans/Agents/FinalizeApprovalCommitShaFormat.md` is keyed to a
different symptom (the documented approval-preparation invocation does not state
that commit-valued parameters reject abbreviated SHAs) but its fix edits the
same `.agents/skills/finalize-changes/references/scripts.md` `## Invocation`
region this Plan may touch if shape 2 is selected. The two are not directional:
either may land first, and whichever lands second re-reads that section as it
then stands and keeps the other's text intact. That Plan owns the commit-SHA
format statement; this Plan owns the owner-token format.

## Notes

This Plan is keyed to the (script pair, symptom) pair:
`Invoke-FinalizeLockClaim.ps1` accepts and issues a live lease under a non-GUID
`-LandingOwner` that `Invoke-FinalizeLanding.ps1` then rejects with
`input.owner-token-invalid`, costing a release and re-claim after user
confirmation. A later observation of the same pair is a duplicate, not a new
residual. `Documents/Plans/Agents/FinalizeApprovalReviewInvocationContract.md`
and `Documents/Plans/Agents/AgentHarnessOwnerTokenReclaimLoop.md` (a different
tool's owner token, and a bookkeeping loop rather than a format mismatch) are
not duplicates. The proven root cause is deferred to `/next-plan-review`.
