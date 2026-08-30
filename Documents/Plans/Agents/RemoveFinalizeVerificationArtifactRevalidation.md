<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T17:35:42.827Z","dependsOn":[]} -->
# Remove finalizer verification-artifact revalidation

## Context

The post-landing review of commit `68d0840d1a0a8b6e692e3d1fcaab27c330ae812c`
found that the landing changed only the deletion of
`Documents/Investigations/Agents/CppPlanTraceAudit.md`. The session spanned
22m03.988s. Deletion implementation took 78.246s, the combined reviewer took
65.943s, finalizer control work took 537.810s, and the final verifier took
209.621s. Control work was 47.7–52.5% of the measured active total. Two
verification-artifact repair loops consumed approximately 292.7 active
agent-seconds.

The current root cause is a split natural-language artifact contract. At
`.agents/skills/finalize-changes/scripts/Show-FinalizeApprovalReview.ps1:42-46,114-185,262-266`,
the finalizer parses `-VerificationPromptFile` and `-VerificationOutFile`,
then `Get-VerificationFile`, `Get-VerificationEvidence`, and
`Assert-Verification` require the exact generated `# (c) Evidence` section,
`Head:`, the reviewer role text, a terminal `PASS`, and output freshness. The
`/verify-changes` contract at `.agents/skills/verify-changes/SKILL.md:86-119,153-168`
instead publishes `Verification: PASS`, a full head binding, and `Residuals`
last. Repairing either side of this prose-to-prose boundary does not buy a
second safety signal.

The approval-review receipt and both advance gates carry the same redundant
proof. `.agents/skills/finalize-changes/references/scripts.md:215-252`
documents the `verification` object and its prompt/output inputs;
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1:1221-1246`
and
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1:125-134`
reparse that object before primary mutation. The workflow already requires a
manager-held `/verify-changes` PASS before SmartGit, the landing summary, and
the confirmation (`.agents/skills/finalize-changes/references/workflow.md:115-160`
and root `AGENTS.md:82-85`).

## Design

The approved outcome is to remove finalizer verification-artifact
revalidation rather than reconcile the two natural-language formats. The
smallest complete implementation is:

1. Remove `-VerificationPromptFile` and `-VerificationOutFile` from
   `Show-FinalizeApprovalReview.ps1`, delete `Get-VerificationFile`,
   `Get-VerificationEvidence`, and `Assert-Verification`, remove the
   `verification` result field, and remove comments and invocations that
   describe those inputs. SmartGit launch remains the existing typed
   approval-review operation.
2. Keep the existing
   `broken-engine-finalize-approval-review/v1` envelope and its
   non-verification fields, including `approvedTip`, launch status, and the
   canonical SmartGit command data. Removing one redundant field does not
   justify a version migration or a legacy parser; all current consumers are
   updated together and no compatibility verification input is retained.
3. Remove `verification` parsing, `approval-review.unverified` checks, and
   prompt/output-specific error text from both advance gates. Keep each
   gate's exact receipt schema/status check, exact `approvedTip` binding,
   attempted-launch requirement, and pre-mutation position.
4. Update the finalize skill, workflow, and scripts reference to remove the
   prompt/output handoff and natural-language revalidation obligations. Keep
   `/verify-changes` mandatory and ordered before SmartGit, the landing
   summary, and explicit confirmation. Keep the verifier's full-head binding,
   SmartGit's `approvedTip` binding, the launch receipt, explicit user
   confirmation, landing lock/CAS, and material-change re-review.
5. Update the existing finalize fixtures and their helper/docs obligations:
   remove verification-file construction and the obsolete prompt-format,
   freshness, and `verification`-field cases; replace them with launch
   coverage that succeeds without those inputs and asserts the receipt has no
   `verification` object. Retain missing receipt, attempted-launch, exact-tip,
   and primary-unchanged failure coverage.

The trust shift is intentional and explicit: landing scripts cease reparsing
reviewer prose and rely on the mandatory manager-held `/verify-changes` PASS
plus the workflow ordering. This Plan does not weaken that manager workflow or
make a direct finalizer invocation an alternate acceptance route.

## Critical files

- `.agents/skills/finalize-changes/SKILL.md:12-48,63-71` — manager-owned
  verifier ordering, SmartGit handoff, and finalizer ownership.
- `.agents/skills/finalize-changes/references/workflow.md:115-160` — the
  verifier-to-SmartGit-to-summary-to-confirmation sequence.
- `.agents/skills/finalize-changes/references/scripts.md:27-32,70-72,215-252,254-261`
  — invocation, receipt, and landing-input documentation.
- `.agents/skills/finalize-changes/scripts/Show-FinalizeApprovalReview.ps1:9-18,42-46,114-185,262-268`
  — obsolete verification inputs, parsers, result field, and launch call.
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1:22-24,1221-1246`
  — postconfirmation receipt gate.
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1:27-30,125-134`
  — primary-commit receipt gate.
- `.agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1:410-430,844-902,1223-1261`
  — receipt helper, launch cases, and both gate cases.

## In scope

- Removing the two verification-file parameters, three verification helper
  functions, their call, the `verification` receipt member, and all
  prompt/output-specific comments and invocation text from the listed
  `Show-FinalizeApprovalReview.ps1` regions.
- Updating both listed receipt gates to consume only the current typed
  approval-review identity/status/launch receipt and exact `approvedTip`,
  with no verification-object branch or compatibility input.
- Updating the listed finalize skill/workflow/scripts reference regions so the
  manager-held `/verify-changes` PASS remains mandatory and precedes
  SmartGit, summary, and confirmation, while full-head, receipt, confirmation,
  lock/CAS, and material-change re-review protections remain documented.
- Removing obsolete verification-artifact fixtures and replacing them with
  direct launch/receipt and gate coverage in the listed fixture regions.
- The smallest applicable `/validate-skill` and
  `/progressive-disclosure-review` evidence for the changed finalize
  instructions, plus the existing finalize fixture command.

## Out of scope

- Any change to `/verify-changes` output, its `Verification: PASS` contract,
  full-head binding, acceptance ownership, or reviewer dispatch.
- SmartGit discovery/launch mechanics, `approvedTip` identity, attempted
  launch statuses, explicit confirmation, landing lock/CAS, rollback,
  material-change re-review, history contracts, primary movement, or claim
  cleanup.
- New receipt schemas, alternate inputs, compatibility aliases, prose-only
  reminders, or any new verifier or finalizer mechanism.
- Source, shader, wire, serialization, replay, `.pack`, CRC, threading,
  allocation, runtime, or build behavior; unit tests; unrelated Plans or
  Investigations; and implementation during this recording stage.

## Risk tier and invariants

Plan-file authoring is current Change Workflow Tier 1 documentation. Executing
this Plan is Tier 3 because removing the finalizer's revalidation changes the
landing trust boundary. The implementation must preserve these invariants:

- The manager still dispatches and receives one fresh `/verify-changes` PASS
  for the final diff before SmartGit, the summary, or confirmation.
- The verifier still binds its result to the complete baseline and full head,
  and both advance routes still bind their launch receipt to the exact
  approved candidate tip and an attempted launch status.
- Explicit confirmation still precedes the landing lease; lock/CAS,
  rollback, material-change re-review, and cleanup remain unchanged.
- No finalizer code reparses natural-language prompt or reviewer-output bytes,
  and no compatibility input preserves the removed artifact contract.

## Acceptance criteria

- Static source and documentation checks show that
  `-VerificationPromptFile`, `-VerificationOutFile`,
  `Get-VerificationFile`, `Get-VerificationEvidence`, `Assert-Verification`,
  and the approval-review receipt's `verification` member/gates are absent
  from the owned regions; no compatibility input or legacy parser remains.
- `Show-FinalizeApprovalReview.ps1` can produce the current typed receipt and
  launch SmartGit without prompt/output paths, and the receipt has no
  `verification` object. Both `Invoke-FinalizeLanding.ps1` and
  `Invoke-FinalizeCandidateCommit.ps1` still require a valid attempted-launch
  receipt whose `approvedTip` exactly equals the commit being advanced.
- The finalize workflow proves `/verify-changes` PASS occurs before SmartGit,
  the landing summary, and explicit confirmation; the verifier's full-head
  binding, lock/CAS, and material-change re-review remain in force.
- Existing finalize fixtures pass after obsolete verification-file and
  prompt-format cases are removed/replaced, including normal launch,
  missing/not-launched/exact-tip receipt failures, primary-unchanged guards,
  rollback, and cleanup coverage.
- `/validate-skill` and `/progressive-disclosure-review` pass for any changed
  instruction files, and the documented finalize fixture suite passes.
- No compatibility input, new schema, new script, or unrelated tracked path is
  introduced.

## Notes

- This Plan has no dependency and is independently executable.
- The current recording stage creates only this executable Plan and does not
  implement the trust-boundary change. Future execution must use the normal
  Tier-3 plan-audit, simplicity, grill, review, verification, and landing
  gates.
