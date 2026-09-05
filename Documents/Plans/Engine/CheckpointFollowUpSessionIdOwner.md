<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-04T23:58:57.817Z","dependsOn":[]} -->
# Fix: run-checkpoint follow-up routing — who resolves the conversation session ID is stated only in the provenance reference

## Context
Observed during a `/next-plan` run's checkpoint follow-up routing.

`.agents/skills/next-plan/references/run-checkpoint.md:71-74` (`## Follow-up
routing`) says an `implementer` routes each accepted checkpoint finding through
`/create-follow-up-plans` "supplying the observed symptom with its citation plus
the provenance block sourced per follow-up-provenance.md". Read literally, that
sentence hands the whole provenance block, session ID included, to the
dispatched worker.

One field of that block cannot be sourced by the worker.
`.agents/skills/next-plan/references/follow-up-provenance.md:22-25`
(`## Conversation session ID`) states that main reads the ID from the
`CLAUDE_CODE_SESSION_ID` environment variable "in its own session shell ...
because the value differs per conversation and resume and a subagent shell
reports that subagent's own ID". `.agents/references/subagent-reporting.md`
states the same hazard from the worker's side, that in a subagent shell
`CLAUDE_CODE_SESSION_ID` names the parent session.

Observed cost this session: main's `/create-follow-up-plans` dispatch brief told
the worker to read the conversation session ID from its own shell. The worker
would have written a wrong ID into the Plan — the ID recorded in a
tooling-friction Plan is the only way `/next-plan-review` later finds the Claude
transcript — so main had to interrupt with a mid-task correction carrying the
correct ID. Rework: one wasted brief field, one mid-task correction message, and
a re-read of `follow-up-provenance.md` to confirm which side owns the value.

The manager writing the dispatch brief reads `run-checkpoint.md`; nothing on
that page says the session ID is main's own to resolve, and the constraint lives
one file away in the reference the sentence defers to wholesale.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 1af20171-b83a-4f30-a4ed-335f78190ee7
- Worktree/branch UUID: e05e48ea-58ba-4b02-908d-5b9c76c49e60
- Session branch: claude/e05e48ea-58ba-4b02-908d-5b9c76c49e60
- Worktree: .claude\worktrees\BrokenEngine\e05e48ea-58ba-4b02-908d-5b9c76c49e60
- Landing ref: claude/e05e48ea-58ba-4b02-908d-5b9c76c49e60
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/e05e48ea-58ba-4b02-908d-5b9c76c49e60` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author's recommendation, for the implementer to confirm or replace: state
once in `run-checkpoint.md`'s `## Follow-up routing` paragraph that main
resolves the conversation session ID and passes it in the dispatch brief, and
keep the reference to `follow-up-provenance.md` for the rest of the block. That
keeps the value's definition where it already lives and adds only the routing
fact the dispatching manager needs, per the root AGENTS.md progressive-disclosure
directive. Do not restate `CLAUDE_CODE_SESSION_ID`, the per-resume behaviour, or
the subagent-shell hazard on the checkpoint page.

## Critical files
- `.agents/skills/next-plan/references/run-checkpoint.md`
- `.agents/skills/next-plan/references/follow-up-provenance.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `## Follow-up routing` section of
  `.agents/skills/next-plan/references/run-checkpoint.md`, plus any wording
  change in `follow-up-provenance.md` `## Conversation session ID` that the fix
  requires to avoid stating the same fact twice

## Out of scope
- The landed change the session produced
- The provenance block's other fields, and how `/create-follow-up-plans` writes
  a Plan body
- `/next-plan-review` transcript discovery behaviour
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (mechanical documentation: instruction prose only, no public
signature or invariant exposure). Escalate if the fix reaches script behaviour or
the dispatch contract itself. Never embed transcript paths or home paths.

## Acceptance criteria
- `run-checkpoint.md` `## Follow-up routing` names main as the resolver of the
  conversation session ID passed in the dispatch brief
- The `CLAUDE_CODE_SESSION_ID` mechanics remain stated in exactly one file
- `/validate-skill` passes for any changed skill package; plan validate exits 0
