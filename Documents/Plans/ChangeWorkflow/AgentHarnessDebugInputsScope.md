<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-29T14:56:30.474Z","dependsOn":[]} -->
# Fix: agent-harness SKILL.md — `## Inputs` names the Debug build only for replay

## Context
Observed symptom. A `/next-plan` run dispatched an `/agent-harness` worker to
record pre-edit baselines against a Release BrokenEngineSandbox server build.
Its scenarios placed ships with `create_cell`, `edit_frame`, and `read_frame`,
and each of those commands fails without `kbDebugInput`, that is, in any
non-Debug build:
- `create_cell` — `.agents/skills/agent-harness/references/command-reference.md:346`
- `edit_frame` — `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:116`
- `read_frame` — `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:192`

`.agents/skills/agent-harness/SKILL.md` `## Inputs` (lines 54-55), which the
delegator reads to request the build and write the brief, names a
`kbDebugInput` build only for a replay determinism check; the per-command rule
lives only in the command references. The Release dispatch returned no
results, and main had to request a Debug server build through `/compile` and
dispatch the harness worker a second time.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: be3ffb28-d3d7-4d8b-83f2-81a23f5d1939
- Worktree/branch UUID: 72ca68d0-0831-42bf-a368-d8e2ca9ab1ae
- Session branch: claude/72ca68d0-0831-42bf-a368-d8e2ca9ab1ae
- Worktree: .claude\worktrees\BrokenEngine\72ca68d0-0831-42bf-a368-d8e2ca9ab1ae
- Landing ref: claude/72ca68d0-0831-42bf-a368-d8e2ca9ab1ae, the session branch
  above, whose tip is that session's final commit and which survives exactly as
  long as the worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a
  periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above. OpenCode transcript review remains unsupported regardless of worktree
  retention.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session
run `/next-plan-review <review ref>` in bounded friction mode — the landing ref
— supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author recommends widening the existing replay sentence in `## Inputs` to
the general rule — a scenario that sends any command the command references
mark as requiring `kbDebugInput` needs a Debug build — and leaving each
command's own rule where it is, rather than listing every such command in
`SKILL.md`.

## Critical files
- `.agents/skills/agent-harness/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `kbDebugInput` sentence of
  `.agents/skills/agent-harness/SKILL.md` `## Inputs`

## Out of scope
- The landed change the session produced
- The command references and the commands' `kbDebugInput` gates themselves
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (documentation: one skill's input wording); escalate to Tier 2
if the fix needs a script change. Never embed transcript paths or home paths.

## Acceptance criteria
- `/agent-harness` `## Inputs` tells a delegator that a scenario sending
  `create_cell`, `edit_frame`, `read_frame`, or any other command that requires
  `kbDebugInput` needs a Debug build, not only a replay determinism check
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
