<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-12T17:36:48.152Z","dependsOn":[]} -->
# Fix: Get-NextPlanList.ps1 — plan list rejects prepared session with git-identity-mismatch

## Context

After final candidate preparation and rebase for the Tier-1 consistency
cleanup in `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1`, the
documented read-only queue invocation was run from the wrapper worktree root:

```powershell
pwsh -NoProfile -File .agents/skills/next-plan/scripts/Get-NextPlanList.ps1
```

It exited `1` and returned the listing code/output `git-identity-mismatch`.
The finalizer did not retry or work around the failure. This blocked progression
to `/verify-changes` and forced a follow-up-plan creation/re-preparation cycle.
The prepared candidate was `3d6c3099cdec6e267b6fc3692e3397c2186c1007`, its
parent/current primary was `9ff9a686d850df655cf39e24f618cb9c30222780`, and the
original baseline was `b4556e529f7a0795cdc6d0e8202747220287377f`.

The claimed Plan completion deleted
`Documents/Plans/Agents/CodexReviewDiffPathspecLiteral.md`; its approved
`## In scope` covered only the Tier-1 cleanup of
`.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1`. The listing
script and its caller modules are outside that boundary, so this is `/next-plan`
tooling friction rather than an in-scope acceptance failure.

The direct caller chain is `Get-NextPlanList.ps1:7-17` to
`NextPlanWorkflowCommon.psm1:4-50`, which resolves the session through
`AgentWorktreeSession.psm1:66-117` and invokes native processes through
`FinalizeWorkflowCommon.psm1:63-104`. The read-only WorktreeCli `RunList`
identity/ancestor guard is at `Tools/WorktreeCli/PlanScheduler.cpp:568-585`,
where the observed `git-identity-mismatch` code is produced. The exact root
cause is intentionally deferred to `/next-plan-review`.

Session provenance (machine-local; not reproducible after cleanup):
- Client: codex
- Session: `49a5c274-72d2-4731-a888-a688a0870fbf`
- Session branch: `codex/49a5c274-72d2-4731-a888-a688a0870fbf`
- Worktree: `.codex\worktrees\BrokenEngine\49a5c274-72d2-4731-a888-a688a0870fbf`
- Landing commit: `git log --diff-filter=A --format=%H -- <this plan path>`
- Run the review before /cleanup-worktrees removes this worktree: Codex transcript discovery requires the producing worktree to remain registered, and Claude review requires the exact session id above.

## Design

In a new session, run `/next-plan-review <landing commit>` supplying the
recorded client and session id, root-cause the friction from the proven
transcript, then make the smallest fix inside the `## In scope` boundary below.
If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files

- `.agents/skills/next-plan/SKILL.md` — read-only queue-list invocation and tooling-friction review contract (`:25-30`, `:140-159`).
- `.agents/skills/next-plan/scripts/Get-NextPlanList.ps1` — list process, JSON projection, and exit mapping (`:7-17`).
- `.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1` — imported common modules, session context, native process, and JSON boundaries (`:4-50`).
- `.agents/scripts/FinalizeWorkflowCommon.psm1` — `Invoke-FinalizeNativeText` and `Get-FinalizeGitIdentity` used by the listing caller (`:63-104`).
- `.agents/scripts/AgentWorktreeSession.psm1` — `Get-AgentWorktreeSessionContext` used to resolve the session and primary identities (`:66-117`).

## In scope

- Root-cause investigation via `/next-plan-review` with the recorded provenance.
- The smallest resulting fix confined to the queue-list and friction guidance
  in `.agents/skills/next-plan/SKILL.md`, the `try`/`Complete-Listing` path in
  `.agents/skills/next-plan/scripts/Get-NextPlanList.ps1`, and the
  `Get-NextPlanContext`/`Invoke-NextPlanProcess` boundaries in
  `.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1`, including the
  `Get-FinalizeExistingWindowsIdentity`, `Invoke-FinalizeNativeText`,
  `Get-FinalizeGitIdentity`, and `Get-AgentWorktreeSessionContext` functions
  only when the review proves the direct caller contract requires them.

## Out of scope

- The landed Tier-1 cleanup and deleted
  `Documents/Plans/Agents/CodexReviewDiffPathspecLiteral.md` Plan.
- `Tools/WorktreeCli/PlanScheduler.cpp`, WorktreeCli scheduler behavior, and
  AgentTools build/bootstrap coordination; if `/next-plan-review` proves that
  the smallest fix belongs there, surface it for re-planning instead of
  expanding this boundary.
- Unrelated next-plan skills/scripts, claims, landing state, and any transcript
  path or transcript text in the repository.

## Risk tier and invariants

Expected Tier 2 (scoped next-plan tool behavior); escalate if the fix reaches
WorktreeCli scheduler coordination or AgentTools build/bootstrap coordination.
Preserve the read-only listing contract: no claim, scheduler guard, healing,
or storage mutation; one structured result with explicit exit semantics; and
the canonical session/primary identity checks. Never embed transcript paths or
home paths. No determinism/CRC, serialization, replay, wire, runtime
allocation, shader, or live-verification state is exposed.

## Coordination

- `Documents/Plans/Agents/NextPlanClaimStaleWorktreeInvisibility.md` covers a
  different symptom — a Plan present at the primary tip reported absent, with no
  staleness signal, while the session worktree is behind primary — but may edit
  the same `Get-NextPlanList.ps1` listing path and the same `Get-NextPlanContext`
  session/primary identity boundary in `NextPlanWorkflowCommon.psm1`. Whichever
  lands second rebases onto the first and re-reads those regions before editing,
  and the session-versus-primary identity handling ends up stated once rather
  than as two parallel mechanisms.

## Acceptance criteria

- The recorded `Get-NextPlanList.ps1` invocation no longer reproduces exit `1`
  with code/output `git-identity-mismatch` after equivalent final candidate
  preparation/rebase, and a valid session reaches the documented listing
  contract.
- A genuine unresolved identity inconsistency remains an explicit,
  actionable failure rather than being silently treated as a valid inventory.
- `/next-plan-review` records the proven root cause and smallest fix within the
  named boundary; if the root cause is outside it, the issue is surfaced for
  re-planning instead of expanding scope.
- `/validate-skill` runs if any skill file is changed; WorktreeCli `plan
  validate` exits `0` with `status:valid` and `code:ok`.

## Notes

This Plan is keyed to the concrete script-plus-symptom pair
(`.agents/skills/next-plan/scripts/Get-NextPlanList.ps1` returning exit `1`
with code/output `git-identity-mismatch` for the documented invocation after
final candidate preparation/rebase). A later observation of the same pair is
a duplicate, not a new residual. The existing
`Documents/Plans/Agents/NextPlanClaimValidationBusy.md` Plan covers a
different script and symptom and is not a duplicate. The root cause is
intentionally deferred to `/next-plan-review`; this body records the exact
command, result, forced rework, and provenance without embedding transcript
material.
