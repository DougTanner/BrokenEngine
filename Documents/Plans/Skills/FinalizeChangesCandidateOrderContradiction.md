<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-21T12:59:46.962Z","dependsOn":[]} -->
# Fix: /finalize-changes — documented candidate-creation order fails when primary carries a newer history row

## Context

At a `/next-plan` landing gate, the finalizer followed
`.agents/skills/finalize-changes/SKILL.md:88` — "Create the authorized source
landing commit, then squash and rebase it onto the current primary tip" — and the
candidate creation was rejected with code `history.source-changed` before any
candidate existed.

The rejecting guard is `Assert-HistoryTreeUnchanged` in
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1:77`:

```powershell
$changed=@((Invoke-Git $Root @('diff','--name-only',$Base,$Tip,'--',$historyJsonPath,$historySvgPath))...)
if($changed.Count-ne 0){Stop-Candidate 2 'history.source-changed' '...'}
```

The session-landing route calls it as
`Assert-HistoryTreeUnchanged $current $ExpectedPrimaryTip $ExpectedCurrentTip`
(`Invoke-FinalizeCandidateCommit.ps1:123`), so the comparison is
`ExpectedPrimaryTip..ExpectedCurrentTip`. A session branch that simply predates a
generator-owned code-quality history row already on primary therefore shows that
row as a difference and is read as a source patch that touches the
generator-owned JSONL/SVG paths, even though the session changed neither file.
The advance route applies the same guard at `:85`.

The consequence is an ordering contradiction: the skill's step 2 says create the
commit first and rebase afterwards, but the guard only passes when the session
branch already sits on the current primary tip. Workaround: the finalizer rebased
the session branch onto the primary tip first and only then created the
candidate, which succeeded cleanly. Cost: one blocked candidate-creation call
plus the reordering.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: b88a6bb2-6008-407d-b6fb-1669597c61f4
- Worktree/branch UUID: 7d5ecbe4-034b-4ed4-be7a-0df5b811f8f7
- Session branch: claude/7d5ecbe4-034b-4ed4-be7a-0df5b811f8f7
- Worktree: .claude\worktrees\BrokenEngine\7d5ecbe4-034b-4ed4-be7a-0df5b811f8f7
- Landed commit of the observing session:
  a342ffa2dc075356fa1b8e0d169471586e762c92 — this session's own landing, and the
  immutable review ref `/next-plan-review` targets. The `Landing ref` line below
  names the mutable branch only because its tree, unlike this commit's, contains
  this Plan.
- Landing ref: claude/7d5ecbe4-034b-4ed4-be7a-0df5b811f8f7 — the observing
  session recorded this Plan itself, so its own branch tip carries it, and that
  branch tip is the landed commit a342ffa2dc075356fa1b8e0d169471586e762c92 plus
  this Plan.
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Skills/FinalizeChangesCandidateOrderContradiction.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design

In a new session, run
`/next-plan-review a342ffa2dc075356fa1b8e0d169471586e762c92` — the observing
session's landed commit above — supplying the recorded client `claude` and the
recorded conversation session ID `b88a6bb2-6008-407d-b6fb-1669597c61f4`.
Root-cause the friction from the proven transcript, then make the smallest fix
inside the `## In scope` boundary below. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

The decided mechanism is documentation: reword step 2 of
`.agents/skills/finalize-changes/SKILL.md` (lines 88-90) so the documented order
is the one the scripts actually accept — rebase the session branch onto the
current primary tip first, then create the candidate commit — matching
`Assert-HistoryTreeUnchanged`'s `ExpectedPrimaryTip..ExpectedCurrentTip`
contract. The surrounding step-2 statements keep their meaning: reconciliation
still never advances primary, dependency overlap and semantics-changing clean
merges are still inspected, and approval preparation still runs the history
producer's Contract mode afterwards.

The guard is deliberately not the mechanism. It protects a primary-advance
invariant by keeping generator-owned history bytes out of a session patch, so
changing its comparison range or either call site would escalate this work to
Tier 3 and is outside this Plan's boundary.

## Critical files

- `.agents/skills/finalize-changes/SKILL.md` — the sole authorized fix boundary:
  step 2's ordering sentence at lines 88-90.
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1` —
  read-only reference for the contract the wording must match:
  `Assert-HistoryTreeUnchanged` at line 77 and its call sites at lines 85 and
  123.

## In scope

- Root-cause investigation via /next-plan-review, run with client `claude`, the
  review ref named in `## Design`, and the recorded conversation session ID.
- The smallest resulting wording fix, confined to step 2's ordering sentence in
  `.agents/skills/finalize-changes/SKILL.md` as decided in `## Design`.

## Out of scope

- The landed change the session produced.
- `Invoke-FinalizeCandidateCommit.ps1` and every other bundled script — no change
  to `Assert-HistoryTreeUnchanged`, its comparison range, its call sites,
  `Assert-HistoryPathsClean`, `Assert-OwnedNotMixed`, owned-path staging, the
  landing lock, or the primary-advance path.
- Every part of `.agents/skills/finalize-changes/SKILL.md` outside step 2's
  ordering sentence, including the confirmation contract and the claim lifecycle.
- The history contract receipt, its digests, and the landing-time history
  overlay.
- Unrelated skills/scripts; any transcript path or transcript text in the repo.

## Risk tier and invariants

Expected Tier 2, because a reworded workflow step's ordering is skill behavior
rather than documentation. Touching the guard instead would escalate to Tier 3,
since it protects a primary-advance invariant — that is why the guard is out of
scope, not an alternative. Never embed transcript paths or home paths. The
invariant that must survive: no script changes, so a session patch that really
does modify
`.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl`
or its SVG is still rejected with `history.source-changed`.

## Acceptance criteria

- The recorded symptom no longer reproduces: a finalizer following step 2 exactly
  as written rebases the session branch onto the current primary tip before
  creating the candidate, so a session branch that predates a code-quality
  history row on primary and changes neither generator-owned path reaches
  candidate creation without a `history.source-changed` rejection.
- Step 2's other statements are unchanged in meaning: reconciliation never
  advances primary, the overlap and clean-merge inspection remains, and approval
  preparation still runs Contract mode before `/verify-changes`.
- `Invoke-FinalizeCandidateCommit.ps1` is byte-unchanged.
- /validate-skill passes for `.agents/skills/finalize-changes/SKILL.md`; plan
  validate exits 0.
