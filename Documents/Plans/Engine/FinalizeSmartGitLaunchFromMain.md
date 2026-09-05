<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T17:36:07.303Z","dependsOn":[]} -->
# Move the SmartGit approval-review launch back to the main session

## Context

`/finalize-changes` currently has the dispatched finalizer worker open the
SmartGit approval-review window itself. `SKILL.md` `## Inputs` (the paragraph
beginning "One dispatch covers everything before the confirmation") says the
worker "runs the SmartGit approval review and returns the completed table, that
review's status, and the landing summary in a single handoff. Main presents that
summary and asks the confirmation below, with no tool call in between." The same
contract appears in the `SmartGit review:` handoff row in `## Handoff`, in
`### Landing confirmation` ("copied from the handoff's `SmartGit review` row"),
and in `references/worker.md` step 6 "Run the SmartGit approval review", step 7,
and step 8. `references/scripts.md` `## Invocation` documents the command as
`Show-FinalizeApprovalReview.ps1 -PrimaryWorktree '<primary-worktree>'
-ApprovedTip '<landing-commit>' -LaunchSmartGit >
'Temp/finalize-approval-review-result.json'`, and
`Invoke-FinalizeLanding.ps1` consumes that receipt file through
`-ApprovalReviewResultFile` as a required landing input.

Observed cost, in both landings of the session that recorded this Plan: the
worker opened the SmartGit window and then spent the rest of its turn writing
the acceptance table and the landing summary. The user finished reviewing the
diff in SmartGit and then had to wait for the worker's turn to end before the
main session could present the summary and ask the landing confirmation
question.

Authority. The user gave a direct instruction in that session: "We just changed
that so the main session launches it, so that as soon as I have reviewed
SmartGit the main session is idle and I can confirm right away." Under the root
`AGENTS.md` authority order an explicit user statement outranks the current
code and the current skill text, so this Plan is authorized to reverse the
current contract.

Contradiction to state plainly. Commit `fe5a6e3b` ("Remove three pieces of
landing ceremony from /finalize-changes") is what moved the launch from main
into the worker, with the stated rationale "so main makes no tool call between
its handoff and the confirmation question". That rationale optimized for zero
main-session tool calls between handoff and question. The user instead wants the
SmartGit window and the confirmation question to become available at the same
moment, which requires main to be the one that opens the window. This Plan
supersedes `fe5a6e3b`'s rationale on that one point; nothing else that commit
removed (the pre-confirmation reconciliation lease rules, the entry-time whole
`scripts.md` load) is revisited here.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: 552a615b-9166-4c5b-8991-7e9c279d2dcf
- Worktree/branch UUID: def6cf77-44da-402b-b5e4-a597e25a7971
- Session branch: claude/def6cf77-44da-402b-b5e4-a597e25a7971
- Worktree: .claude\worktrees\BrokenEngine\def6cf77-44da-402b-b5e4-a597e25a7971
- Landing ref: claude/def6cf77-44da-402b-b5e4-a597e25a7971 (the session branch
  that carries this Plan). The two landings whose confirmations exhibited the
  delay are `60d78d125a977df1aa5f7e6a36a5ffe3cb21be84` and
  `5829aab9c191d0f8678dae13422237ce20793252`; neither contains this Plan.

## Design

Restore the pre-`fe5a6e3b` division of labour, which is the recommended shape
because it is a known-good contract already exercised in this repository rather
than a new mechanism:

1. `references/worker.md` step 6 becomes "Fill in the SmartGit launch line"
   again: the worker takes the `Show-FinalizeApprovalReview.ps1` command from
   `references/scripts.md` `## Invocation`, redirect included, fills in
   `<primary-worktree>` and the prepared landing commit as `<landing-commit>`,
   and does not run it. Step 7 refills that line for a newly reviewed landing
   commit instead of re-running the review. Step 8's handoff row carries the
   filled line.
2. `SKILL.md` `## Handoff` replaces the `SmartGit review:` row with a
   `SmartGit launch:` row carrying that filled command line.
3. `SKILL.md` `### Landing confirmation` states that main first runs that line
   verbatim under the root `AGENTS.md` bundled-script rule, then reads `status`,
   `code`, `message`, and `manualCommand` from the receipt the redirect writes,
   and only then presents the summary and asks the confirmation — with the
   `**SmartGit review:** ...` line filled from the fields just read, and no
   other tool call in between. An unreadable receipt, or one whose status is
   outside the set `references/scripts.md#approval-review-receipt` accepts,
   stays a blocker to report rather than a summary to present.
4. `SKILL.md` `## Inputs` and the refreshed-confirmation paragraph are reworded
   to match, including the "A stale-base result loops back to main before any
   launch line is returned" sense.
5. `references/worker.md` `## Bundled scripts` restores the sentence naming
   `Show-FinalizeApprovalReview.ps1` as the one script the worker fills in and
   returns for main to run verbatim instead of invoking itself, so the
   never-wrap rule is not read as forbidding the handoff of a filled line.

Reading `git show fe5a6e3b -- .agents/skills/finalize-changes/SKILL.md
.agents/skills/finalize-changes/references/worker.md` gives the exact prior
wording; prefer reusing it over inventing new phrasing, adapting only where the
surrounding text has since changed.

Recommended non-changes: `Show-FinalizeApprovalReview.ps1` itself, the receipt
schema, `references/scripts.md` `## Invocation`, and
`Invoke-FinalizeLanding.ps1`'s `-ApprovalReviewResultFile` consumption all stay
byte-identical. Only who runs the documented command moves; the receipt path and
its landing gate are unchanged, so no script edit is required. If the
implementer finds a script edit is required, that is a signal to re-plan rather
than to expand scope.

Rejected alternative: leaving the launch in the worker and merely shortening the
worker's post-launch work (for example by ordering the acceptance table before
the launch). It does not satisfy the user's instruction, because the window
still opens inside a worker turn that main cannot end.

## Critical files

- `.agents/skills/finalize-changes/SKILL.md` — `## Inputs` (the
  "One dispatch covers everything before the confirmation" paragraph), the
  `SmartGit review:` row in `## Handoff`, and `### Landing confirmation`
  including its refreshed-confirmation paragraph.
- `.agents/skills/finalize-changes/references/worker.md` — `## Bundled
  scripts`, step 6, step 7, step 8, and the step-10 sentence naming who wrote
  the receipt.
- `.agents/skills/finalize-changes/references/scripts.md` — `## Invocation` and
  the approval-review receipt section, read as the unchanged command and receipt
  contract.
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1` — the
  `-ApprovalReviewResultFile` receipt gate, read as the unchanged landing input.

## In scope

- Moving the `Show-FinalizeApprovalReview.ps1 -LaunchSmartGit` invocation from
  the worker to the main session, in the `SKILL.md` and `references/worker.md`
  sections named above.
- The handoff row rename from `SmartGit review:` to `SmartGit launch:` and the
  receipt-field reading rules that move with it.
- Recording under `## Context` of the new Plan's own change that `fe5a6e3b`'s
  zero-main-tool-call rationale is superseded by the user's instruction.

## Out of scope

- Any edit to `Show-FinalizeApprovalReview.ps1`, to the receipt schema, or to
  `Invoke-FinalizeLanding.ps1`'s receipt consumption.
- The other two removals `fe5a6e3b` made: the pre-confirmation reconciliation
  lease rules and the entry-time whole-`scripts.md` load.
- The landing confirmation question's own wording, the acceptance table, and the
  landing summary contents.
- Any other skill, and any change to the landing lock or lease sequencing.

## Risk tier and invariants

Expected Tier 2 under the root `AGENTS.md` risk triggers: scoped behavior of one
tool's workflow — the `/finalize-changes` skill's dispatch and handoff contract
— with no determinism/CRC, wire, serialization, replay, threading, or trust
boundary exposure, and no build/bootstrap coordination. Escalate to Tier 3 only
if the implementation turns out to need a script or landing-gate change.

Invariants that must survive: exactly one explicit user confirmation authorizes
changing primary; no tool call may come after the confirmation question in
main's message; the SmartGit receipt that `Invoke-FinalizeLanding.ps1` consumes
must describe the same commit the presented summary describes; a receipt with an
unaccepted or unreadable status remains a blocker.

## Acceptance criteria

- `SKILL.md` and `references/worker.md` agree that main runs the launch line and
  the worker returns it filled in, with no remaining text saying the worker runs
  the review.
- `references/scripts.md` and the two scripts named above are unchanged.
- `/validate-skill` passes for `.agents/skills/finalize-changes`, and
  `/progressive-disclosure-review` is run because instruction prose changed.
- The next landing that uses the revised skill opens SmartGit and asks the
  confirmation question in the same main-session message exchange.

## Notes

The just-landed Plans `Documents/Plans/Engine/FinalizeOwnedPathStagedThenModified.md`,
`Documents/Plans/Engine/FinalizeVerifiedCandidateRebaseReResolution.md`, and
`Documents/Plans/Engine/FinalizeVerifiedCandidateWorktreeIdentity.md` also touch
`/finalize-changes`, but their boundaries are the candidate-commit guard, the
approval-preparation recovery wording, and the verified-candidate identity
assertions respectively. None of them owns the SmartGit launch division of
labour, so no `dependsOn` edge and no reciprocal `## Coordination` section is
warranted.
