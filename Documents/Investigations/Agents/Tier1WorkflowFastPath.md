# Tier 1 workflow fast path

Status: Open investigation; no implementation decision has been made.

Area: Agents / Change Workflow

Record type: Non-executable reference material. This document intentionally
has no scheduler metadata and is not a Plan input.

## Context

The post-landing review of commit
`68d0840d1a0a8b6e692e3d1fcaab27c330ae812c` found only the deletion of
`Documents/Investigations/Agents/CppPlanTraceAudit.md`. The measured session
was 22m03.988s: deletion implementation 78.246s, combined reviewer 65.943s,
finalizer control work 537.810s, and final verifier 209.621s. Control work was
47.7–52.5% of active agent-time. A narrow executable Plan separately records
the deletion-only Investigation fast path; this open record asks whether
other Tier-1 classes can remove more ceremony without weakening a unique
safety signal.

The current workflow still has distinct gates for manager-owned edits, the
Tier-1 combined review, candidate preparation, the history Contract, the
finalizer agent, the final verifier, SmartGit, explicit confirmation, the
landing lock/CAS, and cleanup. The investigation must test each gate against
the measured baseline and the current owning contracts rather than assuming
that every control is either mandatory or redundant.

The fixed priority is removal first, consolidation/determinization second,
and a new rule last. This record authorizes no workflow change.

## Gate inventory

The following is a decision ledger, not a selection. “Expected saving” is
bounded by the measured interval that a candidate could remove; it is not a
claim that a future route will save the entire aggregate. A future review must
separate active agent-time from wall time and record overlap, unique signal,
and recovery evidence for each row.

| Gate | Unique safety signal today | Removal candidate | Deterministic substitute if needed | Expected saved agent-time / wall-time | Failure and recovery effect |
| --- | --- | --- | --- | --- | --- |
| Manager-owned edit/implementation dispatch | Establishes an accountable worker and an implementation handoff for work main should not perform. | Remove for a proven manager-owned Tier-1 class whose Git diff is deterministic and bounded, extending the narrow deletion-only evidence only after review. | Existing Git inventory plus the approved execution card and final candidate identity. | Up to the measured 78.246s implementation interval on this baseline; wall saving depends on whether the dispatch was on the critical path. | Main owns errors and must block on an inconclusive class; no worker capsule exists for recovery, so candidate state must remain inspectable. |
| Tier-1 combined review | Independent coherence/acceptance signal before landing for non-C++ Tier-1 changes. | Remove when one later final verifier proves the same final-diff authorization and no propagation or hygiene trigger; retain for broader or uncertain classes. | One fresh Step-8 verifier with inventory, history/reference search, and a full-head binding. | The measured combined-review interval is 65.943s active and the corresponding wall interval if non-overlapping. | A missed eligibility condition would remove an independent signal; normal-workflow fallback and verifier block must be deterministic. |
| Candidate preparation | Freezes the candidate commit/tree/parent and reconciles the owned diff before review/landing. | Remove only duplicated preparation passes for classes whose existing candidate script already derives the same identities once. | Existing candidate/landing state machine and Git-derived candidate receipt. | A subset of the 537.810s finalizer-control interval; isolate it before claiming wall savings. | Removing it could expose an unreviewed or stale candidate; deterministic identity mismatch must block and return to preparation. |
| History Contract | Proves history ownership and controls the only generated landing overlay. | Remove only when the diff and route prove carry-forward/no generated history; never for a C++ or history-affecting class. | Existing path classifier and carry-forward contract, with an explicit no-overlay result. | No per-gate interval is isolated in this baseline; expected saving is the measured history portion of finalizer control, otherwise zero. | A false no-overlay classification could publish incorrect history; any uncertainty must retain the Contract path and block on mismatch. |
| Finalizer agent | Performs serialized candidate/rebase/summary/landing orchestration and returns recovery state. | Consolidate duplicated manager/finalizer control work only where one existing state owner can resume the exact operation. | Existing finalize scripts/state transitions with one owner and typed receipts. | At most the 537.810s control interval, but only the proven duplicate portion is removable; measure before changing it. | Removing a resumable worker can make crashes harder to recover; preserve lease expiry, rollback, and explicit terminal states. |
| Final verifier | Independent acceptance of the final prepared diff and every approved criterion/invariant. | No broad removal candidate; test only whether a typed machine result can replace duplicate pre-review checks while retaining one final verifier. | Existing `/verify-changes` acceptance table, inventory, and full-head binding; any replacement must prove equivalent signal. | Up to 209.621s active if removed, but unique safety signal and landing trust make this a high-risk candidate with no approval to remove. | Failure would allow an unauthorized or unreviewed diff to land; retain block/review recovery unless equivalent evidence is proven. |
| SmartGit review | Human-facing review of the exact primary anchor and launch attempt. | Remove only if the user-facing signal is already supplied by an existing confirmation/receipt and the loss is explicitly accepted; no evidence currently selects this. | Exact `approvedTip` launch receipt and existing confirmation, if equivalence is proven. | No isolated interval is supplied; expected saving is unverified rather than invented. | Removing the window changes human review behavior; recovery would need an explicit user-visible alternative. |
| Explicit confirmation | User authority immediately before primary mutation. | No removal candidate under the current landing contract; test whether any narrower user-directed route is already authorized elsewhere. | None without changing authority semantics. | No removal saving is assumed; any estimate requires direct timing evidence. | Removing it would change who authorizes the landing; keep the existing wait/confirmation recovery. |
| Landing lock/CAS | Excludes competing primary mutation and detects stale expected tips atomically. | No removal candidate; only consolidate duplicate lock claims or status reads. | Existing lease, compare-and-swap, rollback, and expiry recovery. | No removal saving is assumed; lock waits are not active agent-time unless a cited action occurs. | Removing it risks concurrent or wrong-tip mutation; lock contention/expiry must remain recoverable. |
| Cleanup | Releases claims/locks and proves worktree/session residue is handled after landing or failure. | Consolidate repeated cleanup checks under the existing finalizer owner; do not remove recovery cleanup. | One typed terminal cleanup state with existing re-query/expiry behavior. | Unmeasured subset of finalizer control; report zero until isolated. | Skipping cleanup leaks claims or worktrees; terminal failure must retain actionable residue and retry/recovery. |

## Eligibility and owner choices

The investigation must compare deterministic eligibility for wider Tier-1
classes: status/path/mode, instruction and skill ownership, generated-output
triggers, Plan/Feature boundaries, symlinks/gitlinks, mixed diffs, unresolved
decisions/findings, and whether the candidate can be bound to one final head.
An uncertain classification must choose the normal route rather than a
best-effort bypass.

Two existing ownership shapes should be compared before proposing anything
new:

1. Extend the existing Git-derived inventory/classifier so it emits one
   eligibility result consumed by the root workflow and finalizer. This
   consolidates classification and keeps the result deterministic, but the
   inventory must not silently become a primary-mutation owner.
2. Keep inventory read-only and put the eligibility/state transition in the
   existing finalize workflow/state owner, which can bind candidate, verifier,
   confirmation, lock/CAS, and cleanup in one sequence. This keeps mutation
   ownership centralized, but it must not duplicate path classification.

A new router script or host plugin is an additional option only if both
existing owners cannot express the state safely. Compare every option on
invalid-state prevention, latency/tokens, failure recovery, and added
ceremony. Removal or consolidation wins ties; a prose-only rule is not a
deterministic owner.

## Critical files

- `AGENTS.md:78-85` — all Change Workflow gates and landing invariants under
  consideration.
- `.agents/scripts/Get-SessionChangeInventory.ps1` — current status/path/mode
  and trigger inventory boundary.
- `.agents/references/tier1-combined-review.md` — existing Tier-1
  combined-review signal and handoff shape.
- `.agents/skills/finalize-changes/SKILL.md:12-48,63-71` and
  `.agents/skills/finalize-changes/references/workflow.md:70-160` — candidate,
  history, finalizer, SmartGit, verifier, confirmation, and landing sequence.
- `.agents/skills/verify-changes/SKILL.md:43-168` — final-diff verification,
  acceptance table, full-head binding, and landing gate.
- `.agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1`
  — existing candidate, history, lock/CAS, landing, and cleanup evidence.

## Decisive questions and acceptance evidence

- For each gate, what unique safety signal is not already supplied by the
  final candidate, one final verifier, explicit confirmation, and lock/CAS?
  What concrete evidence proves the signal is duplicate, and what measured
  active-time and wall-time interval would removal save?
- Which exact Tier-1 classes beyond ordinary Investigation deletions can be
  classified deterministically, and which negative cases must remain on the
  normal route? Does the answer cover modes, mixed statuses, references,
  generated outputs, instruction/skill files, Plans/Features, and unresolved
  decisions/findings?
- Is the existing inventory or the existing finalizer state owner the single
  eligibility/state-machine owner? What exact inputs, output/status, candidate
  binding, recovery states, and no-best-effort failure behavior prove it?
- Can candidate preparation, history Contract, finalizer control, SmartGit,
  confirmation, lock/CAS, and cleanup be consolidated without removing the
  final verifier or user authority? If not, what unique failure/recovery signal
  requires each retained gate?
- What exact future Plan changes which root workflow/skill/state-owner regions,
  and what Tier-3 trust/landing boundary and acceptance scenario will prove
  direct-main, verifier, confirmation, lock/CAS, and recovery behavior?

The investigation is complete only when it provides a measured gate ledger,
deterministic eligible/ineligible matrix, one selected existing owner or a
justified new owner, exact interface/state decisions, and a Tier-3 Plan
boundary. It authorizes no implementation, script, configuration, schema,
claim, or landing change.

## Risk tier and invariants

This record is non-executable and remains outside scheduler inventory. Any
future implementation is Tier 3 because it changes manager/worker/reviewer
routing and landing controls. The current narrow deletion-only Plan remains
the boundary for that class; this investigation must not silently broaden it.

Future evidence must preserve exact final-diff/full-head binding, independent
verification where retained, explicit user authority, lock/CAS atomicity,
rollback, cleanup, and normal-workflow fallback for every uncertain case. No
gate may be removed merely because it did not fire once, and no new ceremony
may be added without showing why removal/consolidation cannot carry the same
signal.

## Notes

The timing figures are the measured session baseline, not a promise of future
savings. This record is intentionally open so a later executable Plan can
choose the exact eligible class and trust changes after comparing all gates on
the same evidence.
