<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T19:39:38.675Z","dependsOn":[]} -->
# Fix: Get-SessionChangeInventory.ps1 — region cap blocks both C++ reviews on a large session

## Context
Observed symptom: on a 66-file style sweep
(`Documents/Plans/Engine/StyleGuideScannerRuleSweepCommonDataPackerTools.md`),
`pwsh -NoProfile -File .agents/scripts/Get-SessionChangeInventory.ps1 -RepositoryRoot <root> -Baseline <sha> -Regions`
reported `truncated: true` with 437 and then 441 full regions, above
`$script:MaximumRegions = 400` (`.agents/scripts/Get-SessionChangeInventory.ps1:24`,
applied at :808 and :837). The script takes no path filter
(`param` block :7-16), so a caller cannot narrow one run below the cap.
`.agents/scripts/Test-StyleRuleJudgment.ps1:148` then exited 2
`judgment.inventory-truncated`. `/code-style-review` stopped at its worker
step 5 (`.agents/skills/code-style-review/references/worker.md`, "Confirm the
ranges are complete") and `/comment-review` at its worker step 1
(`.agents/skills/comment-review/references/worker.md`, "Only `status` `pass`
with `truncated` false is usable"), each with 0 blocks scanned. Both review
dispatches were wasted. The rerun had to hand-build a caller-supplied scope per
directory from `git diff -U0` for `/code-style-review`, and pass a whole-file
scope, wider than the changed ranges, to `/comment-review`.

Unlike `Find-CommentBlocks.ps1`, whose truncation the comment-review worker
already handles by splitting the scope and rerunning, neither review has a
documented route past a truncated inventory.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 529e7113-822c-42a1-ae08-17fe68b76d80
- Worktree/branch UUID: 485a0f59-4094-4ff4-8816-96f7e08e2de2
- Session branch: claude/485a0f59-4094-4ff4-8816-96f7e08e2de2
- Worktree: .claude/worktrees/BrokenEngine/485a0f59-4094-4ff4-8816-96f7e08e2de2
- Landing ref: claude/485a0f59-4094-4ff4-8816-96f7e08e2de2 (the observing
  session records and lands this Plan itself).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/SessionChangeInventoryRegionCap.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/485a0f59-4094-4ff4-8816-96f7e08e2de2` in bounded
friction mode, supplying client `claude` and the recorded conversation session
ID. Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author recommends one mechanism both reviews share, mirroring the split
route `/comment-review` already documents for `Find-CommentBlocks.ps1`: a
path-prefix filter on `Get-SessionChangeInventory.ps1` so a caller can split
the session into parts that each stay under the cap, with each review worker's
truncation step pointing at that split instead of stopping. Raising the cap
alone is not recommended, because the cap protects the main context from an
oversized result and a larger session would hit the new cap the same way. The
fix session decides after root-causing.

## Critical files
- `.agents/scripts/Get-SessionChangeInventory.ps1`
- `.agents/scripts/Test-StyleRuleJudgment.ps1`
- `.agents/skills/code-style-review/references/worker.md`
- `.agents/skills/comment-review/references/worker.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `Get-SessionChangeInventory.ps1`
  parameters and region emission, the inventory call and truncation check in
  `Test-StyleRuleJudgment.ps1`, `/code-style-review` worker steps 2-6, and
  `/comment-review` worker step 1

## Out of scope
- The landed change the session produced
- The value of `$script:MaximumOutputBytes` and the other caps' purpose of
  bounding main-context size
- `Find-CommentBlocks.ps1` and its existing split route
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 2 (scoped tool behavior); escalate if the fix reaches build/bootstrap
coordination. A truncated inventory must never be treated as complete: a
changed block must not go unreviewed. Never embed transcript paths or home
paths.

## Acceptance criteria
- A session whose full region count exceeds the cap can be reviewed by both
  `/code-style-review` and `/comment-review` over exactly its changed ranges
  through a documented invocation, with no hand-built `git diff` scope
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
