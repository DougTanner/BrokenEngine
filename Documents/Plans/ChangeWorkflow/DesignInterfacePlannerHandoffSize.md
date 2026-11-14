<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T00:03:09.586Z","dependsOn":[]} -->
# Fix: /external-design-interface — planner handoffs overrun the cap

## Context
Observed symptom: during a `/next-plan` run, `/external-design-interface` step 9
dispatched its three `planner` workers. Each returned the planner extension
`.agents/skills/external-design-interface/SKILL.md` `## Handoff` declares,
whose `Interface declarations: <header-style declarations>` and
`Representative usage: <dominant call-site example>` fields (with step 10's
requirement for "C++ interface declarations, representative usage") call for
multi-line C++ code. That conflicts with
`.agents/references/subagent-handoff.md` `## Handoffs`: every row is one line,
no code is quoted, a skill extension is one line or one row per item, and the
whole handoff stays under 4,000 characters. The three planner handoffs entered
the main session at 16,773, 14,458, and 11,880 characters, carrying 10, 6, and
6 code-fence markers, so main absorbed the full designs inline.

The shared form's overflow route (moving oversized fields to a `Temp/` file
cited under `Evidence`) was also unavailable: the skill's `## Rules` grant a
`Temp/` file only to the brief-writing `implementer` and the researcher, not to
planners, and the brief main wrote in that run told planners to write nothing.
The researcher already follows the intended shape — its full evidence goes to
`Temp/design-interface-pattern-evidence.md` under one `##` heading, and its
extension field stays one line.

Proposed direction from the run checkpoint (the author's recommendation, to
confirm during root-causing): each planner writes its full design to one
gitignored `Temp/` file under one `##` heading, returns one-line summaries per
planner extension field plus an `Evidence` row citing that file, the skill's
`## Rules` grant that one `Temp/` file per planner, and main reads the design
files only when it presents the `### Compare and Decide` comparison.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 643a18e1-7a75-449f-ac82-33cc9dea673f
- Worktree/branch UUID: 16f32bf6-ff9a-4000-a8ae-2650fdff52a6
- Session branch: claude/16f32bf6-ff9a-4000-a8ae-2650fdff52a6
- Worktree: .claude\worktrees\BrokenEngine\16f32bf6-ff9a-4000-a8ae-2650fdff52a6
- Landing ref: claude/16f32bf6-ff9a-4000-a8ae-2650fdff52a6
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

## Critical files
- `.agents/skills/external-design-interface/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to
  `.agents/skills/external-design-interface/SKILL.md`: `## Steps` step 10's
  planner requirements, the `## Handoff` planner extension and the paragraphs
  after it, `## Rules` (the `Temp/` file exceptions), and
  `### Compare and Decide` where main reads the designs

## Out of scope
- The landed change the session produced
- `.agents/references/subagent-handoff.md` and its 4,000-character cap
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one skill's handoff contract); escalate
if the fix reaches build/bootstrap coordination. Never embed transcript paths or
home paths. The workflow stays read-only apart from gitignored `Temp/` files.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  the planner extension fields are one-line rows with no quoted code, each
  planner's full design reaches main through a cited `Temp/` file the skill's
  `## Rules` permit, and a planner handoff stays under the shared cap
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
