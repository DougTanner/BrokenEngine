<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T19:36:35.586Z","dependsOn":[]} -->
# Fix: /finalize-changes — a clean `primary.path-overlap` stops for user authorization

## Context
Observed symptom: during `/finalize-changes` preparation,
`pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizePrimaryMovementCheck.ps1`
(canonical arguments from `.agents/skills/finalize-changes/references/scripts.md`
`## Invocation`) exited 2 with `status: blocked`, `code: primary.path-overlap`,
message "Foreign primary movement overlaps an owned path."
(`Invoke-FinalizePrimaryMovementCheck.ps1:340-341`). Foreign commit c50506a5
(ENet checksum trust policy) touched five session-owned paths under
`Projects/BrokenEngineSandbox/Source/` (`Agent/AgentCommandsServer.cpp`,
`Agent/AgentCommandsServerQueries.cpp`, `Agent/AgentCommandsServerQueries.h`,
`Agent/AgentScene.cpp`, `Frame/StatusChange.h`). A read-only
`git merge-tree --write-tree --merge-base=56f543cc df49df9b ac2b1b50` (candidate
parent, live primary, candidate) exited 0 with no conflict, so the two sides
combined without any textual conflict. The preparation worker still returned
"Recovery needs your authorization", which forced a user round-trip before the
ordinary recovery rebase could run.

Where the authorization requirement lives:
- `.agents/skills/finalize-changes/references/worker.md:249-252` — on
  `primary.path-overlap` the worker must "stop and return the result as a
  handoff for the recovery dispatch. When the manager authorizes recovery, that
  worker performs the single ordinary linear rebase ...". Nothing distinguishes
  a clean overlap from a conflicting one, so the manager escalates to the user.
- `.agents/skills/finalize-changes/references/scripts.md:107` — the terminal
  table maps `primary.path-overlap` to "Stop before SmartGit or the landing
  summary and return a blocker."
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizePrimaryMovementCheck.ps1:340-341`
  — every overlap is `blocked`; the script never assesses whether the overlap
  merges cleanly.
- `.agents/skills/finalize-changes/references/worker.md:44-48` — the blocked
  result becomes a recovery handoff to main instead of an in-worker recovery.

User decision (this session, in reply to being asked to authorize that
recovery): "tell it not to do that lmao" — the landing tool must not stop and
ask when someone else's change touches the same files but the edits combine
with no textual conflict.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 18167b15-4d71-4df3-8135-c74d5c9449ff
- Worktree/branch UUID: 1c63fc76-064c-4ceb-be1a-72c69c05376a
- Session branch: claude/1c63fc76-064c-4ceb-be1a-72c69c05376a
- Worktree: .claude\worktrees\BrokenEngine\1c63fc76-064c-4ceb-be1a-72c69c05376a
- Landing ref: claude/1c63fc76-064c-4ceb-be1a-72c69c05376a
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/FinalizeCleanOverlapAutoRebase.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/1c63fc76-064c-4ceb-be1a-72c69c05376a` in bounded
friction mode, supplying client `claude` and the conversation session ID above.
Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

Decided behavior (user decision above, shape fixed by the session's main
agent): on a `primary.path-overlap` whose read-only merge
(`git merge-tree --write-tree --merge-base=<candidate parent> <live primary> <candidate>`)
is clean, the finalizer proceeds with the ordinary recovery rebase already
documented at `worker.md:249-263` and `scripts.md:134-150` (the
`Invoke-FinalizeApprovalPreparation.ps1` bullet) and re-verifies the overlap
regions — the meaning check for places the rebase merged cleanly but changed
meaning, the affected acceptance-table rows (step 3), and `/compile` only when
an overlap region's meaning changed — without asking the user. A textual
conflict, or a failed re-verification, still stops and asks. The single landing
confirmation in `SKILL.md` `### Landing confirmation` stays mandatory and
unchanged; the recovery happens before the landing summary exists, so that
confirmation still binds the rebased diff.

Recommendation (author's, not binding): run the merge-tree assessment inside
`Invoke-FinalizePrimaryMovementCheck.ps1` and report the clean case as its own
code, rather than having the worker run `git merge-tree` itself, because
`scripts.md` states callers never reconstruct the assessment from Git output.
If the result shape or code set changes, bump
`broken-engine-finalize-primary-movement/v2` per the no-backward-compatibility
directive and update every consumer of that schema. Whether the in-worker
recovery runs in the same preparation worker or keeps the fresh recovery
dispatch (`worker.md:44-48`) is left to the fix session; either satisfies the
decision as long as no user question is asked on the clean path.

## Critical files
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizePrimaryMovementCheck.ps1`
- `.agents/skills/finalize-changes/references/scripts.md`
- `.agents/skills/finalize-changes/references/worker.md`
- `.agents/skills/finalize-changes/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to:
  - `Invoke-FinalizePrimaryMovementCheck.ps1`: the overlap branch at lines
    340-341 (and the result construction it feeds) so a clean read-only merge
    of the overlap is distinguishable from a conflicting one
  - `scripts.md`: `### Primary movement check` (schema version and fields, the
    terminal table row at line 107, and the `Invoke-FinalizeApprovalPreparation.ps1`
    bullet at lines 134-150 that describes the `primary.path-overlap` recovery
    rebase)
  - `worker.md`: `## Steps` intro lines 44-48, step 5 (lines 101-118), and the
    `primary.path-overlap` recovery bullet at lines 249-263 — remove the
    "When the manager authorizes recovery" gate for the clean case and keep the
    stop-and-ask for a textual conflict or failed re-verification
  - `SKILL.md` `### Landing confirmation` recovery paragraph only if its wording
    would otherwise contradict the clean-overlap path

## Out of scope
- The landed change the session produced
- The single landing confirmation's requirement and wording of the question
- The `rebase.conflicted` and rewritten-history (`Repair-SessionForkPoint.ps1`)
  recovery paths, `Invoke-FinalizeLanding.ps1`'s internal bounded rebase, and
  the other blocked codes (`primary.not-descendant`, `primary.evidence-truncated`,
  `candidate.*`)
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 2 (trigger: scoped tool behavior — one skill's landing-preparation
behavior). Escalate to Tier 3 if the fix reaches build/bootstrap coordination
or changes what the landing lock or primary-change authorization trusts.
Invariants: the movement check stays read-only (no ref, checkout, index, or
worktree change); no path to primary change bypasses the single landing
confirmation; a textual conflict never auto-resolves without the existing
stop. Never embed transcript paths or home paths.

## Acceptance criteria
- A `primary.path-overlap` whose read-only merge is clean proceeds through the
  ordinary recovery rebase and overlap re-verification to the landing summary
  with no user question before that summary.
- A `primary.path-overlap` with a textual conflict, or a failed re-verification,
  still stops and returns a blocker for the user.
- The recorded symptom no longer reproduces under the documented invocation
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
