<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-19T16:02:33.923Z","dependsOn":[]} -->
# Fix: /next-plan acceptance dry-run briefs — full per-run handoffs flood main for three facts each

## Context
In a `/next-plan` run implementing `Documents/Plans/ChangeWorkflow/JevStyleRuleReadingOrder.md`, main dispatched the acceptance-table dry runs of `/code-style-review` to a `mechanic` worker: three runs in one dispatch, then a one-run re-check. Both briefs' `Required sections` demanded the full `/code-style-review` handoff for every run — every declared field each — plus the `git status --porcelain` output pasted inline. The returns measured 10686 and 5361 characters. The only facts main used were, per run, whether the expected observation was met, the `Status`, and the `Judgment` row, plus one line stating that the post-revert tree held only the pre-existing non-C++ files. The rest entered main's context unread; no rework followed, but the two returns were the run's largest tool results, and the `/next-plan-checkpoint-review` classed the finding `fixable-defect`.

Emitter named by that review: the acceptance-table dispatch under `.agents/skills/next-plan/SKILL.md` step 8 ("Implement the approved change. Done when its own acceptance checks pass."), which gives no bounding rule for a dry-run brief. The general brief form in `.agents/references/subagent-reporting.md` `## Task brief` says `Required sections` "names which skill-specific report sections the caller will read; the worker skips the rest", but names no shape for a brief whose worker runs another skill several times as evidence, so main defaulted to requiring everything.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 2230551d-9e61-416d-9d53-ffa095213bb7
- Worktree/branch UUID: 96b2cebf-15a0-4d5b-97be-d14a5ff0e899
- Session branch: claude/96b2cebf-15a0-4d5b-97be-d14a5ff0e899
- Worktree: .claude\worktrees\BrokenEngine\96b2cebf-15a0-4d5b-97be-d14a5ff0e899
- Landing ref: claude/96b2cebf-15a0-4d5b-97be-d14a5ff0e899 (the observing session records and lands this Plan itself, so the branch tip is that session's final commit and survives as long as the worktree above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/AcceptanceDryRunBriefBounding.md`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`: confirm that neither `.agents/skills/next-plan/SKILL.md` step 8 nor `.agents/references/subagent-reporting.md` `## Task brief` bounds what a brief for repeated evidence runs of another skill may require. Only when the transcript is genuinely needed, in a new session run `/next-plan-review claude/96b2cebf-15a0-4d5b-97be-d14a5ff0e899` in bounded friction mode, supplying the client `claude` and the conversation session ID above. Then make the smallest fix inside the `## In scope` boundary below. If root-causing shows the fix lies outside that boundary, surface it for re-planning instead of expanding scope.

The bounding rule, as accepted at the checkpoint: a brief that has a worker run another skill one or more times as acceptance evidence (a dry run) requires the worker to write each full run handoff to one gitignored `Temp/` file under `## Run <n>` headings, cited under `Evidence` as path plus selector, and to return one `Decisive checks` row per run — the expectation met or not, the `Status`, and the run's decisive field row — plus one row on the post-revert tree state. `git status --porcelain` output never travels inline; its one-line summary is that tree-state row.

Owning layer — the author's recommendation from the root `AGENTS.md` progressive-disclosure directive: the rule is a brief-authoring constraint that applies to any dispatch running another skill as evidence, not a `/next-plan` step or an acceptance-table verification rule, so it belongs once in `.agents/references/subagent-reporting.md` `## Task brief`, as a sentence extending the existing `Required sections` explanation. Rationale: that section already owns what `Required sections` means, every delegation reads it, and the shared handoff form in `.agents/references/subagent-handoff.md` `## Handoffs` already supplies the `Temp/` path-plus-selector mechanism the rule relies on, so the rule reuses an existing mechanism rather than adding one. `/next-plan` step 8 stays unchanged unless root-causing shows main at step 8 does not reach the brief form, in which case the step gains at most a cross-reference clause naming that section; `/verify-acceptance` is not the owner, because it verifies a completed table in a read-only reviewer and never authors dry-run briefs.

## Critical files
- `.agents/references/subagent-reporting.md` — `## Task brief`, the `Required sections` explanation
- `.agents/skills/next-plan/SKILL.md` — step 8 of `## Steps`, cross-reference only

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files named above: the dry-run bounding sentence in `.agents/references/subagent-reporting.md` `## Task brief`, and at most a cross-reference clause in `.agents/skills/next-plan/SKILL.md` step 8

## Out of scope
- The landed change the session produced (`Documents/Plans/ChangeWorkflow/JevStyleRuleReadingOrder.md`)
- `/code-style-review`'s own `## Handoff` form, which correctly declares the fields a single review returns
- `/verify-acceptance` and `.agents/references/subagent-handoff.md` `## Handoffs`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: changes what a dry-run worker returns to main across every skill that dispatches one); escalate if the fix reaches build/bootstrap coordination. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a brief authored from the amended `## Task brief` for three `/code-style-review` dry runs requires one `Decisive checks` row per run plus one tree-state row, and cites the full handoffs as a `Temp/` path plus `## Run <n>` selector
- `/progressive-disclosure-review` over the changed instruction prose reports the rule stated once at its owning layer
- The static-checks runner, invoked as `.agents/references/static-checks.md` documents it, reports every row the change triggers passing

## Notes
- Duplicate check: `Documents/Plans/ChangeWorkflow/` held only `JevStyleRuleReadingOrder.md` when this Plan was written; no live Plan keys on (`/next-plan` acceptance dispatch, oversized dry-run handoff).
- Reviewer tool-use IDs of the oversized returns, for the transcript review: `toolu_01NrTZzCGjp37yQmCbL5MRpk` (10686 chars, three runs) and `toolu_01619PFY7fZdGoxWHAVcgcce` (5361 chars, one-run re-check).
