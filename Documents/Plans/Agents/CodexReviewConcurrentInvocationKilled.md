<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T22:48:04.180Z","dependsOn":[]} -->
# Fix: codex-review — two concurrent invocations are both killed, producing no output

## Context

During this session two `/codex-review` dispatches were launched at the same
time as background shell commands, exactly as
`.agents/skills/codex-review/SKILL.md` documents the run step, each with its own
distinct prompt and output file:

```powershell
pwsh -NoProfile -File .codex/codex-review.ps1 -Worktree <worktree> -PromptFile <distinct prompt> -OutFile <distinct out>
```

Both concurrent runs ended with background task status "stopped" mid-run, and
neither `-OutFile` was ever written, so neither review produced any result or
diagnosis. Every sequential run of the identical commands — before the
concurrent pair and again after it — exited normally and wrote its `-OutFile`.
The work had to be redone by serializing the two reviews, which is the current
undocumented workaround.

The root cause is not proven: the observed kill is consistent either with host
background-task management terminating one or both detached shells, or with a
concurrency limit inside `.codex/codex-review.ps1` or the `codex` CLI it
invokes. Nothing in `.agents/skills/codex-review/SKILL.md` says whether
concurrent review dispatches are supported, so a caller reasonably launches
several at once.

The codex-review skill and wrapper are outside the claimed Plan
`Documents/Plans/Agents/CompileAbsoluteTargetPathResolution.md`, whose scope is
the compile skill's build target invocations and `Tools/WorktreeCli` target path
handling, so this is tooling friction rather than an in-scope failure.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: fe36f80e-d412-42e1-a666-ef41c25e7922
- Worktree/branch UUID: c0faf209-8637-4558-9c1f-4aa17b596692
- Session branch: claude/c0faf209-8637-4558-9c1f-4aa17b596692
- Worktree: .claude\worktrees\BrokenEngine\c0faf209-8637-4558-9c1f-4aa17b596692
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
the proven transcript — deciding whether the kill originates in host background
task management or in the wrapper/CLI — then make the smallest fix inside the
`## In scope` boundary below: either make concurrent dispatches safe, or state
and enforce serialization in the skill so a caller never launches a pair that
will be killed. If root-causing shows the fix lies outside that boundary,
surface it for re-planning instead of expanding scope.

## Critical files

- `.codex/codex-review.ps1` — the wrapper each dispatch invokes, including its
  result-copy handling that leaves no `-OutFile` when the run does not complete.
- `.agents/skills/codex-review/SKILL.md` — the run step and its dispatch
  guidance, which says nothing about concurrent invocations.

## In scope

- Root-cause investigation via `/next-plan-review`, run with the recorded
  landing ref, client, and conversation session ID.
- The smallest resulting fix, confined to `.codex/codex-review.ps1` and the run
  step and fallback guidance of `.agents/skills/codex-review/SKILL.md`.

## Out of scope

- The landed change this session produced and every file it touched.
- Review prompt assembly, review content, and the Codex model, effort, sandbox,
  or auth selection.
- Unrelated skills/scripts; any transcript path or transcript text in the repo.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. The wrapper must keep its existing local failure
exit-code contract meaningful to the skill's `CODEX-UNAVAILABLE` fallback, and
must never leave a partially written `-OutFile` that a caller could read as a
complete review. Never embed transcript paths or home paths in tracked files.

## Coordination

- The landed commit `98fb1f7` ("Launch codex-review asynchronously and poll for
  completion") fixed a different symptom — a single long review killed at the
  host's 10-minute command cap — by restructuring `.codex/codex-review.ps1`
  into a launch-plus-poll contract and rewriting the same skill run step.
  Re-read those regions before editing: the fix here must preserve that
  launch/poll contract while reaching this Plan's concurrency outcome, and its
  reproduction evidence below predates that restructuring, so confirm the
  failure still reaches the current code before implementing.

## Acceptance criteria

- Two `/codex-review` dispatches issued as documented, with distinct
  `-PromptFile` and `-OutFile`, either both complete and write their output
  files, or are prevented by documented, enforced guidance before either run
  starts — the silent both-killed, no-output outcome no longer reproduces.
- A failed or refused run still reports through the existing exit-code and
  `CODEX-UNAVAILABLE` contract rather than vanishing without diagnosis.
- `/validate-skill` passes for `.agents/skills/codex-review/SKILL.md`;
  WorktreeCli `plan validate` exits `0` with `status:valid` and `code:ok`.

## Notes

This Plan is keyed to the pair (`.codex/codex-review.ps1` / `/codex-review` run
step, concurrent invocations both killed with no output file). Re-observing that
same symptom updates nothing and is a duplicate. The root cause is deliberately
deferred to `/next-plan-review`; this body records the commands, observed
result, workaround, and provenance without embedding transcript material.
