<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-06T19:20:36.390Z","dependsOn":[]} -->
# Fix: /agent-harness — a CPU timing scenario's `Profile` requirement is absent from the public text

## Context

Observed symptom: the pre-edit baseline for a CPU timing criterion, labelled
`Cost` on its execution card, was dispatched to `/agent-harness` on the Debug
build. The card cited `.agents/skills/agent-harness/references/command-reference.md`
`## Durable caveats`, whose `query_profile` comparison rule names no
configuration. `.agents/skills/agent-harness/SKILL.md:58` keeps every game-project
scenario on Debug unless an acceptance criterion needs another configuration's
own behavior, without saying a CPU timing is such a criterion. The rule that
settles it, that Debug and Release CPU timings are not acceptance evidence, is
at `.agents/skills/agent-harness/references/worker.md:857`
(`### Performance investigations`), which that file's skill marks private to the
executing worker.

The first baseline dispatch returned `BLOCKED` on `Cost`. Main then built
`Profile` and dispatched a second baseline.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 4ba81e08-3493-4492-84a8-df446e573f41
- Worktree/branch UUID: aa8d6b01-5a38-4ae9-a02f-fbbcb7611124
- Session branch: claude/aa8d6b01-5a38-4ae9-a02f-fbbcb7611124
- Worktree: .claude\worktrees\BrokenEngine\aa8d6b01-5a38-4ae9-a02f-fbbcb7611124
- Landing ref: claude/aa8d6b01-5a38-4ae9-a02f-fbbcb7611124
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/NameProfileForCpuTimingScenarios.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design

First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the
recorded client and the recorded conversation session ID. Then make the smallest
fix inside the `## In scope` boundary below. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

The author recommends one clause in `SKILL.md` `## Inputs` naming a CPU timing
criterion, on both sides of a comparison, as one that requests `Profile`, because
`## Inputs` is the section a brief is composed from, and one citation of that
sentence in the `query_profile` bullet of `## Durable caveats`, because that
bullet is what the card cited. `worker.md:857` stays as written.

## Critical files

- `.agents/skills/agent-harness/SKILL.md` — `## Inputs`
- `.agents/skills/agent-harness/references/command-reference.md` —
  `## Durable caveats`

## In scope

- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the configuration sentences of
  `SKILL.md` `## Inputs` and the `query_profile` comparison bullet of
  `command-reference.md` `## Durable caveats`

## Out of scope

- The landed change the session produced
- `worker.md`, `Invoke-HarnessClaim.ps1`, and any change to the `/compile`
  configurations
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 1: the highest trigger is documentation with no public signature or
invariant exposure, since a brief-composition sentence changes and no script or
skill contract does. Determinism, wire, serialization, replay, affinity,
threading, allocation, shader, and build surfaces are not exposed. The change
needs no live verification, and the supplied evidence records no harness setup.
Never embed transcript paths or home paths.

## Acceptance criteria

- The recorded symptom no longer reproduces under the documented invocation: a
  brief composed from `SKILL.md` `## Inputs` for a CPU timing criterion requests
  `Profile` for both comparison sides
- The `query_profile` bullet of `## Durable caveats` names where that requirement
  is stated
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
