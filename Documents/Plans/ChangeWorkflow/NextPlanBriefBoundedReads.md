<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-26T16:53:11.684Z","dependsOn":[]} -->
# Fix: /next-plan — step 4 brief writing pulls whole files into the main session

## Context
Context-efficiency envelope from the `/next-plan` run checkpoint of the Engine
Plan `GameBaseBackReferenceRemoval.md`, while main wrote the step 4 preparation
brief:
- Tool `Read`, invocation: the whole claimed Plan file, measured 21122 chars.
  Step 4 has main write the brief "from the claimed Plan's own citations" while
  the preparation `implementer` consumes the full Plan itself; a `^## ` heading
  Grep for section line ranges plus reads of only the sections the brief needs
  would have sufficed.
- Tool `Read`, invocation: the whole `.agents/references/subagent-reporting.md`,
  measured 9948 chars. Step 4 cites only its `## Task brief` section.

Checkpoint: the `/next-plan-checkpoint-review` of that run (session below).

Current-tree evidence: `.agents/skills/next-plan/SKILL.md` step 4 (the
`4. Dispatch one preparation implementer` item) cites
`../../references/subagent-reporting.md` `## Task brief` and says main writes
the brief from the Plan's citations, naming other references as paths only,
but gives no bounded way to gather those citations from the Plan. The root
`AGENTS.md` `## Directives` Bounded reading rule already makes a cited `##`
section the reading scope.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 5c6a9eef-1279-4356-8749-17c9c4d4bae1
- Worktree/branch UUID: b7e73f28-c654-42c5-aa6b-01ae9633e1eb
- Session branch: claude/b7e73f28-c654-42c5-aa6b-01ae9633e1eb
- Worktree: .claude\worktrees\BrokenEngine\b7e73f28-c654-42c5-aa6b-01ae9633e1eb
- Landing ref: claude/b7e73f28-c654-42c5-aa6b-01ae9633e1eb (the observing
  session records and lands this Plan itself; the branch tip is that session's
  final commit and survives exactly as long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/NextPlanBriefBoundedReads.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref
above — supplying client `claude` and the recorded conversation session ID.
Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author recommends one step 4 sentence telling main how to gather the
brief's inputs: a `^## ` heading Grep of the claimed Plan for section line
ranges, then reading only the sections the brief needs, and reading only the
cited `## Task brief` section of `subagent-reporting.md`. Rationale: both reads
are the same pattern at the same step, and the Bounded reading directive
already owns the general rule, so step 4 only needs to point at the bounded
route for its own inputs rather than restate that rule.

## Critical files
- `.agents/skills/next-plan/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/next-plan/SKILL.md`
  step 4 (the paragraph on how main writes the brief from the Plan's citations
  and the `## Task brief` citation)

## Out of scope
- The landed change the session produced
- `.agents/references/subagent-reporting.md` and the root `AGENTS.md`
- The step 4 role-assignment gap, owned by
  `Documents/Plans/ChangeWorkflow/NextPlanPreparationBriefRoles.md`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Change Workflow Tier 2 — trigger: scoped tool behavior (one skill's
brief-writing instruction). Escalate if the fix reaches build/bootstrap
coordination. Never embed transcript paths or home paths.

## Acceptance criteria
- A `/next-plan` run's main session writes the step 4 brief without a
  whole-file read of the claimed Plan or of `subagent-reporting.md`
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing

## Notes
- The `/external-skill-creator` validation applies to the fix because it
  changes a `.agents/skills/*/` package.
- `NextPlanPreparationBriefRoles.md` edits the same step 4; whichever lands
  second rebases onto the other's wording.
