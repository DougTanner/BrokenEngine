<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-03T23:00:11.170Z","dependsOn":[]} -->
# Fix: codex-review SKILL.md — the up-front authorization condition is only stated in the worker file

## Context
`/codex-review`'s public file describes the up-front-authorization route twice —
`.agents/skills/codex-review/SKILL.md:28-33` (`## When to use`) and `:54-60`
(`## Handoff`) — and both times defers the wording of the authorization itself to
`references/worker.md` `### Fallback`. A main session deciding whether it already
holds that authorization therefore cannot decide from the public file, and this
session read `references/worker.md` to find out. That file is 191 lines of
worker-only Codex dispatch, wait, receipt, and `-NoRetry` mechanics, with
`### Fallback` at `:160-191`; roughly 58 lines and 3,150 characters of it entered
the main context to answer a one-sentence dispatch question. Root `AGENTS.md`'s
public/private skill rule makes `SKILL.md` the file that must suffice for the
dispatch decision.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 62f98e1f-3ac2-49b9-8c3a-08d631bb56db
- Worktree/branch UUID: ec8cd668-9c77-4241-bf54-16269ae03fa8
- Session branch: claude/ec8cd668-9c77-4241-bf54-16269ae03fa8
- Worktree: .claude\worktrees\BrokenEngine\ec8cd668-9c77-4241-bf54-16269ae03fa8
- Landing ref: claude/ec8cd668-9c77-4241-bf54-16269ae03fa8
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/ec8cd668-9c77-4241-bf54-16269ae03fa8` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below.

The author's recommendation: state the authorization condition itself in
`SKILL.md` — explicit user authorization given in the current session, routing
the same unchanged assignment to the Opus `reviewer` subagent — once, at the
place a dispatch decision is made, and keep the pointer to
`references/worker.md` `### Fallback` for the failure-time mechanics (what
counts as genuine failure, the single `-NoRetry` re-dispatch, the
`general-purpose` last resort). Stating the condition once and referencing it
from the second site is preferable to repeating it in both `## When to use` and
`## Handoff`. The relocation must not change what the condition is; the
worker file remains the authority for the mechanics.

If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/codex-review/SKILL.md` — `## When to use` (`:28-33`) and
  `## Handoff` (`:54-60`)
- `.agents/skills/codex-review/references/worker.md` — `### Fallback`
  (`:160-191`), the retained owner of the failure-time mechanics

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the two `SKILL.md` sections named
  above and, only where the same fact would otherwise be stated twice, the
  corresponding sentences of `references/worker.md` `### Fallback`

## Out of scope
- The routing policy itself: which roles route through `/codex-review`, the
  parent/manager orchestrator exclusion, and the fact that `codex-review` is the
  only skill that may name a model
- The Codex dispatch, wait, receipt, and `-NoRetry` mechanics
- Root `AGENTS.md` and `.claude/agents/reviewer.md`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (documentation relocation inside one skill package, behavior
preserving); escalate to Tier 2 if the change alters the authorization condition
or the routing it selects rather than only where the condition is stated.
Invariants to preserve: explicit user authorization in the current session
remains the only thing that unblocks the fallback, and the fallback target
remains the Opus `reviewer` subagent; `CODEX-UNAVAILABLE: <short reason>` with
the unchanged target remains the genuine-failure handoff; the skill's size and
public/private split stay within the skill-skeleton shape. Never embed
transcript paths or home paths.

## Acceptance criteria
- A main session can decide, from `SKILL.md` alone, whether an up-front
  authorization it holds routes the assignment straight to the Opus `reviewer`
- No fact is stated in both `SKILL.md` and `references/worker.md`
- `/validate-skill` and `/progressive-disclosure-review` pass on the changed
  files; plan validate exits 0
