<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-21T13:04:09.132Z","dependsOn":[]} -->
# Fix: /finalize-changes — a stale session base is reported as the session changing history files

## Context

During `/finalize-changes` step 2 (session landing of
`Documents/Plans/Common/RemoveUnusedLaunchExecutable.md`), the documented
invocation of
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1`
with `-Route session-landing` returned exit `2`, `status: blocked`, code
`history.source-changed`, message "The candidate source patch changes
generator-owned history JSONL/SVG paths."

Nothing in that session touched any history path. The real situation was that
primary had advanced to commit `c847a2766ff71e0ffca7ad91b2fa7daa945feed3` — a
code-quality history-overlay commit touching only
`.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl`,
the sibling `.svg`, and a new Plan document — while the session tip still
predated it. The guard is
`Invoke-FinalizeCandidateCommit.ps1:123`, which calls
`Assert-HistoryTreeUnchanged $current $ExpectedPrimaryTip $ExpectedCurrentTip`;
that helper at `Invoke-FinalizeCandidateCommit.ps1:77` diffs the history JSONL
and SVG paths between the primary tip and the session tip, so a session whose
base predates any history-overlay commit shows those paths as differing even
though the session never wrote them.

Rework forced: the worker rebased the session branch onto the primary tip with
`git rebase --autostash` and re-ran candidate creation, which then passed
cleanly. The message misdirects diagnosis (it names history paths and blames
the session's own patch rather than a stale base), and every session whose base
predates a history-overlay landing hits the same block and the same manual
rebase-and-retry, although `.agents/skills/finalize-changes/SKILL.md:88-89`
already orders a rebase onto the current primary tip as part of the same step.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: e994fbb2-cb6b-4492-9520-8d06fff45780
- Worktree/branch UUID: 2f89421f-fe17-4b1c-8639-482b99ca9470
- Session branch: claude/2f89421f-fe17-4b1c-8639-482b99ca9470
- Worktree: .claude\worktrees\BrokenEngine\2f89421f-fe17-4b1c-8639-482b99ca9470
- Landing ref: claude/2f89421f-fe17-4b1c-8639-482b99ca9470
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Skills/FinalizeCandidateStaleBaseHistoryBlock.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design

In a new session, run `/next-plan-review claude/2f89421f-fe17-4b1c-8639-482b99ca9470`,
supplying the recorded client (claude) and the recorded conversation session ID
above. Root-cause the friction from the proven transcript, then make the
smallest fix inside the `## In scope` boundary below, which is a self-describing
block and nothing more: when `Assert-HistoryTreeUnchanged` trips only because the
session base predates a primary history-overlay commit, the blocked result must
name that actual cause — the stale session base, the primary tip carrying the
history-overlay commit — and direct the rebase onto the primary tip that step 2
already orders. The block still blocks: candidate creation never rebases by
itself, gains no new recovery machinery, and a session patch that genuinely
writes the reserved history paths keeps its current blocked meaning. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files

- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1` —
  `Assert-HistoryTreeUnchanged` (line 77) and its `session-landing` call site
  (line 123).
- `.agents/skills/finalize-changes/references/scripts.md` — the documented
  result codes and invocation for that script.
- `.agents/skills/finalize-changes/SKILL.md` — step 2, which already orders the
  rebase onto the current primary tip.

These three files are the authorized fix boundary.

## In scope

- Root-cause investigation via /next-plan-review, run with the recorded client
  and the review ref named in `## Design`, plus the recorded conversation
  session ID.
- The smallest resulting fix, confined to the files named above: the blocked
  result's message text and only the stale-base detection needed to produce it,
  in `Assert-HistoryTreeUnchanged` and its `session-landing` call site, plus the
  matching wording in `references/scripts.md` and `SKILL.md` step 2.

## Out of scope

- The landed change the session produced.
- The `primary-commit` route, `Invoke-FinalizeLanding.ps1`, the landing lock,
  and the history producer under
  `.agents/skills/code-quality-metrics/scripts/`.
- New recovery machinery: no new script, new result field beyond what the fix
  requires, or new retry mechanism.
- Automatic reconciliation: candidate creation must not rebase, reset, or
  otherwise move the session branch on its own.
- Unrelated skills/scripts; any transcript path or transcript text in the repo.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior: what the session-landing candidate route
blocks on and reports); escalate if the fix reaches build/bootstrap
coordination or the primary-advance path. The invariant that must survive: a
session may never land bytes on the generator-owned history JSONL/SVG paths,
so any relaxation must still block a session patch that genuinely writes them.
Never embed transcript paths or home paths.

## Acceptance criteria

- A session whose base predates a history-overlay commit and whose patch does
  not touch history paths is still blocked, but the blocked result no longer
  blames its own patch: it names the stale session base against the primary tip
  and directs the rebase onto that tip.
- Candidate creation performs no rebase or other automatic reconciliation.
- A session patch that does change either reserved history path is still
  blocked with its current meaning.
- /validate-skill passes for `.agents/skills/finalize-changes/SKILL.md`; plan
  validate exits 0.
