<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-15T20:28:40.097Z","dependsOn":[]} -->
# Fix: Change Workflow Step 4 — pre-edit checks miss data mode, read the card

## Context
Both symptoms come from `.agents/references/change-workflow.md`
`#### Step 4 — Implement and propagate`, which schedules the acceptance checks
the execution card marks `before the first edit`, during one `/next-plan` run.

Symptom 1 — pre-edit baseline taken in the wrong data mode. The run's slices
edited shaders, a data input of the game project, so the post-change build
resolved Local data mode. Main's pre-edit `builder` `/compile` of the unedited
tree resolved Shared data mode (stamp match), and the `runner` took the
pre-change baseline screenshots in Shared mode. The runner named the data-mode
mismatch in its `Residuals`; main went on to the first slice without retaking
the baseline in Local mode. The final runtime check (the screenshot
comparison) returned `BLOCKED`, because `.agents/skills/agent-harness/SKILL.md`
`## Handoff` makes no comparison across data modes, and the user waived the
check. Cost: about 247 s of build time plus 458 s of runner time that produced
no usable comparison. The Step 4 `builder` bullet says nothing about the data
mode a pre-edit baseline needs; the rule that a change whose slices edit a data
input takes its pre-change baseline Local, through an untracked temporary file
placed before the build, lives only in `.agents/skills/agent-harness/SKILL.md`
`## Handoff` and `.agents/skills/compile/references/runtime-data-mode.md`
`## Mode selection`.

Symptom 2 — main read the execution card to schedule the pre-edit checks. To
schedule the `before the first edit` checks, main read a 50-line slice of the
preparation snapshot's `## Execution card` after a bounding Grep had already
identified the one pre-edit check, although the `runner` reads the check itself
from the card selector its brief cites. Step 4 names no handoff field or
selector that tells main which checks are pre-edit, so main reads the card.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 85e84643-6c43-4c42-89cb-db3da312f523
- Worktree/branch UUID: 42225028-bbfa-4078-98f5-84becc296ea6
- Session branch: claude/42225028-bbfa-4078-98f5-84becc296ea6
- Worktree: .claude\worktrees\BrokenEngine\42225028-bbfa-4078-98f5-84becc296ea6
- Landing ref: claude/42225028-bbfa-4078-98f5-84becc296ea6
  The review ref the invoker passes to /next-plan-review has two cases. When
  the `Observed in an earlier session` line is present, it is that line's
  landed commit hash, with no lookup. When that line is absent, the recording
  session is the observing session, and worktree reuse moves its branch past
  its landing, so the invoker never reviews the ref itself: it resolves the ref
  to the commit that added this Plan with
  `git log --follow --diff-filter=A -n 1 --format=%H <ref> -- <this plan path>`
  and passes that commit. When the recorded ref is gone or that command prints
  nothing (a later session moved this Plan), it runs the same command with
  `<ref>` omitted. A periodic Plan-history squash can make either command
  return an unrelated aggregate commit, and a landing that also deletes a
  similar file can make it return another session's commit, so review the
  result only when the commit is attributable to the observing session recorded
  above alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode, with the review ref
the provenance block above defines, supplying the recorded client and, on
Claude, the recorded conversation session ID; a Codex review supplies the
client and that review ref only. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

The author's recommendation, to confirm during root-causing: the Step 4
`builder` bullet routes a pre-edit build for a change whose slices edit a data
input to the existing Local-baseline rule by reference (citing its owning
section, not restating it), so the baseline is taken Local before the first
slice; and Step 4 has main dispatch the pre-edit checks by the execution-card
selector alone, without reading the card body.

## Critical files
- `.agents/references/change-workflow.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/references/change-workflow.md`
  `#### Step 4 — Implement and propagate`: its `Order` paragraph and its
  `builder` and `runner` bullets

## Out of scope
- The landed change the session produced
- The data-mode rule itself in `.agents/skills/agent-harness/SKILL.md`
  `## Handoff` and `.agents/skills/compile/references/runtime-data-mode.md`
  `## Mode selection`
- The execution-card format and the preparation handoff; if the fix needs a new
  handoff field naming the pre-edit checks, surface it for re-planning
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one workflow step's scheduling); escalate
if the fix reaches build/bootstrap coordination. Never embed transcript paths or
home paths. Each fact stays at its owning layer: Step 4 references the
data-mode rule rather than copying it.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a
  Step 4 run for a change whose slices edit a data input takes its pre-edit
  baseline in the mode the post-change build resolves, and main schedules the
  `before the first edit` checks without reading the execution card body
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
