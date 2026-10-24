# Diagnosis builds that a Build required round-trip cannot carry

Open debt in `/external-diagnose-bug`: two diagnosis signals need a build, the
diagnosis worker may not run `/compile`, and a shared `Build required` request
does not cover them. A design choice is needed first, so this record sits
here. Once the user makes the choice, it becomes a Plan under
`Documents/Plans/ChangeWorkflow/`. Line numbers are from commit 6d3cf1270.

## Fixed constraint

`/compile` runs only inside a delegated `builder`
(`.agents/skills/compile/SKILL.md` `## Purpose`, lines 12-13; the
`.agents/references/change-workflow.md` `### Delegation roles` table). The
diagnosis worker is an `implementer` (`change-workflow.md` line 218), and a
subagent cannot dispatch one, so its only build route is the shared
`Build required` field. `subagent-reporting.md` line 114 sends that field to a
`builder`. The field carries "exact targets", meaning target plus
configuration/platform (`.agents/references/subagent-handoff.md` lines 18 and
50-52), and it is returned only in a handoff. This record leaves that
constraint as it is.

Step 2 item 3 (the `/compile` signal for build, link, and static failures)
already routes build signals through `Build required`
(`.agents/skills/external-diagnose-bug/references/worker.md` `## Steps`,
step 2 item 3).

## Gap 1 — differential run against an old build

`.agents/skills/external-diagnose-bug/references/worker.md` `## Steps`,
step 2 item 4 (lines 30-31), offers "the same input through the old and new
build or two configurations". Two configurations fit `Build required` as two
targets. An old build does not fit: the field names no commit and no tree, and
the builder builds the session worktree as it is.

## Gap 2 — temporary C++ instrumentation

Step 13 (lines 82-86) has the worker add `// TEMP START` / `// TEMP END`
blocks. Step 5 (line 44) relies on them ("even with a temporary log
(step 13)"). A temporary C++ log is observed only after a rebuild. Step 14
(lines 87-89) says instrumentation "is removed before the handoff", and
step 18 (lines 104-112) checks that removal. `SKILL.md` `## Handoff` requires
`Instrumentation removed` (line 52) and `Changed files` `none` (line 58). A
worker that returns `Build required` for its temporary code must therefore
remove that code in the same handoff. The builder then builds a tree without
the instrumentation, so the build can never show the log.

## Options

Gap 2:

- A. Keep the temporary blocks across an intermediate handoff. The handoff
  names the targets under `Build required`. Main runs a `builder`, then
  dispatches a fresh diagnosis worker with a continuation capsule, and that
  worker runs the harness and removes the blocks before its final handoff. This
  changes steps 14 and 18 and the `SKILL.md` `## Handoff` rules for
  `Changed files`, `Instrumentation removed`, and the intermediate status token.
  Only `PASS` and `BLOCKED` are defined today, at lines 60-63.
- B. Limit diagnosis to signals that need no rebuild: `set_log_level` /
  `get_logs` over existing logs, and harness queries. This drops temporary C++
  logs from steps 5 and 13.

Gap 1:

- C. Drop the old-build half of item 4 and keep the two-configurations half.
- D. Add a way to build an old commit for the diagnosis, for example a
  separate worktree at that commit built by a `builder`. The record has no
  evidence yet for which mechanism or owner would do this.

The author leans toward A and C, as a recommendation only. A reuses a pause
pattern that already exists: step 2 item 5 (lines 32-33) returns a requirement
to main "before diagnosis continues". C removes the half that has no carrier
and adds nothing new. The user decides.

## Tier when planned

The fix would be Tier 2, under the scoped tool behavior trigger: how one Change
Workflow skill, `/external-diagnose-bug`, gets its build signals. That holds
while the change stays inside `.agents/skills/external-diagnose-bug/`. If the
chosen option changes the shared handoff form or adds a WorktreeCli
mechanism (option D), re-derive the tier from
`.agents/references/risk-tiers.md` when writing the Plan. No determinism/CRC,
serialization, replay, wire, threading, or trust surface is reached.
