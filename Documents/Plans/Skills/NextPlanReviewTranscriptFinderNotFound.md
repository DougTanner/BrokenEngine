<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-22T12:47:46.743Z","dependsOn":[]} -->
# Fix: /next-plan-review — recorded session lookup can be blocked with transcript.not-found

## Context

During the `/next-plan` claim-exit tooling-friction review, Codex ran this
documented finder invocation (the machine-specific repository-root value is
omitted from this public Plan):

```powershell
$finder = pwsh -NoProfile -File .agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1 -RepositoryRoot '<repository-root>' -Commit 'b18439b489cd2e3ad73d81867f79c06a321679f1' -SessionId '831d5cf0-38db-4fbd-beaf-1e093fb7d715' | ConvertFrom-Json
```

The native exit was captured before processing the parsed result. The finder
returned schema `broken-engine-agent-session-transcript/v2`, `status: blocked`,
code `transcript.not-found`, message `No transcript matched the commit time and
an eligible retained worktree.`, selection mode `explicit-session-id`, no
candidates, and no read errors. The recorded provenance named the Claude
session and its retained worktree, but the exact session ID still produced no
candidate. The `/next-plan-review` contract prohibited broader discovery, so
main stopped with no changes; the user authorized substituting current code
evidence, causing one blocked review attempt and a meaningful acceptance-
evidence deviation/workaround.

This gap is outside the active Plan's `## In scope`: the claimed
`FinalizeChangesOwnerTokenPlaceholder.md` Plan changes only the owner-token
wording in `.agents/skills/finalize-changes/references/scripts.md`. That Plan
itself is not failing.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: codex
- Conversation session ID: none on Codex
- Worktree/branch UUID: 6d17006e-adf5-4d18-b512-d85f9425a6b0
- Session branch: codex/6d17006e-adf5-4d18-b512-d85f9425a6b0
- Worktree: .codex\worktrees\BrokenEngine\6d17006e-adf5-4d18-b512-d85f9425a6b0
- Landing ref: codex/6d17006e-adf5-4d18-b512-d85f9425a6b0
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Skills/NextPlanReviewTranscriptFinderNotFound.md`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design

In a new session, run `/next-plan-review
codex/6d17006e-adf5-4d18-b512-d85f9425a6b0`, supplying the recorded client
`codex`. Root-cause the friction from the proven finder result and current
owner contracts, then make the smallest fix inside the `## In scope` boundary
below. If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files

- `.agents/skills/next-plan-review/SKILL.md` — the Codex/Claude provenance,
  bounded-discovery, and blocked-result contract in `## Prove provenance`.
- `.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1` —
  explicit-session-id discovery, transcript metadata matching, and the
  `broken-engine-agent-session-transcript/v2` result.

## In scope

- Root-cause investigation via `/next-plan-review`, run with client `codex` and
  the landing ref named in `## Design`.
- The smallest resulting fix, confined to the two critical files above, that
  makes the recorded session lookup usable under the documented bounded
  provenance rules without a broader discovery workaround.

## Out of scope

- The landed change produced by the active owner-token Plan.
- Unrelated skills/scripts; any transcript path or transcript text in the repo.
- Removing bounded discovery, path-safety checks, or the untrusted-transcript
  handling contract without explicit re-planning.

## Risk tier and invariants

Expected Tier 2 (scoped transcript-discovery/tool behavior); escalate to Tier 3
only if the resulting fix changes untrusted-transcript or path-safety
trust-boundary semantics. No determinism/CRC, serialization/`.pack`/`kiVersion`, replay,
wire/protocol, affinity, threading, allocation, shader, build, or live-
verification exposure is expected. Never embed transcript paths or home paths.

## Acceptance criteria

- The recorded explicit-session-id invocation no longer reproduces the blocked
  `transcript.not-found` result for the recorded retained session, without a
  broader discovery workaround.
- The documented Codex/Claude provenance route remains bounded and reports an
  actionable result when the recorded session cannot be reached.
- `/validate-skill` passes if `.agents/skills/next-plan-review/SKILL.md` changes;
  plan validate exits 0.

## Coordination

- None. No prerequisite or reciprocal coordination is evidenced for this
  scoped tooling fix.

## Notes

- The originating gap is the blocked finder result during a `/next-plan` run;
  `/next-plan-review` owns future root-cause proof and the resulting fix.
- No dependency is recorded because the active owner-token Plan is outside this
  boundary and does not block the tooling follow-up.
