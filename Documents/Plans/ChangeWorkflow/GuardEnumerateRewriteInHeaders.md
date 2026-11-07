<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T18:23:57.642Z","dependsOn":[]} -->
# Fix: /code-style-review — rule 8 auto-fix rewrites a header counter loop to std::views::enumerate, which MSVC rejects

## Context
Observed symptom, from one `/next-plan` run whose claimed Plan changed
DataPacker scene export:

- A `mechanic` running `/code-style-review` over the session's changed C++
  hand-read rule 8 (`.agents/skills/code-style-review/references/worker.md`
  `## Steps` step 6) and applied, as a meaning-preserving auto-fix (step 10),
  a rewrite of the counter-initializer loop in the inline header function
  `SourceMaterialNodeInfos` (`DataPacker/Source/ExportJobs/Scene/SceneExportData.h`)
  to `std::views::enumerate`.
- The following DataPacker Release build failed: MSVC reported
  `error C2872: 'iter_move': ambiguous symbol` inside `<algorithm>`
  (the `std::ranges` algorithm bodies), followed by C3861 and C2672 naming
  `std::ranges::enumerate_view<...>::_Iterator<true>::iter_move`, in
  translation units that include the header and call a `std::ranges`
  algorithm.
- The run reverted the loop to the counter-initializer form with a comment
  naming the hazard, at the cost of a failed build, a `/resolve-findings` fix
  round, a style re-check, a rebuild, and a `/comment-review` fix round.
- The same hazard was already documented before this run, in
  `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.h:138`
  (`NavigationGraphIndex`), but neither `Documents/C++StyleGuide.txt` rule 8
  — which recommends `std::views::enumerate` over a counter initializer
  statement — nor the `/code-style-review` worker mentions it, so the worker
  had nothing to reject the rewrite against.

Root cause is not confirmed; the unguarded rewrite is the observed symptom.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: f8d3b92d-a9e2-4bf3-bd98-6a05a1cf0039
- Worktree/branch UUID: 63a744e9-264f-4640-9d1d-449723b1fe0b
- Session branch: claude/63a744e9-264f-4640-9d1d-449723b1fe0b
- Worktree: .claude\worktrees\BrokenEngine\63a744e9-264f-4640-9d1d-449723b1fe0b
- Landing ref: claude/63a744e9-264f-4640-9d1d-449723b1fe0b, whose tip is the
  observing session's final commit and which survives exactly as long as the
  worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the
recorded client and the recorded conversation session ID. Then make the
smallest fix inside the `## In scope` boundary below. If root-causing shows the
fix lies outside that boundary, surface it for re-planning instead of expanding
scope.

The author recommends, pending that root cause, stating the MSVC hazard once in
`Documents/C++StyleGuide.txt` rule 8 as a case where the counter initializer
statement is kept (a loop in a header whose including translation units call a
`std::ranges` algorithm), because the worker adjudicates every hand-read rule
against the guide and would then reject the rewrite; and checking whether the
worker's step 6, 9, or 10 wording led it to treat the guide's "recommended"
form as a finding when the counter form "remains allowed".

## Critical files
- `Documents/C++StyleGuide.txt`
- `.agents/skills/code-style-review/references/worker.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `Documents/C++StyleGuide.txt` rule 8
  and `.agents/skills/code-style-review/references/worker.md` `## Steps`
  steps 6, 9, and 10

## Out of scope
- The landed change the session produced, including the hazard comments in
  `SceneExportData.h` and `TerrainUtils.h`
- Rewriting existing `std::views::enumerate` or counter loops elsewhere
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 2, trigger: scoped tool behavior of one subsystem, what `/code-style-review`
auto-fixes (`.agents/references/risk-tiers.md` `### Risk tiers`). Escalate if
the fix reaches build/bootstrap coordination. Never embed transcript paths or
home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces: `/code-style-review` over a header
  counter-initializer loop like `SourceMaterialNodeInfos` leaves it unchanged
  rather than rewriting it to `std::views::enumerate`
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
