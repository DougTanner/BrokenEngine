<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T16:19:10.545Z","dependsOn":[]} -->
# Resolve the worktree identity before the verified-candidate checks

## Context

`Invoke-FinalizeApprovalPreparation.ps1` validates the optional
`-VerifiedCandidateCommit`/`-VerifiedCandidateTree` pair before it resolves the
worktree it was given:

- `:278` `Assert-Input (Test-FinalizeGitSuccess $CurrentWorktree @('rev-parse',
  '--verify', "$VerifiedCandidateCommit^{commit}")) 'Verified candidate commit
  does not exist.'`
- `:279` the matching tree comparison, also on raw `$CurrentWorktree`
- `:288` `$script:CurrentIdentity = Get-FinalizeExistingWindowsIdentity
  $CurrentWorktree 'Current worktree'` — every other Git call in the script uses
  that resolved identity, never the raw parameter.

`Get-FinalizeExistingWindowsIdentity` (`.agents/scripts/FinalizeWorkflowCommon.psm1:42-51`,
through `Get-FinalizeRootPreservingFullPath` at `:5-12`) turns the input into an
absolute final path, so a relative `-CurrentWorktree` works for the whole script
after `:288`. Before it, the same value is applied twice: `Invoke-FinalizeGit`
and `Test-FinalizeGitSuccess` (`:89-97`) build `git -C <worktree>` while
`Invoke-FinalizeNativeText` (`:63-66`) also sets the child process working
directory to that same string, so the child starts in `<cwd>/<relative>` and
then `-C <relative>` resolves against that, naming a directory that does not
exist. Git exits non-zero, `Test-FinalizeGitSuccess:96` discards its stderr and
returns `$false`, and `:278` reports `input.invalid` "Verified candidate commit
does not exist." for a commit that does exist. Observed live in a scratch
repository: the identical invocation with an absolute `-CurrentWorktree` passes;
with a relative one it fails at `:278`. `:279` would surface Git's real error
through `Invoke-FinalizeGit`, but `:278` fires first.

`references/scripts.md` `## Invocation` (`:28-56`) documents
`-CurrentWorktree '<current-worktree>'` without stating that the value must be
absolute, and its sourcing guidance names `Get-AgentWorktreeSessionContext` only
for `<baseline>` and `<session-label>`. So a relative value is neither a
documented input nor a forbidden one, and the script accepts it everywhere
except these two checks. The sibling scripts resolve first and do not have the
defect: `Invoke-FinalizePrimaryMovementCheck.ps1:290`,
`Invoke-FinalizeLanding.ps1:887`, `Invoke-FinalizeCandidateCommit.ps1:61`.

Impact: a finalizer that passes a relative session-worktree path is told its
already-reviewed candidate commit is missing — a false, unactionable input
rejection at the one check whose purpose is to prove the reviewed tree is
unchanged. Nothing is corrupted; the run simply blocks with the wrong reason.
Line numbers above are from the current tree and may shift.

## Design

Author's recommendation: move the two verified-candidate assertions below the
`Get-FinalizeExistingWindowsIdentity` call that sets `$script:CurrentIdentity`,
and pass `$script:CurrentIdentity` to them, matching every other Git call in the
script. The purely textual assertions that precede them — the 40-character
object-ID format check and the commit/tree paired-supply check — need no Git and
stay where they are, so a malformed pair is still rejected before any process
starts. After the move, a `-CurrentWorktree` that does not exist fails first
with the existing "Current worktree does not exist" message, which is the
correct diagnosis, and the "Verified candidate commit does not exist." message
becomes true whenever it is emitted.

The alternative — declaring `-CurrentWorktree` and `-PrimaryWorktree`
absolute-only in `scripts.md` and asserting it in the script — is not
recommended: it adds an input rule to every finalize script's documented form to
protect two lines, and leaves the same double-applied-path trap for any future
pre-identity Git call.

`Test-FinalizeGitSuccess` discarding stderr is correct for a boolean helper and
should stay as it is; after the move no wrong-path failure can reach it.

## Critical files

- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeApprovalPreparation.ps1`
  — the verified-candidate assertions (`:278-279`) and the identity resolution
  at `:288`.
- `.agents/scripts/FinalizeWorkflowCommon.psm1` — `:42-51`, `:63-66`, `:89-97`,
  read as the unchanged helper contract.
- `.agents/skills/finalize-changes/references/scripts.md` — `## Invocation`,
  read as the unchanged documented input contract: the move keeps a relative
  `-CurrentWorktree` accepted, so the documented form still holds.

## In scope

- The placement of the two verified-candidate Git assertions in
  `Invoke-FinalizeApprovalPreparation.ps1` relative to the
  `Get-FinalizeExistingWindowsIdentity` call, and the worktree argument they
  pass.

## Out of scope

- `FinalizeWorkflowCommon.psm1`: no change to `Invoke-FinalizeNativeText`,
  `Invoke-FinalizeGit`, `Test-FinalizeGitSuccess`, or the identity helpers,
  including making them tolerate relative paths generally.
- The meaning of the verified-candidate gate, its result fields, its error code,
  and its message text.
- The other finalize scripts, the landing lock, the confirmation gate, and
  primary advancement.
- Any new absolute-path input rule for the finalize scripts.
- `references/scripts.md`: no wording change to `## Invocation` or to any other
  documented finalize-script input.

## Acceptance criteria

The diff alone does not prove the behavior, so, in a scratch repository:

1. Before the fix, one invocation with a relative `-CurrentWorktree` and a valid
   `-VerifiedCandidateCommit`/`-VerifiedCandidateTree` pair returns
   `input.invalid` "Verified candidate commit does not exist."; after the fix,
   the same invocation produces the same result as the identical invocation with
   an absolute `-CurrentWorktree`.
2. A commit ID that genuinely does not exist in the session worktree still
   returns `input.invalid` with that same message, and a tree that does not
   match its commit still returns the existing tree-mismatch rejection.
3. A `-CurrentWorktree` naming a directory that does not exist fails with the
   existing "Current worktree does not exist" error.

## Coordination

No directional prerequisite (`dependsOn: []`).
`Documents/Plans/Engine/FinalizeVerifiedCandidateRebaseReResolution.md` changes
the same script's whole-tree verified-candidate comparison (`:353`) and the
`references/scripts.md` and `references/worker.md` prose this Plan reads as
unchanged; this Plan moves the two verified-candidate assertions at `:278-279`.
The two fixes are independent and either may land first, but whichever lands
second re-reads the current bytes of both files before editing, because the
cited line numbers will have moved.

## Notes

Change Workflow tier for the fix: **Tier 2** — scoped behavior of one tool
script's input validation. It touches no determinism/CRC, wire, serialization,
save/replay, or threading surface, changes no trust boundary, and never advances
primary or touches the landing lock; it is not Tier 1 because it changes which
check fires first and therefore what a caller is told.

The fix session should re-read the current bytes before editing: this defect was
recorded while a separate change to the same script was in flight, so the cited
line numbers, but not the two assertions or the identity call, may have moved.
