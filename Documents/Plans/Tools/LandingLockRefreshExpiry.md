<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T12:36:27.101Z","dependsOn":[]} -->
# Reject refresh of expired landing leases

## Context

The frozen plan-trace survivor `CPT/shard-0061/002` is decision-complete and
pre-existing at `80896f33661aaab99cf180a96db54600099be652`. `HandleRefresh`
accepts a structurally valid lease when the owner matches, writes a new
heartbeat, and extends `expiresAt` without requiring the lease to remain live
(`Tools/WorktreeCli/LandingLockCommands.cpp:170-192`).
`ValidateLandingLease` deliberately accepts an expired timestamp so status and
recovery can classify it (`LandingLockLifecycle.cpp:41-80,100-105`). The
documented recovery path, however, requires expiry and worktree quiescence
before takeover (`Tools/WorktreeCli/LandingLockCommands.cpp:195-213`;
`.agents/skills/finalize-changes/references/scripts.md:185-213`), and the
finalizer treats an expired supplied-token lease as foreign contention
(`.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1:1350-1365`).

The implementation boundary is the owner-only `lock refresh` path and its
lease-state decision; `ValidateLandingLease` status/recovery classification and
the explicit `lock recover` takeover path remain distinct. No engine runtime,
serialization, or Plan scheduler state is involved.

Impact: a stale landing actor can keep primary-advance exclusivity after its
lease should be reclaimable, delaying or racing a new landing actor and
weakening the fail-safe handoff.

## Design

Author's recommendation: require `uiCurrentTicks < lease->uiExpiresTicks` in
`HandleRefresh` before writing a heartbeat. An expired owner token must return
the existing landing conflict result and leave recovery to `lock recover` with
its expiry and registered-worktree checks. Preserve live owner refresh,
original lease duration, compare-and-swap metadata writes, and finalizer
continuity for a live lease.

## Critical files

- `Tools/WorktreeCli/LandingLockCommands.cpp:170-213` — refresh conflict and recovery paths.
- `Tools/WorktreeCli/LandingLockLifecycle.cpp:41-80,100-105` — structural lease validation and expired-state classification.
- `Tools/WorktreeCli/AGENTS.md:9` — lease-expiry ownership contract.
- `.agents/skills/finalize-changes/references/scripts.md:185-213` and `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1:1350-1365` — recovery and finalizer continuity rules.

## In scope

- Live-versus-expired admission in `HandleRefresh` for an owner-matching landing lease.
- Returning an expired-refresh conflict without extending the lease, while preserving `lock recover` as the post-expiry takeover route.
- Keeping live refresh, lease duration, metadata atomicity, and finalizer behavior unchanged.

## Out of scope

- Landing-lock schema or duration changes, recovery/worktree policy, lock stealing, scheduler claims, or unrelated coordination stores.
- User confirmation policy, primary branch movement, generated history overlays, and game/runtime state.
- New compatibility behavior, instrumentation, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the fix changes a repository landing
lease's trust and ownership handoff at a coordination boundary that can block
primary changes and must remain serialized.

Preserve these invariants:

- A live owner can refresh its lease using the original duration and atomic
  metadata write.
- An expired lease cannot be resurrected by `refresh`; only validated expiry
  recovery can transfer ownership after the registered-worktree check.
- Finalizer continuity accepts only a live, identity-matching, sufficiently
  long lease, and no unrelated landing/coordination behavior changes.

## Acceptance criteria

- A structurally valid owner token whose `expiresAt` is at or before now gets
  the existing conflict result from `lock refresh`; its heartbeat and expiry
  remain unchanged.
- `lock recover` can take over the expired lease only under its existing
  expiry and registered-worktree conditions, while a live owner refresh still
  extends by the original duration.
- WorktreeCli Debug/Release builds pass through `/compile`, and focused lock
  fixture scenarios cover live refresh, expired refresh, and recovery.

## Notes

Audit provenance: `CPT/shard-0061/002` in the frozen consolidated index and
its shard report under `Temp/CppPlanTraceAudit/80896f33661aaab99cf180a96db54600099be652/`.
No source or lock state was changed by this routing session.
