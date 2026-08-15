<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T16:10:16.429Z","dependsOn":[]} -->
# Fix: codex-review repo-code-review dispatch — the metrics digest and the targets file each require the other

## Context
Dispatching `/repo-code-review` through `/codex-review` cannot be completed as
documented. `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1`
blocks with exit `2` and code `prompt.metrics-digest-required` when the
`-ScopeFile` text carries no `broken-engine-code-quality-evidence/v2` digest
(`.agents/skills/codex-review/SKILL.md:88-90`, fixture
`.agents/skills/codex-review/scripts/Test-CodexReviewPromptFixtures.ps1:396-410`),
and it creates neither the prompt nor the targets file in that case.

That digest comes from
`pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1 -Mode Compare -Targets <targets file> -Baseline <sha> -RepositoryRoot <root>`
(`.agents/skills/code-quality-metrics/SKILL.md:33-36`), and
`.agents/skills/repo-code-review/references/metrics-protocol.md:11-16` binds the
digest's `targetSelection` to "the supplied targets file's paths" with per-side
`sha256` identities. But the targets file the dispatch uses is the one the
prompt script itself writes next to the prompt and reports as `targetsPath`
(`.agents/skills/codex-review/SKILL.md:72-76`) — the same script that refuses to
run without the digest. Neither `/codex-review` nor `metrics-protocol.md` names
a producer for the manager's copy.

The manager had to discover
`.agents/scripts/Get-SessionChangeInventory.ps1 -EmitTargets` on its own
(documented only from the reviewer's side in
`.agents/skills/repo-code-review/SKILL.md:25-29` and `:45-48`) and hand-build an
identical targets file to feed `Invoke-CodeQualityMetrics.ps1` before the prompt
script would run — a pre-step no dispatching skill documents.

The claimed active intent was `Documents/Plans/Frame/TerrainTraceToEngine.md`,
whose `## In scope` covered only moving `SegmentHit`,
`TracePointAgainstTerrain`, and `TracePointToFrameExit` and requalifying their
call sites; all of these skill and script files are outside that boundary, so
this is `/next-plan` tooling friction rather than an in-scope failure of the
active change. The exact root cause is intentionally deferred to
`/next-plan-review`.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: ccaa8c39-19a7-4b6c-b2b6-3b6fb1bb0de1
- Worktree/branch UUID: ec7d0a29-fe8c-4d31-b355-17406342ca2e
- Session branch: claude/ec7d0a29-fe8c-4d31-b355-17406342ca2e
- Worktree: .claude\worktrees\BrokenEngine\ec7d0a29-fe8c-4d31-b355-17406342ca2e
- Landing ref: the session branch above, whose tip is the session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to this session alone
  (its diff limited to this session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact conversation session ID above.

## Design
In a new session, run `/next-plan-review <landing ref>` supplying the recorded
client and the recorded conversation session ID; a Codex review supplies the
client and landing ref only. Root-cause the friction from the proven transcript,
then make the smallest fix inside the `## In scope` boundary below: break the
circular ordering in one place, either by documenting the
`Get-SessionChangeInventory.ps1 -EmitTargets` pre-step on the dispatching side
(`/codex-review` or `metrics-protocol.md`) as the producer of the targets file
the metrics run consumes, or by giving the prompt script a digest-less
pre-flight mode that emits the targets file the metrics run then uses. Whichever
side is chosen, the digest must still bind to the exact targets bytes the
dispatched review receives. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

## Critical files
- `.agents/skills/codex-review/SKILL.md` — dispatch evidence requirements
  (`:48-71`) and the receipt/blocked codes (`:72-94`)
- `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1` — the
  `prompt.metrics-digest-required` gate and the `targetsPath` it writes
- `.agents/skills/repo-code-review/references/metrics-protocol.md` — the digest
  and targets-file binding (`:11-16`, `:23`)

## In scope
- Root-cause investigation via /next-plan-review, run with the recorded landing
  ref and client, plus, on Claude, the recorded conversation session ID; a Codex
  review supplies the client and landing ref only
- The smallest resulting fix, confined to the files named above: the documented
  pre-step, or a digest-less pre-flight targets emission, plus its blocked-code
  and receipt documentation

## Out of scope
- The landed change the session produced
- `.agents/scripts/Get-SessionChangeInventory.ps1` and
  `.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1`
  behavior, JSON shapes, and exit mappings
- The metric contract, thresholds, and what the review does with the digest
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. A dispatch whose scope file genuinely lacks a
digest must still be blocked, and the digest must stay bound to the exact
targets bytes the reviewer receives. Never embed transcript paths or home paths.

## Acceptance criteria
- A `repo-code-review` dispatch can be completed end to end by following the
  documented steps in order, with no undocumented script discovery and no
  hand-built duplicate targets file
- A scope file with no `broken-engine-code-quality-evidence/v2` digest still
  blocks with `prompt.metrics-digest-required` and writes no prompt
- /validate-skill passes for any changed SKILL.md; plan validate exits 0

## Notes
This Plan is keyed to the pair (`/codex-review` `repo-code-review` dispatch,
`prompt.metrics-digest-required` reachable only by hand-building the targets
file the same script would have produced). A later observation of the same pair
is a duplicate, not a new residual. This body records the observed blocking
order and the forced workaround without embedding transcript material.
