# Explicit Primary Commit

Load only for an explicitly requested non-session commit on the primary
checkout. Require primary checkout identity and a clean authorized scope.
`../scripts/Invoke-FinalizeCandidateCommit.ps1 -Route primary-commit` uses a temporary
index to build the source candidate without moving the primary ref or real index.

The candidate is
uncommitted by design, so a reviewed path the landing inventory reports dirty
whose bytes match the reviewed candidate tree is expected state and classifies as
`intentionally persisted`.

After acceptance binds to that diff, invoke
`../scripts/Show-FinalizeApprovalReview.ps1 -LaunchSmartGit` exactly as
[`references/scripts.md`](scripts.md#invocation) shows, then present the primary
summary and the authoritative confirmation from the skill's `## Landing confirmation`
section. Only its affirmative response permits
claiming the normal 3600-second landing lease and resuming the same candidate
script with `-AdvancePrimary`, `-OwnerToken`, `-SessionLabel`,
and `-ApprovalReviewResultFile`. The receipt is mandatory on this route and the
candidate script checks it before `Invoke-PrimaryHistoryAdvance`, under the
[approval review receipt](scripts.md#approval-review-receipt) gate.
The direct-primary route never performs omitted-token implicit mutation.
Under that lease it follows the direct-primary
[primary movement](scripts.md#primary-movement-check) and
[landing](scripts.md#landing-and-recovery) contracts in
[`references/scripts.md`](scripts.md) and the [root `AGENTS.md` Step 8 landing
invariant](../../../../AGENTS.md). The direct-primary route
advances by guarded CAS with guarded rollback on postcondition failure.
When the commit changes
`Documents/Plans/**`, run WorktreeCli `plan validate` afterwards.
