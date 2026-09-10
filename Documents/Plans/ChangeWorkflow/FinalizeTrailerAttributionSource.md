<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-10T21:42:15.171Z","dependsOn":[]} -->
# Fix: /finalize-changes — commit-message rules name no source for the attribution trailer

## Context
Observed symptom: the commit-message rules in `.agents/skills/finalize-changes/references/worker.md` (:56-58, the landing-commit creation step) name no source for the commit's attribution trailer, and a case-insensitive search for `trailer` and `co-authored` across `.agents/` and `Documents/` matches nothing outside `.agents/skills/external-skill-creator/LICENSE.txt`, so no repository file states where the trailer comes from. In this session the finalizer brief supplied the trailer `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>` from main's own host attribution instruction, while the dispatched finalizer's host session configuration named `Claude Opus 5 (1M context)`. With nothing in the rules ranking the two, the worker trusted its own configuration, its trailer acceptance row returned FAIL with an attribution-contradiction residual, and main had to send a corrective re-preparation instruction and re-prepare the candidate commit.

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

Author's recommendation, for the fix session to confirm or replace: state in the finalizer's commit-message rules that the attribution trailer the dispatch brief carries is authoritative, because main's host attribution instruction names the model the user is actually interacting with, and that the worker's own session configuration never overrides it; and have the finalizer dispatch brief carry that trailer as a named field so a brief that omits it is visibly incomplete. Whether the brief field is required or optional-with-a-fallback, and which file owns the brief template, are open for the fix session.

## Critical files
- `.agents/skills/finalize-changes/references/worker.md`
- `.agents/skills/finalize-changes/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files named above — the commit-message rules in `worker.md` and the finalizer dispatch brief the skill declares

## Out of scope
- The landed change the session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo
- The wording of any specific attribution trailer, and any host or user configuration outside the repository

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches build/bootstrap coordination. Never embed transcript paths or home paths. The stated source must not conflict with the authority order in `.agents/references/authority-order.md`.

## Acceptance criteria
- The finalizer rules name exactly one authoritative source for the attribution trailer, and the dispatch brief carries it
- A finalizer whose own session configuration disagrees with the brief's trailer uses the brief's, with no acceptance FAIL and no attribution residual
- /external-skill-creator validate mode passes wherever the Change Workflow Apply the triggered cleanup step triggers it; plan validate exits 0
