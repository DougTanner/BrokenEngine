<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-16T13:48:24.935Z","dependsOn":[]} -->
# Fix: compile/builder — a long build backgrounded, then the turn ended waiting for it

## Context

Manager-session observation during the Change Workflow acceptance step for the
claimed Plan `Documents/Plans/Agents/CompilePrefastAnalysisRebuildEvidence.md`.
A delegated `builder` was dispatched to run three sequential `/compile` builds,
the last of them the long `-Prefast` Client Release build documented at
`.agents/skills/compile/references/prefast-mode.md`:

```text
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target Client -Configuration Release -Prefast
```

The builder launched that invocation as a host background shell task rather than
in the foreground, then ended its turn with the final text "I will now stop
making further tool calls and wait for the notification that background task ...
has completed" in place of a handoff or any other resumable work. Because that
text was delivered as the worker's final answer, the background build's
completion notification never resumed the worker. The acceptance sequence
stalled with nothing in flight until the manager noticed and manually resumed
the worker with an instruction to read the build result and continue — the
workaround that unblocked the step.

Both governing documents already argue against the pattern without naming it.
`.agents/skills/compile/SKILL.md:80` requires each build to "Run each build
invocation synchronously in the foreground and remain in-turn until its process
exit code and single JSON result are captured", give the call "the maximum
available execution timeout", and forbids `Start-Job`, a trailing `&`, a
fire-and-forget watcher, and ending a delegated turn while a build is running —
but its prohibition list names no host-provided background-execution parameter,
so a worker can read backgrounding through the host tool as unlisted.
`.agents/references/subagent-reporting.md:68-71` states a worker "ends its turn
with the handoff as its final answer and never enters an open-ended wait after
delivering it; continuation goes through the host's resume path", and that any
mid-task wait carries a bounded timeout — but it does not say that ending a turn
in order to await one's own background child is itself that prohibited
open-ended wait, which is exactly how the worker treated it. The exact root
cause is deliberately deferred to `/next-plan-review`.

The claimed Plan's `## In scope` is limited to the `-Prefast` build invocation
inside `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` and the text of
`.agents/skills/compile/references/prefast-mode.md`. The files misbehaving here
— the compile skill body's execution discipline and the shared delegated
reporting reference — lie outside that boundary, so this is tooling friction
rather than an in-scope failure of the active change.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: 6a6f432e-7392-49bd-9cc3-50b7d2239a3d
- Worktree/branch UUID: 7bb47afc-da3b-4686-b67c-2afba2b6daa5
- Session branch: claude/7bb47afc-da3b-4686-b67c-2afba2b6daa5
- Worktree: .claude\worktrees\BrokenEngine\7bb47afc-da3b-4686-b67c-2afba2b6daa5
- Landing ref: the session branch above, whose tip is the session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to this session alone
  (its diff limited to this session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact conversation session ID above.

## Design

In a new session, run `/next-plan-review <landing ref>` supplying the recorded
client and the recorded conversation session ID. Root-cause the friction from
the proven transcript — deciding which of the two documents left the pattern
reachable, and whether the worker backgrounded the build to dodge a host call
cap or simply chose to — then make the smallest resulting instruction change
inside the `## In scope` boundary below, so a worker running a long build either
runs it in the foreground with an adequate timeout within the host cap, or, if
it is backgrounded at all, never ends its turn expecting a child-completion
notification to resume it. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

## Critical files

- `.agents/skills/compile/SKILL.md` — the `## Execution and result discipline`
  section (`:80`), whose foreground/in-turn requirement and prohibition list
  name no host background-execution parameter.
- `.agents/references/subagent-reporting.md` — the `## Handoffs` turn-ending and
  bounded-wait paragraph (`:68-71`), which does not classify ending a turn to
  await one's own background child as the prohibited open-ended wait.

## In scope

- Root-cause investigation via `/next-plan-review`, run with the recorded
  landing ref, client, and conversation session ID.
- The smallest resulting instruction change, confined to the
  `## Execution and result discipline` section of
  `.agents/skills/compile/SKILL.md` and the `## Handoffs` turn-ending paragraph
  of `.agents/references/subagent-reporting.md`.

## Out of scope

- The landed change this session produced and every file it touched.
- `Invoke-CompileBuild.ps1`, WorktreeCli, MSBuild invocation, build
  serialization, locks, data mode, oracle receipts, and the
  `broken-engine-build-result/v1` envelope.
- PREfast mode content and every other compile skill section.
- Unrelated skills/scripts; any transcript path or transcript text in the repo.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. `.agents/references/subagent-reporting.md` governs
every delegated role, so any edit there must stay compatible with the existing
handoff shape, the `Build required` and trailing `Residuals` fields, and the
bounded mid-task wait rule. Never embed transcript paths or home paths in
tracked files.

## Acceptance criteria

- A `builder` following the documented `/compile` instructions for a long build
  either completes it in-turn, or, if the build is backgrounded, does not end
  its turn expecting a child-completion notification to resume it — the
  stalled-until-manually-resumed outcome no longer reproduces.
- The changed instructions still permit the documented re-invocation of an
  identical command after a host call timeout, and keep the existing handoff
  shape intact.
- `/validate-skill` passes for `.agents/skills/compile/SKILL.md`; WorktreeCli
  `plan validate` exits `0` with `status:valid` and `code:ok`.

## Notes

This Plan is keyed to the pair (`/compile` execution discipline plus the shared
delegated reporting reference, a worker backgrounding a long build and ending
its turn to await it). Re-observing that same symptom updates nothing and is a
duplicate. It is distinct from
`Documents/Plans/Agents/CodeQualityMetricsCompareHostTimeout.md` (a documented
foreground command killed at the host cap) and from
`Documents/Plans/Agents/CodexReviewConcurrentInvocationKilled.md` (two
concurrent codex-review background shells both killed with no output). The root
cause is deliberately deferred to `/next-plan-review`; this body records the
command, observed behavior, workaround, and provenance without embedding
transcript material.
