<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-14T17:29:42.338Z","dependsOn":[]} -->
# Fix: Invoke-NextPlanClaim.ps1 / Get-NextPlanList.ps1 — a Plan present at the primary tip is reported as unavailable when the session worktree is behind primary, with no staleness signal

## Context

During a `/next-plan` run, the documented targeted claim invocation was run from
the session worktree root:

```powershell
pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan 'Documents/Plans/Engine/FailureReportingOutReferenceConvention.md'
```

It exited `0` and returned `status: pass`, `code: none-available`, message
`No eligible Plans plan is available.` The named Plan existed at the primary tip
at that moment (commit `335413f8559da95de89a02d85e884690d11eedef`). The session
worktree was two commits behind primary (session HEAD
`8cf8e4c4bfd9a86e7da46ebf35809079b628e6dc`).

The read-only queue listing showed the same absence:

```powershell
pwsh -NoProfile -File .agents/skills/next-plan/scripts/Get-NextPlanList.ps1
```

omitted the Plan entirely, with nothing in either result indicating that the
session worktree was behind primary or naming the primary tip the inventory
disagreed with.

Neither result is actionable on its own: the documented reading of
`none-available` for a `-Plan`-targeted invocation is that the requested Plan is
ineligible (`.agents/skills/next-plan/SKILL.md:53-57`), which is
indistinguishable here from "your worktree does not contain it yet". Recovery
required un-documented manual diagnosis — `git rev-list --left-right --count
HEAD...main` in the session worktree to discover the two-commit gap — followed by
a fast-forward of the session branch, after which the identical claim invocation
succeeded.

The reachable mechanism is visible in the script without deciding the root cause:
`Invoke-NextPlanClaim.ps1:22` derives the executable-Plan inventory from
`plan validate --repo <common-dir> --worktree <session worktree>`, so the
inventory is read from the session worktree's tree, while `plan claim-next`
(`:42`) is additionally handed `--primary-worktree`. The exact root cause and the
correct owner of the fix are deferred to `/next-plan-review`.

The claimed Plan for this run was
`Documents/Plans/Engine/FailureReportingOutReferenceConvention.md`, whose
`## In scope` covers only `engine::DifferenceStreamReader` and its game consumer
in C++ plus one `Engine/Source/File/AGENTS.md` sentence. Every script named below
is outside that boundary, so this is `/next-plan` tooling friction rather than an
in-scope acceptance failure.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Session: 68da05af-c6e0-4e32-979d-87ba545ba868
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

The symptom already narrows the outcome to be delivered: a caller running the
documented claim or list invocation from a session worktree that is behind
primary can tell that from the result alone — a distinct code or notice naming
the primary tip — or the inventory is refreshed from primary so the Plan is
selectable. Which of those two, and which component owns it, is what root-causing
decides; they are alternatives, not a set to implement together.

## Critical files

- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1` — the
  `plan validate` inventory derivation at `:22`, its projection at `:23`, the
  `plan claim-next` invocation at `:42`, and the `none-available` mapping at
  `:46`
- `.agents/skills/next-plan/scripts/Get-NextPlanList.ps1` — the listing process
  and its JSON projection
- `.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1` — the
  `Get-NextPlanContext` session/primary identity boundary the two scripts share
- `.agents/skills/next-plan/SKILL.md` — the queue-listing paragraph (`:33-38`)
  and the `none-available` reading at `:53-57`

## In scope

- Root-cause investigation via `/next-plan-review` with the recorded provenance
- The smallest resulting fix, confined to the four files named above: the result
  code, notice, or inventory source used by the claim and listing paths, and the
  `/next-plan` prose that tells a caller how to read them

## Out of scope

- The landed change this session produced, and its claimed Plan
- `Tools/WorktreeCli/**` and WorktreeCli scheduler behavior; if root-causing
  proves the smallest fix belongs there, surface it for re-planning rather than
  expanding this boundary
- Any change to which Plan the scheduler selects, to claim ownership, expiry,
  self-healing, or to the read-only guarantee of the listing path
- Automatically moving, rebasing, or fast-forwarding the session branch as part
  of a claim or listing invocation
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2 (scoped `/next-plan` tool behavior); escalate if the fix reaches
WorktreeCli scheduler or AgentTools build/bootstrap coordination. The listing
path stays read-only — no claim, guard, healing, or storage mutation — and a
genuinely ineligible Plan must still report a normal stop rather than being
silently treated as claimable. Never embed transcript paths or home paths. No
determinism/CRC, serialization, replay, wire, threading, allocation, shader, or
build exposure.

## Acceptance criteria

- The recorded symptom no longer reproduces: with the session worktree behind
  primary and the requested Plan present at the primary tip, the documented
  claim invocation either reports the staleness distinctly, naming the primary
  tip, or the Plan is selectable — in neither case does the caller need manual
  Git diagnosis to learn why
- The documented listing invocation carries the same signal for the same state
- A Plan that is genuinely ineligible with an up-to-date session worktree still
  returns the existing normal stop unchanged
- `/validate-skill` passes for any changed `SKILL.md`; WorktreeCli
  `plan validate` exits `0` with `status: valid` and `code: ok`

## Notes

`.agents/skills/next-plan/SKILL.md:137-139` excludes "documented normal stops
such as `none-available`" from tooling friction. That exclusion does not cover
this observation: the stop was reported for a Plan that existed at the primary
tip, so the result contradicted repository state, and un-documented manual Git
diagnosis plus a branch fast-forward were required before the identical
invocation succeeded. The friction recorded here is the missing staleness
signal, not the existence of the `none-available` stop.

This Plan is keyed to the pair (`Invoke-NextPlanClaim.ps1` /
`Get-NextPlanList.ps1`, a Plan present at the primary tip reported absent with no
staleness signal while the session worktree is behind primary). A later
observation of the same pair is a duplicate, not a new residual.
`Documents/Plans/Agents/NextPlanListGitIdentityMismatch.md` covers a different
symptom of the listing script — exit `1` with code `git-identity-mismatch` after
final candidate preparation and rebase — and is not a duplicate; both may edit
`Get-NextPlanList.ps1` and `NextPlanWorkflowCommon.psm1`, which the
`## Coordination` section below states.

## Coordination

- `Documents/Plans/Agents/NextPlanListGitIdentityMismatch.md` owns the
  `git-identity-mismatch` failure of `Get-NextPlanList.ps1` and may edit the same
  `Get-NextPlanList.ps1` listing path and the same `Get-NextPlanContext` boundary
  in `NextPlanWorkflowCommon.psm1`. The two symptoms and their fixes are
  distinct, but the regions overlap: whichever lands second rebases onto the
  first and re-reads those regions before editing, and the session-versus-primary
  identity handling ends up stated once rather than as two parallel mechanisms.
