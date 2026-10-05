<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-05T00:10:30.654Z","dependsOn":[]} -->
# Fix: /next-plan step 6 — final claim refresh lists every changed path into main

## Context
Observed symptom (context-efficiency envelope, `/next-plan` run checkpoint):
the final claim refresh in `.agents/skills/next-plan/SKILL.md` `## Steps`
step 6 returned a `sync` object
(`e301deaa179e472d76cdc0e7e4ca8a7df61ceed7..34756e84a7808ddbea17535ebc7f0989772e311d`).
As step 6 directs ("main lists, as names only with no diff content, the paths
`sync.from..sync.to` changed and keeps those the preparation handoff cited"),
main ran `git diff --name-only <sync.from> <sync.to>`. The result was 170 paths,
10,282 characters, all read into the main session, although step 6 keeps only
the paths the preparation handoff cited. Main then ran a separate Grep to
intersect that listing with the cited paths.

- Tool: Bash (`git diff --name-only`)
- Invocation: `git diff --name-only <sync.from> <sync.to>`, unrestricted by pathspec
- Measured size: 170 paths / 10,282 characters
- Checkpoint: `/next-plan-checkpoint-review` at this run's `/next-plan` step 9

The listing grows with every commit landed on primary between preparation and
the refresh. Step 6 needs only the intersecting paths, plus whether any exist.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 683f381b-dffb-4ffc-a54f-57b048d3df45
- Worktree/branch UUID: 5cd206cc-bf76-4686-b771-fe675c9c2837
- Session branch: claude/5cd206cc-bf76-4686-b771-fe675c9c2837
- Worktree: .claude\worktrees\BrokenEngine\5cd206cc-bf76-4686-b771-fe675c9c2837
- Landing ref: claude/5cd206cc-bf76-4686-b771-fe675c9c2837, whose tip is the
  observing session's final commit and which survives exactly as long as the
  worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/NextPlanClaimRefreshChangedPathListing.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode with the landing ref,
supplying client `claude` and the recorded conversation session ID. Then make
the smallest fix inside the `## In scope` boundary below. If root-causing shows
the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

Recommendation (the author's, not binding): restrict the listing to the cited
paths in step 6's own wording, so main runs
`git diff --name-only <sync.from> <sync.to> -- <cited paths>` and receives only
the intersection. Rationale: it is a one-sentence instruction change with no
script, schema, or result-shape change, and the output is bounded by the cited
path count rather than by primary's landing volume. The alternative the
checkpoint reviewer offered — having `Invoke-NextPlanClaim.ps1` report the
changed-path count plus only the intersecting paths — needs the cited paths as
a new script input and a new `sync` field documented in
`references/claim-results.md`; prefer it only if root-causing shows main cannot
reliably pass the cited paths as a pathspec.

## Critical files
- `.agents/skills/next-plan/SKILL.md`
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1` (only if the
  script alternative is chosen)
- `.agents/skills/next-plan/references/claim-results.md` (only if the script
  alternative is chosen)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/next-plan/SKILL.md`
  `## Steps` step 6's changed-path listing instruction, or, under the script
  alternative, the `sync` object construction in `Invoke-NextPlanClaim.ps1` and
  `references/claim-results.md` `## Session baseline and the sync object`

## Out of scope
- The landed change the session produced
- The rest of step 6: the `researcher` dispatch, its `Decisive checks` rows, and
  the return-to-step-4 loop
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. Never embed transcript paths or home paths.
Invariant: step 6 still detects every cited path that `sync.from..sync.to`
changed; only non-cited paths leave the main session's view.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  a final claim refresh that reports a `sync` object brings into the main
  session only the cited paths that changed (plus, under the script
  alternative, a changed-path count), never the full changed-path list
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
