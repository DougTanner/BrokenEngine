<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-09T13:45:35.815Z","dependsOn":[]} -->
# Fix: change-workflow.md User Interaction — decision options lose their labels when rendered

## Context
Observed symptom: during a `/next-plan` run, main asked the user to choose
between options for one decision and presented those options as a numbered list
nested under a bullet. The user's client rendered that nested list with the
labels a/b/c instead of 1/2/3. The user answered "a", which did not match any
label main had written, so main had to ask what the answer meant: one extra
round trip before the decision could be acted on.

`.agents/references/change-workflow.md` `### User Interaction` owns how main
presents questions and options to the user (`/next-plan` `### Implementation
approval`, `/plan-alternatives` `### User presentation`, `/finalize-changes`, and
`/what` defer to it). It requires options, trade-offs, and a recommendation and
says how to compare options, but it says nothing about how options are labelled,
so nothing ties the label the user sees to the label main reads back.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: b8a2e1a8-4453-4ff9-830f-4b5ce81fa393
- Worktree/branch UUID: 967a3660-a0a0-4100-9bd1-44cffe47f82d
- Session branch: claude/967a3660-a0a0-4100-9bd1-44cffe47f82d
- Worktree: .claude\worktrees\BrokenEngine\967a3660-a0a0-4100-9bd1-44cffe47f82d
- Landing ref: claude/967a3660-a0a0-4100-9bd1-44cffe47f82d — the observing
  session records and lands this Plan itself; that branch's tip is the session's
  final commit and survives exactly as long as the worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/LabelUserDecisionOptionsExplicitly.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref —
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

Author's recommendation, with rationale: add one sentence to the existing
`Comparing options` bullet of `### User Interaction` requiring each option to
start with an explicit written label such as `Option 1:`, not a
renderer-generated list marker, and a question that names options to use those
same labels. A written label survives any client rendering, while a top-level
numbered list still depends on the renderer keeping its numbers. The rule leaves
fixed verbatim questions that name no option label, such as
`/plan-alternatives` `### User presentation`'s, unchanged. One sentence in the
bullet that already governs options adds no new section.

## Critical files
- `.agents/references/change-workflow.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `### User Interaction` section of
  `.agents/references/change-workflow.md`

## Out of scope
- The landed change the session produced
- The skills that defer to `### User Interaction` for their presentation
  (`/next-plan`, `/plan-alternatives`, `/finalize-changes`, `/what`)
- Every other section of `change-workflow.md`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (trigger: documentation-only wording in one instruction
reference, no script or tool behavior). Never embed transcript paths or home
paths.

## Acceptance criteria
- `### User Interaction` requires every option in a user decision request to
  start with an explicit written label, not a renderer-generated list marker,
  and a question that names options to use those same labels.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
