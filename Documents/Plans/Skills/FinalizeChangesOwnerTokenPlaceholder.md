<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-21T12:20:41.544Z","dependsOn":[]} -->
# Fix: /finalize-changes — the `<owner-token>` placeholder never states its GUID format

## Context
During this session's landing execution at a `/next-plan` claim exit, the
finalizer ran the documented lock-claim command from
`.agents/skills/finalize-changes/references/scripts.md:18`:

```text
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeLockClaim.ps1 -WorktreeCliExecutable '<worktreecli-exe>' -GitCommonDirectory '<git-common-dir>' -SessionLabel '<session-label>' -Worktree '<primary-worktree>' -LandingOwner '<owner-token>' -LeaseSeconds '3600'
```

The call was rejected with `landing-lock.owner-token-invalid` because the
substituted `-LandingOwner` value was not a canonical lowercase GUID. The
rejecting guard is
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeLockClaim.ps1:80-82`,
whose own message names the two sanctioned sources — omit `-LandingOwner` to
have the script mint one, or take one from `WorktreeCli lock token`. The
finalizer then minted a canonical token through `WorktreeCli lock token` and the
call succeeded, so the cost was exactly one blocked call.

The reference never states that format. Every owner-token argument in it is the
bare placeholder `'<owner-token>'`: `-LandingOwner` at
`.agents/skills/finalize-changes/references/scripts.md:18`, `:22`, and `:23`, and
`-OwnerToken` at that same file's `:19`, `:24`, and `:25`. The
`Invoke-FinalizeLockClaim.ps1` contract bullet at
`.agents/skills/finalize-changes/references/scripts.md:87-115` describes the
token's lease and orphan-release behavior without ever saying what a valid value
looks like or where to get one. The only mention of minting through
`WorktreeCli lock token` in that file — line 142 — describes what
`Invoke-FinalizeLanding.ps1` does internally when no token is passed, not how a
caller obtains one.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 831d5cf0-38db-4fbd-beaf-1e093fb7d715
- Worktree/branch UUID: a46aa082-e436-4375-af82-d3272ab2c4b0
- Session branch: claude/a46aa082-e436-4375-af82-d3272ab2c4b0
- Worktree: .claude\worktrees\BrokenEngine\a46aa082-e436-4375-af82-d3272ab2c4b0
- Landed commit of the observing session:
  b18439b489cd2e3ad73d81867f79c06a321679f1 — this session's own landing, and the
  immutable review ref `/next-plan-review` targets. The `Landing ref` line below
  names the mutable branch only because its tree, unlike this commit's, contains
  this Plan.
- Landing ref: claude/a46aa082-e436-4375-af82-d3272ab2c4b0
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Skills/FinalizeChangesOwnerTokenPlaceholder.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
In a new session, run
`/next-plan-review b18439b489cd2e3ad73d81867f79c06a321679f1` — the observing
session's landed commit above — supplying the recorded client `claude` and the
recorded conversation session ID. Root-cause the friction from the proven transcript, then make the
smallest fix inside the `## In scope` boundary below. If root-causing shows the
fix lies outside that boundary, surface it for re-planning instead of expanding
scope.

The mechanism this Plan authorizes is documentation-only: state, once, in
`.agents/skills/finalize-changes/references/scripts.md`, what the
`'<owner-token>'` placeholder must be and where a caller gets one. Two separate
things need saying, and conflating them would make the reference wrong:

- Obtaining a token, which happens exactly once per landing, at the lock claim.
  `Invoke-FinalizeLockClaim.ps1` accepts `-LandingOwner` only as a canonical
  lowercase GUID (`Invoke-FinalizeLockClaim.ps1:80-82`), and omitting the
  parameter there makes the script mint the token itself, so the caller must read
  it back from the claim result to keep it. Minting one explicitly beforehand
  through `WorktreeCli lock token` is the other sanctioned source, and it is the
  one that survives a host kill, per the existing orphan-release wording at
  `references/scripts.md:98-103`.
- Passing that already-held token to the later commands, which do not mint
  anything. `Invoke-FinalizeCandidateCommit.ps1 -AdvancePrimary` requires
  `-OwnerToken` and rejects an omitted or non-canonical value with
  `landing-lock.owner-required`
  (`Invoke-FinalizeCandidateCommit.ps1:80`), so "omit it and the script mints
  one" must never be stated as a general rule.

Changing any guard, the accepted token format, or any script behavior is
deliberately not this Plan's mechanism: the value really must be canonical, and
the observed cost was purely a missing statement.

## Critical files
- `.agents/skills/finalize-changes/references/scripts.md` — the authorized fix
  boundary: the `## Invocation` placeholder usage at lines 13 and 18-25, the
  `Invoke-FinalizeLockClaim.ps1` contract bullet at lines 87-115, and the
  `-AdvancePrimary` `-OwnerToken` sentence at lines 59-62.

## In scope
- Root-cause investigation via /next-plan-review, run with client `claude`, the
  review ref named in `## Design`, and the recorded conversation session ID.
- The smallest resulting documentation fix, confined to
  `.agents/skills/finalize-changes/references/scripts.md`, stating the owner
  token's canonical-lowercase-GUID format, where the lock claim gets one, and
  that the later commands only carry the already-held token.

## Out of scope
- The landed change the session produced.
- `Invoke-FinalizeLockClaim.ps1`, `Invoke-FinalizeLanding.ps1`,
  `Invoke-FinalizeCandidateCommit.ps1`, and every other bundled script — no
  guard, format, message, or behavior change.
- `.agents/skills/finalize-changes/SKILL.md` behavior: no change to the workflow,
  step ordering, or lock-claim rules.
- Unrelated skills/scripts; any transcript path or transcript text in the repo.

## Risk tier and invariants
Expected Tier 1 (documentation-only wording in one reference file); escalate if
root-causing shows the fix must change script or skill behavior. Never embed
transcript paths or home paths. The invariant that must survive: the canonical
invocation text stays byte-identical and every placeholder remains quoted.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a
  reader substituting `-LandingOwner` from the reference alone supplies a
  canonical lowercase GUID and is not rejected with
  `landing-lock.owner-token-invalid`.
- The reference names, for the lock claim, both sanctioned sources that
  `Invoke-FinalizeLockClaim.ps1`'s own error message names — omit `-LandingOwner`
  so the claim mints and returns one, or mint one with `WorktreeCli lock token` —
  and does not state that omission mints a token for
  `Invoke-FinalizeCandidateCommit.ps1 -AdvancePrimary`, which requires the
  already-held token.
- plan validate exits 0.
