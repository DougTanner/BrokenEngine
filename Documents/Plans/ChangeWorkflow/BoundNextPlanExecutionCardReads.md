<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-26T02:34:27.433Z","dependsOn":[]} -->
# Fix: .agents/skills/next-plan — main reads whole Execution card instead of only approval sections and pre-edit gate

## Context
- 2026-11-24 (`agy/c8b8acf3-a87f-45e0-b77c-5c12d7bd857c`): selector: claimed-Plan
  preparation snapshot `## Execution card` (4,603 chars); bounding: read the two
  approval sections and query `Acceptance checks` for `before the first edit`
  gates.
- 2026-11-25 (`agy/320e1e78-f9bd-40aa-8241-ece6f657ade4`): selector: claimed-Plan
  preparation snapshot `## Execution card` (53 lines, 3,672 chars); bounding:
  read only the two approval sections and query `Acceptance checks`/`Roles`
  separately.
- 2026-11-25 (`agy/67048fc4-9412-489e-bec4-ad39620046af`): selector: claimed-Plan
  preparation snapshot `## Execution card` (143 lines, 6,718+ chars); bounding:
  cite `## Execution card` and AC IDs to reviewer and runner instead of reading
  the full card in main.

In the current tree, `.agents/skills/next-plan/SKILL.md`
`### Implementation approval` (lines 216-225) tells main to present only the
execution card's `### What does this plan do?` and
`### Why this is good for the codebase` sections, while
`.agents/references/execution-card.md` `## Template` (lines 10-65) places those
two `###` approval sections immediately above `- Goal:` through `- Roles:`
without a bounding heading or explicit read-boundary guidance keeping main from
reading the full `## Execution card` body during approval, review, or harness
dispatch.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: agy
- Conversation session ID: bc6732d0-b89b-4893-a756-20026e542517
- Worktree/branch UUID: 67048fc4-9412-489e-bec4-ad39620046af
- Session branch: agy/67048fc4-9412-489e-bec4-ad39620046af
- Worktree: .agy\worktrees\BrokenEngine\67048fc4-9412-489e-bec4-ad39620046af
- Landing ref: agy/67048fc4-9412-489e-bec4-ad39620046af
  The review ref the invoker passes to `/next-plan-review` resolves this ref to
  the commit that added this Plan with
  `git log --follow --diff-filter=A -n 1 --format=%H <ref> -- <this plan path>`
  and passes that commit. When the recorded ref is gone or that command prints
  nothing (a later session moved this Plan), run the same command with `<ref>`
  omitted. Review the result only when the commit is attributable to the
  observing session recorded above alone (its diff limited to that session's
  files); never review an aggregate or multi-session squash commit.
- Run the review before `/cleanup-worktrees` removes the worktree recorded
  above: `agy` review requires the conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session
run `/next-plan-review <review ref>` in bounded friction mode, with the review
ref the provenance block above defines, supplying client `agy` and the recorded
conversation session ID. Then make the smallest fix inside the `## In scope`
boundary below — for example, tightening `.agents/skills/next-plan/SKILL.md`
(and `.agents/references/execution-card.md` if a bounding heading or selector
helps) so main reads only the two approval sections plus targeted
`before the first edit` and `Roles` queries and passes `## Execution card` plus
criterion IDs to reviewers and runners instead of reading the full card. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/next-plan/SKILL.md`
- `.agents/references/execution-card.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/next-plan/SKILL.md`
  (`## Steps` and `### Implementation approval`) and
  `.agents/references/execution-card.md` (`## Template`)

## Out of scope
- The landed change the session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented `/next-plan`
  invocation: main reads only the two approval sections and targeted
  `before the first edit` / `Roles` queries, citing `## Execution card` and
  criterion IDs to reviewers and runners rather than reading the full card
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
- Converted from three distinct session-branch rows in
  `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` for key
  `(.agents/skills/next-plan, main reads whole Execution card instead of only approval sections and pre-edit gate)`.
