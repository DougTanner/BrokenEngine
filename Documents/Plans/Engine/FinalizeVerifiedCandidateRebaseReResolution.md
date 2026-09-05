<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T17:10:06.946Z","dependsOn":[]} -->
# Fix: the verified-candidate gate is unsatisfiable after a documented recovery rebase

## Context

Observed symptom, in one session:

- The primary-movement check blocked with `primary.path-overlap` (foreign
  primary movement touched a session-owned path). The manager authorized the
  documented recovery, and the finalizer ran the ordinary linear
  `git rebase refs/heads/main` and re-invoked
  `pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeApprovalPreparation.ps1 ...`
  with `-ExpectedCurrentTip` and `-ExpectedPrimaryTip` re-resolved and its other
  arguments unchanged, exactly as
  `.agents/skills/finalize-changes/references/scripts.md:134-142` prescribes.
- That re-invocation returned exit 2, `candidate.tree-changed`, "Reconciled
  session tree no longer equals the reviewed candidate tree."
- Rework it forced: the finalizer dropped the optional
  `-VerifiedCandidateCommit`/`-VerifiedCandidateTree` pair and re-invoked
  without it. The run that then produced the landing candidate records
  `"verifiedCandidate":{"supplied":false,"matched":false}` in
  `Temp/finalize-approval-preparation-result.json`, i.e. the reviewed-tree gate
  was not merely blocked, it was abandoned for the landing that followed.

Why the block is structural rather than a one-off, read from the current tree:

- `Invoke-FinalizeApprovalPreparation.ps1:353` compares the reconciled session
  tip's whole tree against `-VerifiedCandidateTree`
  (`if ($verifiedCandidateCommitBound -and $originalTree -cne $VerifiedCandidateTree)`).
- After any rebase onto a moved primary, the session tip's tree contains every
  foreign commit's content too, so whole-tree equality with a pre-rebase
  candidate tree cannot hold unless primary moved by nothing at all. The
  recovery route that `scripts.md:134-142` documents therefore always trips this
  gate, and its instruction that the re-invocation carries "its other arguments
  unchanged" is what carries the now-stale pair into it.
- `.agents/skills/finalize-changes/references/worker.md:237-246` already
  requires that after this same recovery rebase the finalizer inspects any place
  the rebase merged cleanly but changed the code's meaning and re-runs the
  verification step for the changed regions. So a re-reviewed post-rebase tree
  exists at that point; nothing tells the finalizer to use it as the new
  verified pair.

Effect: the one gate whose purpose is proving the reviewed content still is what
lands is unsatisfiable on a documented recovery path, and the only way forward
the contract leaves is to switch it off.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 552a615b-9166-4c5b-8991-7e9c279d2dcf
- Worktree/branch UUID: def6cf77-44da-402b-b5e4-a597e25a7971
- Session branch: claude/def6cf77-44da-402b-b5e4-a597e25a7971
- Worktree: .claude\worktrees\BrokenEngine\def6cf77-44da-402b-b5e4-a597e25a7971
- Landing ref: claude/def6cf77-44da-402b-b5e4-a597e25a7971
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design

First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/def6cf77-44da-402b-b5e4-a597e25a7971` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

Author's recommendation: fix the contract, not the comparison. State in
`scripts.md`'s `Invoke-FinalizeApprovalPreparation.ps1` entry that a recovery
re-invocation after the authorized rebase re-resolves the
`-VerifiedCandidateCommit`/`-VerifiedCandidateTree` pair from the re-reviewed
rebased session tip, alongside the two tips it already re-resolves, instead of
carrying the pre-rebase pair; and state in `worker.md`'s `primary.path-overlap`
bullet that the re-review it already requires is what produces that new pair.
Rationale: the gate's job is to prove that nothing drifted between review and
landing, and after the rebase the reviewed artifact genuinely is the rebased
tree — the pre-rebase tree is no longer a thing anyone is landing. This keeps
the gate live for the whole post-rebase window at the cost of prose only, and
it removes the incentive to drop the pair, which is what actually happened.

Alternatives considered and not recommended:

- Compare patch identity (for example `git patch-id` over the session's own
  diff against its merge-base) instead of whole trees at `:353`. Rejected: it
  adds a second identity notion to the finalize scripts to approximate what
  re-resolving the pair states exactly, it is not equality-preserving under a
  rebase that resolved content (which the overlap case is precisely about), and
  it would silently pass a rebase whose clean merge changed meaning — the case
  `worker.md:237-246` exists to catch.
- Skip the comparison whenever the run follows a rebase. Rejected: the script
  cannot tell, and a flag saying so is a caller assertion, which is exactly the
  trust the gate is supposed to remove.
- Leave it and let the finalizer drop the pair. Rejected: that is the observed
  workaround, and it silently removes the gate from the landings most likely to
  need it.

If root-causing shows the intended contract was always to re-resolve the pair
and only the wording is missing, the fix is that wording alone and no script
changes.

## Critical files

- `.agents/skills/finalize-changes/references/scripts.md` — the
  `Invoke-FinalizeApprovalPreparation.ps1` entry: the recovery re-invocation
  rule (`:134-142`) and the verified-candidate pair description (`:148-158`).
- `.agents/skills/finalize-changes/references/worker.md` — the
  `primary.path-overlap` recovery bullet (`:237-246`).
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeApprovalPreparation.ps1`
  — `:353`, the comparison, read as the behavior the prose must describe; change
  it only if root-causing proves the prose fix cannot hold.

## In scope

- Root-cause investigation as `## Design` states.
- The smallest resulting fix, confined to the recovery re-invocation and
  verified-candidate wording in `references/scripts.md` and the
  `primary.path-overlap` bullet in `references/worker.md`, plus the `:353`
  comparison in `Invoke-FinalizeApprovalPreparation.ps1` only if the
  investigation proves prose alone cannot fix it.

## Out of scope

- The verified-candidate input validation at `:277-279` and the worktree
  argument it uses — owned by
  `Documents/Plans/Engine/FinalizeVerifiedCandidateWorktreeIdentity.md`.
- The squash behavior, the parenting rule, the result schema's other fields,
  and every other finalize script.
- The rebase itself, the landing lock, the confirmation gate, and primary
  advancement.
- Any new caller-supplied flag describing how the session tip was reached.
- The landed change the session produced.
- Unrelated skills/scripts; any transcript path or transcript text in the repo.

## Coordination

No directional prerequisite (`dependsOn: []`).
`Documents/Plans/Engine/FinalizeVerifiedCandidateWorktreeIdentity.md` changes the
same script's verified-candidate region (`:277-279`) and reads
`references/scripts.md` as unchanged; this Plan changes `references/scripts.md`
and `references/worker.md` prose and touches `:353`. The two fixes are
independent and either may land first, but whichever lands second re-reads the
current bytes of both files before editing, because the cited line numbers will
have moved.

## Risk tier and invariants

Expected Tier 2 (scoped behavior of one tool contract on the landing path);
escalate if the fix reaches primary advancement or the landing lock. Never embed
transcript paths or home paths.

## Acceptance criteria

1. The documented recovery route, followed end to end, leaves the
   verified-candidate pair supplied: a re-invocation after the authorized rebase
   returns `verifiedCandidate.supplied` true and `matched` true, without the
   caller dropping the pair.
2. A session tip whose tree genuinely differs from the reviewed tree, with no
   rebase in between, still blocks with exit 2, `candidate.tree-changed`.
3. `plan validate` exits 0, and /validate-skill passes wherever the root
   AGENTS.md Apply the triggered cleanup step triggers it.

## Notes

Change Workflow tier for the fix: **Tier 2** — scoped behavior of one tool's
documented contract, with no determinism/CRC, wire, serialization, save/replay,
or threading exposure and no trust-boundary change; it is not Tier 1 because it
changes what a finalizer supplies to the gate that guards a landing.
