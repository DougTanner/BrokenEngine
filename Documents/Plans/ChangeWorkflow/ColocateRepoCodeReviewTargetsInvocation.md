<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T22:55:17.230Z","dependsOn":[]} -->
# Fix: repo-code-review `## Inputs` — the targets-file bullet names `Get-SessionChangeInventory.ps1 -EmitTargets` without its invocation, which sits 21 lines further into the section

## Context

Observed symptom. Main composed a `/repo-code-review` brief by the section-bounded read that `.agents/references/subagent-reporting.md:93-97` prescribes: a `Grep` for `^## Inputs` in `.agents/skills/repo-code-review/SKILL.md` with 20 lines of trailing context. `## Inputs` runs from `:29` to `:79`. Its targets-file bullet (`:36-44`) names the producer only as `` `.agents/scripts/Get-SessionChangeInventory.ps1 -EmitTargets` ``. The complete invocation is at `:58-61`: `-RepositoryRoot`, `-Baseline`, and the single-call `| Set-Content -LiteralPath Temp/code-review-targets.json -NoNewline` form that the root `AGENTS.md` bundled-scripts rule permits. It is inside the section but past the 20 lines the search returned. Main then searched the script for its parameters, read the script header, and ran it as `... -EmitTargets > <file>; Get-Content <file>`. That is two commands chained in one shell call, against the one-script-per-call rule, instead of the documented form.

The misbehaving file is outside the claimed Plan's `## In scope`, which covered engine networking code and its documentation only.

Session provenance (machine-local; not reproducible after cleanup). The Client through Worktree fields name the session that observed the friction — the session `/next-plan-review` must reach — while the `Landing ref` line names a ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 7e1a7312-98f6-437d-ad5c-61651f58a4f3
- Worktree/branch UUID: e61b4640-26bd-4b7a-a2c0-5690beb0de97
- Session branch: claude/e61b4640-26bd-4b7a-a2c0-5690beb0de97
- Worktree: .claude\worktrees\BrokenEngine\e61b4640-26bd-4b7a-a2c0-5690beb0de97
- Landing ref: claude/e61b4640-26bd-4b7a-a2c0-5690beb0de97 (the observing session records and lands this Plan itself).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/ColocateRepoCodeReviewTargetsInvocation.md`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design

First root-cause the friction from the current tree and this Plan's `## Context`. Only when the transcript is genuinely needed, in a new session run `/next-plan-review claude/e61b4640-26bd-4b7a-a2c0-5690beb0de97` in bounded friction mode, supplying client `claude` and the conversation session ID above. Then make the smallest fix inside the `## In scope` boundary below. If root-causing shows the fix lies outside that boundary, surface it for re-planning instead of expanding scope.

Recommended shape (the author's recommendation): state the complete invocation once, at or immediately after the targets-file bullet where the script is first named, and keep the rest of the targets-file prose after it. The bullet then either carries the invocation or points to it by its position within `## Inputs`. This keeps the fact in one place, which the progressive-disclosure directive requires, while putting it within the first lines a trailing-context search returns. The cause could also be read as the trailing-context search in `subagent-reporting.md` returning less than a whole section. That reading is not recommended as the fix site, because every other skill's `## Inputs` would then be affected for a symptom observed in one section.

No C++, determinism/CRC, wire, serialization, `.pack`, replay, or build surface changes.

## Critical files

- `.agents/skills/repo-code-review/SKILL.md` — `## Inputs` (`:29-79`)

## In scope

- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `## Inputs` of `.agents/skills/repo-code-review/SKILL.md`: the targets-file bullet (`:36-44`) and the invocation paragraph (`:51-79`)

## Out of scope

- The landed change the session produced
- `.agents/scripts/Get-SessionChangeInventory.ps1` and its parameters
- `.agents/references/subagent-reporting.md` and every other skill's `## Inputs`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Tier 1. Trigger: documentation only, a reorder or cross-reference of the prose within one skill section, with no change to what the section requires. Escalate to Tier 2 if root-causing shows the required brief contents or the invocation itself must change. Never embed transcript paths or home paths.

## Acceptance criteria

- A `Grep` for `^## Inputs` in `.agents/skills/repo-code-review/SKILL.md` with 20 lines of trailing context returns the complete documented `Get-SessionChangeInventory.ps1 -EmitTargets` invocation, including its single-call save form, or an exact pointer to it.
- The invocation is stated exactly once in the file.
- The static-checks runner, invoked as `.agents/references/change-workflow.md` `#### Step 5 — Run targeted pre-review checks` documents it, reports every row the change triggers passing.
