<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-26T02:34:18.379Z","dependsOn":[]} -->
# Fix: .agents/skills/next-plan — step 4 cites full Task brief section instead of dedicated brief-template selector

## Context
- 2026-11-25 (`agy/d7526070-354e-4684-946f-69451661d537`): selector
  `.agents/references/subagent-reporting.md` lines 24-111; observed: full
  `## Task brief` read in main; bounding: give main a dedicated selector for
  the brief template and governing-path lookup.
- 2026-11-25 (`agy/8546d4d8-447a-43bf-bb88-7607c7955b01`): selector
  `.agents/references/subagent-reporting.md:24-111` (5,169 chars); bounding:
  give main the brief schema and `/next-plan` baseline mapping while citing
  worker instructions for dispatched roles.
- 2026-11-25 (`agy/67048fc4-9412-489e-bec4-ad39620046af`): selector
  `.agents/references/subagent-reporting.md:24-111`; bounding: keep the brief
  template and session identity mapping in main and leave worker verification
  instructions to each receiving role.

In the current tree, `.agents/skills/next-plan/SKILL.md` step 4 (lines 66-68)
and step 6 (line 134) cite `.agents/references/subagent-reporting.md`
`## Task brief`, which spans lines 24-111 with no sub-headings separating the
brief template and `/next-plan` identity mapping from pre-dispatch inventory
rules, bounded-reading guidance, and worker verification instructions.

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
boundary below — for example, adding focused `###` sub-headings inside
`.agents/references/subagent-reporting.md` `## Task brief` for the brief
template, `/next-plan` identity mapping, and governing-path lookup and updating
`.agents/skills/next-plan/SKILL.md` step 4 (and step 6 if applicable) to cite
those narrower selectors instead of the whole `## Task brief` section. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/next-plan/SKILL.md`
- `.agents/references/subagent-reporting.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/next-plan/SKILL.md`
  (`## Steps` step 4 and step 6 citation) and
  `.agents/references/subagent-reporting.md` (`## Task brief`)

## Out of scope
- The landed change the session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented `/next-plan`
  step 4 invocation: main reads only the dedicated brief-template, `/next-plan`
  identity-mapping, and governing-path lookup selectors rather than the full
  `## Task brief` section
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
- Converted from three distinct session-branch rows in
  `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` for key
  `(.agents/skills/next-plan, step 4 cites full Task brief section instead of dedicated brief-template selector)`.
