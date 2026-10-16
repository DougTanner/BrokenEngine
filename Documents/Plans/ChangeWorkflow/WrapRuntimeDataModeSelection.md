<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-15T22:40:50.875Z","dependsOn":[]} -->
# Fix: /compile runtime-data-mode.md — `## Mode selection` bullets elided by Grep as overlong lines

## Context
During a `/next-plan` run for `Documents/Plans/Engine/FlightPlaneData.md`, the
main session needed to know whether the build should use Local or Shared data.
It ran Grep with pattern `^## Mode selection` and `-A 30` on
`.agents/skills/compile/references/runtime-data-mode.md`. Six lines in the
result came back as `[Omitted long context line]`, including the Local-trigger
and exemption bullets the decision depended on. The session then ran a second
shell read (`sed` piped to `cut`) to see those bullets.

Measured at baseline `56efaca5`: each `## Mode selection` bullet (lines 7-13) is
a single unwrapped paragraph. Lines 8 (872 characters), 9 (636), 10 (788),
12 (1,187), and 13 (1,074) exceed the length at which Grep context lines are
elided. Line 32 (625, in `## Build invocation and derived properties`) also
falls inside the `-A 30` window, which makes six omitted lines. Lines 7 (266)
and 11 (256) were returned. Grep returned a 460-character line elsewhere
without eliding it, so the threshold lies between 460 and 625 characters.

The same checkpoint review noted that a dispatched worker could have returned a
one-line Local/Shared verdict instead of the main session reading this section.
That is a different fix site and is out of scope here.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 5110f3a4-1e4c-443c-8c42-afea167fec2e
- Worktree/branch UUID: fc541619-370f-4444-95c7-e171693702c1
- Session branch: claude/fc541619-370f-4444-95c7-e171693702c1
- Worktree: .claude/worktrees/BrokenEngine/fc541619-370f-4444-95c7-e171693702c1
- Landing ref: claude/fc541619-370f-4444-95c7-e171693702c1 (the observing
  session records and lands this Plan itself; the branch tip is that session's
  final commit and survives exactly as long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/WrapRuntimeDataModeSelection.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode, using the landing ref
and supplying client `claude` and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author recommends a meaning-preserving hard wrap of each `## Mode selection`
bullet. Wrap at about 80 columns, the width `.agents/skills/*/SKILL.md` files
already use, with continuation lines indented two spaces under the bullet. No
word, path, or rule should change. Wrapping keeps every bullet in place and
under the Grep elision threshold, so a bounded section read returns the whole
rule set.

## Critical files
- `.agents/skills/compile/references/runtime-data-mode.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `## Mode selection` section of
  `.agents/skills/compile/references/runtime-data-mode.md` (its bullet lines)

## Out of scope
- The landed change the session produced (`Documents/Plans/Engine/FlightPlaneData.md`)
- Any wording, rule, or meaning change in `## Mode selection`
- The other sections of `runtime-data-mode.md`, even where they hold similarly
  long lines (for example `## Local generation`); the observed friction named
  only `## Mode selection`
- A worker dispatch that returns a one-line Local/Shared verdict, or any change
  to `/compile` `SKILL.md`, `/next-plan`, or the Grep tool
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1. Trigger: documentation only. The fix rewraps one reference
section, preserves its meaning, and leaves every script, signature, and
invariant alone. Escalate to Tier 2 if root-causing moves the fix into tool
behavior. Never embed transcript paths or home paths.

## Acceptance criteria
- Grep `^## Mode selection` on
  `.agents/skills/compile/references/runtime-data-mode.md`, with an `-A` count
  that reaches the next `##` heading, returns every line of the section and no
  `[Omitted long context line]`. No line in the section exceeds 400 characters
- The section's word sequence is unchanged apart from whitespace
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Coordination
- `Documents/Plans/ChangeWorkflow/ProjectAwareTooling.md` restates the Sandbox
  paths in the same `## Mode selection` bullets per project. Whichever Plan
  lands second rebases its wording onto the other; neither changes the other's
  meaning.
