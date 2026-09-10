<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-10T21:42:10.215Z","dependsOn":[]} -->
# Fix: Get-SessionChangeInventory.ps1 — landing acceptance skeleton owes no coherence review for Markdown-only or Plan-only changes

## Context
Observed symptom: `.agents/scripts/Get-SessionChangeInventory.ps1` builds its landing acceptance skeleton from `$script:AcceptanceSkeletonChecks` (:37-44), whose only entries are `/code-style-review`, `/comment-review`, `/update-vcxproj`, `/external-skill-creator validate mode`, `/update-claude-docs`, and `/progressive-disclosure-review`. Their triggers (:358-368) fire on changed C++/GLSL, source membership, changed skill packages, and instruction docs; none fires on changed Markdown, Plan, or script files as such. A session whose whole diff was three new `Documents/Plans/**` files therefore ran `Get-SessionChangeInventory.ps1 -Landing` and received an `acceptanceSkeleton` owing only the Executable Plan check, so `/finalize-changes` filled the Review-and-resolve-correctness acceptance row as UNVERIFIED with no `/coherence-review` handoff in existence. Rework forced: main dispatched `/coherence-review` only after the landing commit had been prepared, that review returned eight accepted findings, and resolving them took three fix/re-review rounds plus a re-preparation of the already-prepared candidate commit.

Session provenance (machine-local; not reproducible after cleanup). The Client through Worktree fields name the session that observed the friction — the session `/next-plan-review` must reach — while the `Landing ref` line names a ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: e254c889-b1bd-41e0-8b74-c61ced845bcc
- Worktree/branch UUID: 637a7f16-f462-48b6-9ea7-79aa886b3d95
- Session branch: claude/637a7f16-f462-48b6-9ea7-79aa886b3d95
- Worktree: .claude\worktrees\BrokenEngine\637a7f16-f462-48b6-9ea7-79aa886b3d95
- Landing ref: claude/637a7f16-f462-48b6-9ea7-79aa886b3d95
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`. Only when the transcript is genuinely needed, in a new session run `/next-plan-review claude/637a7f16-f462-48b6-9ea7-79aa886b3d95` in bounded friction mode, supplying client `claude` and conversation session ID `e254c889-b1bd-41e0-8b74-c61ced845bcc`. Then make the smallest fix inside the `## In scope` boundary below. If root-causing shows the fix lies outside that boundary, surface it for re-planning instead of expanding scope.

Author's recommendation, for the fix session to confirm or replace: add one `coherenceReview = '/coherence-review'` entry to `$script:AcceptanceSkeletonChecks` and a matching trigger in the trigger computation that fires on changed tracked non-C++/non-GLSL artifacts (the `plan`, `skill`, `script`, `doc`, and `vcxproj` classes the inventory already assigns), so the skeleton owes the row `.agents/references/change-workflow.md` :124-125 already requires — the Tier-1 non-C++ combined pass and the Tier-2+ other-artifacts pass. Whether the two Change Workflow dispatches need one skeleton row or two, and whether `repoCodeReview`/`glslReview` belong in the same set, are open for the fix session; keep the change to the minimum that makes a Markdown-only or Plan-only landing owe a coherence-review row.

## Critical files
- `.agents/scripts/Get-SessionChangeInventory.ps1`
- `.agents/skills/finalize-changes/references/landing-acceptance-table.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files named above — `$script:AcceptanceSkeletonChecks` and the trigger computation in `Get-SessionChangeInventory.ps1`, plus any acceptance-row wording in `landing-acceptance-table.md` the new row makes incorrect

## Out of scope
- The landed change the session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo
- Changing when `/coherence-review` itself runs, or its worker contract

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches build/bootstrap coordination. Never embed transcript paths or home paths. The inventory result schema is consumed by `/finalize-changes`; a new skeleton row must keep the emitted row order deterministic.

## Acceptance criteria
- A `-Landing` inventory run over a diff of only `Documents/Plans/**` Markdown files emits a coherence-review row in `acceptanceSkeleton`
- The recorded symptom no longer reproduces under the documented invocation
- /external-skill-creator validate mode passes wherever the Change Workflow Apply the triggered cleanup step triggers it; plan validate exits 0
