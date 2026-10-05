---
name: sweep
description: >-
  Run a large scoped cleanup sweep on Codex with this Claude Code session as
  the orchestrator: per-file Codex find and fix runs, per-batch propagation, a
  Codex stage cleanup and ordering review, then Claude builds, checks, and one
  landing. Use when a Plan claimed through /next-plan carries a `## Sweep`
  section, or to answer a request for a large Codex-run cleanup of whole areas
  without a saved Plan. Not for a findings-only scoped review, such as the
  cleanup scope of /comment-review or /code-style-review. Codex callers never
  invoke it.
allowed-tools: [Read, Agent, PowerShell, Skill, Monitor, AskUserQuestion]
---

# Sweep

## Purpose

Runs one sweep Plan's scoped cleanup on Codex — per-unit find and fix,
per-batch propagation, stage cleanup and ordering review — while this Claude
session picks the model, builds, checks, and lands.

## When to use

- A Plan claimed through `/next-plan` carries a `## Sweep` section;
  `/next-plan` `## Rules` routes it here.
- The user asks for a large Codex-run cleanup of whole areas with no saved
  Plan: ask the user to save one with `/save-plan`, carrying the `## Sweep`
  section, and for `cpp-adoption` or `document` the `## Sweep rules` section,
  that `## Inputs` defines, then run `/next-plan <Plan>`. `/save-plan` is
  user-invoked, so never run it yourself.

Claude Code only: the coordinator launches Codex, so Codex callers never invoke
it.

## Inputs

The claimed sweep Plan. Its `## Sweep` section holds three bullets, values in
backticks:

```text
## Sweep

- Type: `style-guide`
- Targets: `Engine/*.h`, `Engine/*.cpp`, `:(exclude)Engine/Source/Agent/*`
- Batches: `Engine/Source/Graphics`, `Engine/Source/Ui`
```

- `Type` — a file under `references/types/`, listed in `## References`.
- `Targets` — Git pathspecs passed to `git ls-files`, exclude magic allowed.
  The coordinator rejects a target under `.agents/`.
- `Batches` — optional directory prefixes that are batches of their own; every
  other target file batches by its top-level directory. The listed order is the
  run order: the listed batches run first, in that order, then the remaining
  top-level directories by name, so list every area whose run order matters
  relative to another. Each file goes to the first listed prefix it falls under,
  so list a deeper prefix before its parent; listed after it, the deeper prefix
  gets no files. List here every directory holding more than 20 C++ files, and
  order the stage Plans so an area is swept before the areas that consume its
  renames.

A type whose `## Find rules` defers to the Plan (`cpp-adoption`, `document`)
also needs a `## Sweep rules` section: what FIND looks for, the target form,
and any rulings. Files differing only in a `.h`/`.cpp` extension form one unit;
every other file is its own unit.

## Steps

The stage baseline is the SHA in `Temp/Sweep/Baseline.txt`, which the first
`-Batch` run writes. The `BATCH-DONE` and `CLOSE-DONE` lines end in the fields
`cpp=yes|no`, `builds=<targets>`, and `profile=yes|no`, which the coordinator
derives from the C++ files changed since the baseline.
DataPacker, WorktreeCli, and AgentHarness build in Release only (`/compile`
`## Inputs`).

### Setup

1. Run `pwsh -NoProfile -File .agents/skills/sweep/scripts/Invoke-Sweep.ps1 -ListModels`,
   then ask the user to pick one listed slug, offering the one marked
   `(recommended)` first. Done when the user has chosen; that slug is `-Model`
   for every `-Batch` and `-Close` launch.
2. Run `pwsh -NoProfile -File .agents/skills/sweep/scripts/Invoke-Sweep.ps1 -Plan <Plan> -List`.
   Done when it prints each batch name with its unit count, in run order.

### Per batch

Run the batches one after another in the order step 2 printed them, with no
commit before landing.

3. Delete `Temp/Sweep/Status.txt` if present, then launch the coordinator
   detached, because a batch outlives the 2-hour background-task limit:

   ```powershell
   Start-Process pwsh -WindowStyle Hidden -WorkingDirectory '<worktree root>' -ArgumentList '-NoProfile','-File','.agents/skills/sweep/scripts/Invoke-Sweep.ps1','-Plan','<Plan>','-Model','<slug>','-Batch','<batch>'
   ```

   When the user asks for more or less parallelism, append `'-Throttle','<n>'`
   to a `-Batch` launch to set how many of the batch's units run at once
   (default 8).
   Done when the process has started.
4. Wait with a Monitor until-loop until `Temp/Sweep/Status.txt` exists and no
   longer reads `RUNNING`; the delete in step 3 keeps the previous run's final
   line from ending the wait early. Set the Monitor to its maximum timeout, and
   re-arm it with the same loop whenever it expires while `Status.txt` still
   reads `RUNNING`. Done when `Status.txt` holds a final line.
5. On `INCOMPLETE` or `PROPAGATE-FAILED`, repeat steps 3-4 for the same batch;
   the coordinator resumes from `Temp/Sweep/Progress.md` and reruns the units
   `Temp/Sweep/Failures.txt` lists. Done when `Status.txt` reads `BATCH-DONE`.
6. When the `BATCH-DONE` line reads `cpp=yes`, dispatch one `builder` running
   `/compile` for its `builds=` targets: Client and Server in Debug, the others
   in Release. A failure goes to an
   `implementer` running `/resolve-findings`. Done when every build passes or
   the line reads `cpp=no`.

### Stage close

7. Delete `Temp/Sweep/Status.txt`, launch as step 3 with `'-Close'` in place of
   `'-Batch','<batch>'`, and wait as step 4. On `CLOSE-FAILED <run>`, repeat;
   finished runs are skipped. Done when `Status.txt` reads `CLOSE-DONE`.
8. When the `CLOSE-DONE` line reads `ordering=` above 0, dispatch an
   `implementer` running `/resolve-findings` on `Temp/Sweep/Ordering.md`
   `## Findings`; each finding is fixed, never ledgered. Done when every
   finding has a passing `/resolve-findings` handoff.
9. Run the static checks:

   ```powershell
   pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot '<worktree root>' -Baseline <baseline>
   ```

   Done when they pass.
10. When the `CLOSE-DONE` line reads `cpp=yes`, dispatch one `builder` running
    `/compile` for its `builds=` targets: Client and Server in Debug and
    Release, plus Profile when the line reads `profile=yes`; the others in
    Release. Done when every build passes or the line reads `cpp=no`.
11. Report the stage per `## Handoff`, then land through `/next-plan` steps
    9-11. Done when `/next-plan` step 11 is done.

## Handoff

The stage report main gives the user, from the `CLOSE-DONE` line: units swept,
fixes applied, deferred entries, and the deferred-fixes record path the type's
`## Deferred fixes` names.

## Rules

- A sweep run does not follow the Change Workflow; it spends Codex tokens on a
  large scoped cleanup, approved as [`/next-plan`](../next-plan/SKILL.md)
  `## Rules` states. There is no `/prepare-change`, `/plan-alternatives`,
  `/plan-audit`, `/plan-simplicity-review`, execution card,
  `/repo-code-review`, `/comment-review` dispatch, `/coherence-review`,
  `/update-claude-docs`, or `/verify-acceptance`. A sweep keeps the per-batch
  and stage builds, the static checks, ordering fixes through
  `/resolve-findings`, and the landing gate. Any further review or runtime
  verification of the stage's change is main's decision: main follows the
  review the Plan states, or, when the Plan states none, asks the user before
  step 11.
- The `/finalize-changes` brief supplies the sweep outputs for the landing
  acceptance-table rows
  ([`landing-acceptance-table.md`](../finalize-changes/references/landing-acceptance-table.md)
  `## Acceptance table`) a normal change fills from reviews: the unit
  `Temp/Sweep/units/*.fix.md` outputs and `Temp/Sweep/Cleanup.md` for the
  `/code-style-review` and `/comment-review` rows, and for the
  `/coherence-review` row over a document sweep's targets; the batch
  `Temp/Sweep/*.propagate.md` outputs for the `/update-claude-docs` row. Every
  other owed row a sweep does not produce, such as `/coherence-review` over the
  deferred-fixes record, is shown `UNVERIFIED`, never `PASS`, for the user's
  single landing confirmation.
- Out-of-bound findings reach the deferred-fixes record for the user to decide
  on; the sweep creates no follow-up Plans on its own.
- Running a sweep authorizes Local generation (`-RunDataPacker`, Gaea export
  still forbidden) for any build whose changes trigger Local data mode, such as
  `DataPacker/**` or `Common/DataFile.h`
  (`.agents/skills/compile/references/runtime-data-mode.md` `## Mode selection`).
  `Tools/` changes to WorktreeCli or AgentHarness follow `/compile`'s
  AgentTools policy.
- A finding whose fix needs an `.agents/` edit is declined and reaches the
  deferred-fixes record. A Codex git warning that it cannot read the user's
  global ignore file is harmless. Codex writing its own helper scripts and file
  backups under `Temp/Sweep/` is expected.
- Propagation failure modes seen so far, each caught by the per-batch Debug
  build: a rename that left callers of a removed member unchanged; a
  propagation helper script that wrote stray text before the first line of
  seven files; and a fix that moved a symbol or constant into a file, or
  qualified a C runtime name with `std::`, without adding the header that
  declares it. A failure in the precompiled header hides every later error, so
  rebuild after each repair.
- Codex agents hand off through files under `Temp/Sweep/`; main reads only
  `Status.txt` and the ordering findings it routes. `Progress.md` records each
  finished unit and batch, so a compacted or restarted session resumes where it
  stopped.
- The coordinator calls the `codex` CLI directly rather than through
  [`/claude-to-codex`](../claude-to-codex/SKILL.md), because it runs detached
  parallel unit pipelines.

## References

- [scripts/Invoke-Sweep.ps1](scripts/Invoke-Sweep.ps1) — the coordinator; its
  header owns the modes and its body the status lines and `Temp/Sweep/` files.
- [references/types/style-guide.md](references/types/style-guide.md) — the
  style guide rules, user rulings, fix bound, and cleanup checklist.
- [references/types/cpp-adoption.md](references/types/cpp-adoption.md) — a C++
  feature adoption ruled by the Plan's `## Sweep rules`.
- [references/types/document.md](references/types/document.md) — an `AGENTS.md`
  or documentation pass.
- [references/prompts/Find.md](references/prompts/Find.md),
  [Fix.md](references/prompts/Fix.md),
  [Propagate.md](references/prompts/Propagate.md),
  [Cleanup.md](references/prompts/Cleanup.md),
  [Ordering.md](references/prompts/Ordering.md) — the Codex prompt templates
  the coordinator fills and runs.
