<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T20:11:08.007Z","dependsOn":[]} -->
# Fix: Get-SessionChangeInventory.ps1 — region truncation blocks /code-style-review on a large session

## Context
Observed symptom: in a /next-plan run whose session changed 171 files (168
`cpp`, 1 `dual-language-header`, 2 `doc`) against baseline
`56f543cc5d2b69f7c88ac575d0bb86c35bbc05d1`, the /code-style-review worker ran
`pwsh -NoProfile -File .agents/scripts/Get-SessionChangeInventory.ps1
-RepositoryRoot <root> -Baseline 56f543cc5d2b69f7c88ac575d0bb86c35bbc05d1
-Regions` and got `status=pass`, `code=ok`, `truncation.regions`
`full=963` `emitted=291`, and top-level `truncated=true`. The script caps
regions at `$script:MaximumRegions = 400` and the whole stdout document at
`$script:MaximumOutputBytes = 131072`, shedding regions first
(`.agents/scripts/Get-SessionChangeInventory.ps1`, the region emission and
output-budget loop near the end of the script). The worker's step 5
range-completeness gate (`.agents/skills/code-style-review/references/worker.md`
step 5: ranges are usable only when `truncated` is false) therefore reported
the ranges unavailable before any C++ was reviewed. `Find-SessionCandidates.ps1`
inherited the same shortfall (`truncated=true` with 9 hits against its 400-hit
cap, via `regionsCapped`), and `Test-StyleRuleJudgment.ps1` blocks with
`judgment.inventory-truncated` on the same condition. The /glsl-review and
/progressive-disclosure-review workers also saw `truncated=true` and reasoned
around it.

Workaround forced: main re-dispatched five per-area /code-style-review runs
with caller-supplied scopes derived from `git diff -U0`. Cost: one wasted
dispatch (about 75k tokens, about 6 minutes).

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: fbec346a-f24d-469f-a997-e16c51a3d95a
- Worktree/branch UUID: 72d1b862-47bb-49ec-8ba2-4d27d0d1c138
- Session branch: claude/72d1b862-47bb-49ec-8ba2-4d27d0d1c138
- Worktree: .claude\worktrees\BrokenEngine\72d1b862-47bb-49ec-8ba2-4d27d0d1c138
- Landing ref: claude/72d1b862-47bb-49ec-8ba2-4d27d0d1c138 (the observing
  session records and lands this Plan itself; the branch survives exactly as
  long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/SessionInventoryRegionTruncationBlocksReview.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode with the landing ref
above, supplying client `claude` and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author's recommendation, for the fix session to confirm: add one optional
path-scope parameter to the inventory script (repository-relative path prefixes
restricting `entries` and `regions`), forward it from `Find-SessionCandidates.ps1`
and `Test-StyleRuleJudgment.ps1`, and let the /code-style-review worker accept
that filter for a session-changed scope, so a large session is reviewed as
several complete slices each under the caps. Rationale: it keeps the
completeness gate intact (a truncated slice still blocks) and keeps the stdout
budget that protects consumer contexts, instead of raising or dropping caps.
Alternatives the fix session may prefer if root-causing favors them: paging the
region output with an offset, or a class filter (`cpp`/`dual-language-header`)
— the latter alone would not have helped here, since 169 of 171 files were C++.
Whatever is chosen, when the gate still blocks, its report should name the
filter to re-run with rather than only "ranges unavailable".

## Critical files
- `.agents/scripts/Get-SessionChangeInventory.ps1`
- `.agents/scripts/Find-SessionCandidates.ps1`
- `.agents/scripts/Test-StyleRuleJudgment.ps1`
- `.agents/skills/code-style-review/references/worker.md` (steps 1-5)
- `.agents/skills/code-style-review/SKILL.md` (only if a new input is added
  to its dispatcher contract)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files named above: the
  inventory script's parameters and region/entry selection, the two helper
  scripts' inventory invocation, and the /code-style-review worker's scope and
  range-completeness steps plus its dispatcher-facing inputs

## Out of scope
- The landed change the session produced
- The inventory's cap values as a standalone change, and its result schema
  beyond what the chosen filter requires
- Other `-Regions` consumers (/comment-review, /glsl-review,
  /progressive-disclosure-review, /session-audit workers); they may adopt the
  filter in a later Plan
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 2 (scoped tool behavior): trigger is one tool's behavior — the session
inventory script and its /code-style-review consumers — with no
determinism, wire, serialization, or build/bootstrap surface. Escalate if the
fix changes the `broken-engine-session-change-inventory/v1` schema in a way
other consumers must absorb. Without a filter argument the inventory's output
stays byte-identical to today's. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a
  session whose unfiltered `-Regions` run truncates can be reviewed by
  /code-style-review through complete (non-truncated) filtered runs, and
  `Find-SessionCandidates.ps1` and `Test-StyleRuleJudgment.ps1` honor the same
  filter
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
Evidence: the /code-style-review evidence file `Temp/CodeStyleReview-Sweep.md`
`## Inventory truncation` of the observing session (machine-local).
