---
name: sweep
description: >-
  Run a large scoped cleanup sweep on Codex from a non-Codex host (Claude
  Code, Antigravity CLI, or OpenCode): per-unit find/fix, per-batch
  propagation, stage cleanup/ordering, builds, static checks, and
  /finalize-changes landing. Explicit /sweep <spec-path> under
  Documents/Investigations/ or Projects/Coretide/Documents/Investigations/
  only; not for findings-only reviews or Codex callers.
disable-model-invocation: true
argument-hint: "<Documents/Investigations/...>"
allowed-tools: [Read, Agent, PowerShell, Skill, Monitor, AskUserQuestion]
---

# Sweep

## Purpose

Runs one sweep specification's scoped cleanup on Codex — per-unit find and fix,
per-batch propagation, stage cleanup, and ordering review — while a non-Codex
host picks the model, builds, checks, and lands through `/finalize-changes`.

## When to use

Only on explicit `/sweep <spec path>` for a specification under
`Documents/Investigations/` or `Projects/Coretide/Documents/Investigations/`.
Non-Codex hosts (Claude Code, Antigravity CLI, OpenCode) run `/sweep`;
`Invoke-Sweep.ps1` enforces the non-Codex branch check.

## Inputs

The sweep specification path (`## When to use`). Its `## Sweep` section holds
bullets with backticked values (`Batches` optional):

```text
## Sweep

- Type: `style-guide`
- Targets: `Engine/*.h`, `Engine/*.cpp`, `:(exclude)Engine/Source/Agent/*`
- Batches: `Engine/Source/Graphics`, `Engine/Source/Ui`
```

- `Type` — a file under `references/types/`, listed in `## References`.
- `Targets` — Git pathspecs passed to `git ls-files`, exclude magic allowed.
  The coordinator rejects a target under `.agents/`.
- `Batches` — optional directory prefixes forming their own batches (listed
  prefixes run first; first match wins, so list deeper prefixes before their
  parent). List directories with >20 C++ files and order staged specifications
  before consumers of their renames; use `-List` for exact batch run order.

A type whose `## Find rules` defers to the specification (`cpp-adoption`,
`document`) also needs a `## Sweep rules` section: what FIND looks for, the
target form, and any rulings. Files differing only in a `.h`/`.cpp` extension
form one unit; every other file is its own unit.

## Steps

The stage baseline is the SHA in `Temp/Sweep/Baseline.txt`, which the first
`-Batch` run writes. The `BATCH-DONE` and `CLOSE-DONE` lines end in the fields
`cpp=yes|no`, `builds=<targets>`, and `profile=yes|no`, which the coordinator
derives from the changed C++ files: for `BATCH-DONE`, those changed since the
tracked working tree as it stood before the batch's first unit ran; for
`CLOSE-DONE`, those changed since the stage baseline.
DataPacker, WorktreeCli, and AgentHarness build in Release only (`/compile`
`## Inputs`).

### Setup

1. Run
   `pwsh -NoProfile -File .agents/skills/sweep/scripts/Invoke-Sweep.ps1 -ListModels`,
   then ask the user to pick one listed slug, offering the one marked
   `(recommended)` first. Done when the user has chosen; that slug is `-Model`
   for every `-Batch` and `-Close` launch.
2. Run
   `pwsh -NoProfile -File .agents/skills/sweep/scripts/Invoke-Sweep.ps1 -Spec '<spec>' -List`.
   Done when it prints each batch name with its unit count, in run order.

### Per batch

Run the batches one after another in the order step 2 printed them, with no
commit before landing.

3. Delete `Temp/Sweep/Status.txt` and `Temp/sweep-stderr.txt` if present,
   ensure `Temp` exists (not `Temp/Sweep/`), and launch (override default
   `-Batch` parallelism with `'-Throttle','<n>'` inside `Start-Process`'s
   `-ArgumentList` for detached, or `-Throttle <n>` on `pwsh` for async):
   - Detached hosts (Claude Code, >2h limit) — record `<pid>`:

     ```powershell
     (Start-Process pwsh -WindowStyle Hidden -PassThru -RedirectStandardError 'Temp/sweep-stderr.txt' -WorkingDirectory '<worktree root>' -ArgumentList '-NoProfile','-File','.agents/skills/sweep/scripts/Invoke-Sweep.ps1','-Spec','"<spec>"','-Model','<slug>','-Batch','<batch>').Id
     ```

   - Async-task hosts (Antigravity CLI; kills detached children) — run async:

     ```powershell
     pwsh -NoProfile -File .agents/skills/sweep/scripts/Invoke-Sweep.ps1 -Spec '<spec>' -Model '<slug>' -Batch '<batch>'
     ```

   Done when `<pid>` prints or the task starts.
4. Wait until exit, then read `Temp/Sweep/Status.txt` once: for `<pid>`, wait
   until `Get-Process -Id <pid> -ErrorAction SilentlyContinue` returns nothing,
   re-arming on timeout (never advance on `Status.txt` while `<pid>` runs;
   `Invoke-Sweep.ps1` writes a terminal line, not `RUNNING`, only before
   exiting `0`, so `<pid>` checks `Status.txt`, not exit code); for an async
   task, await completion. On refusal/crash — missing/`RUNNING` `Status.txt`,
   or non-zero task exit — surface `Temp/sweep-stderr.txt` or task stderr
   verbatim and stop without retrying. Done on a terminal `Status.txt` line
   after `<pid>` exits or the task exits `0`.
5. Retry steps 3-4 only on `INCOMPLETE` or `PROPAGATE-FAILED` (resumes from
   `Progress.md`, reruns `Failures.txt`). Done on `BATCH-DONE`.
6. If `BATCH-DONE` has `cpp=yes`, dispatch `builder` `/compile` for `builds=`
   (Client/Server Debug, others Release); route failures to `implementer`
   `/resolve-findings`. Done when builds pass or `cpp=no`.

### Stage close

7. Run steps 3-4 replacing `-Batch`/`<batch>` with `-Close`. On
   `CLOSE-FAILED Baseline ...`, surface the refusal and stop without retrying;
   retry only other `CLOSE-FAILED <run>` (finished runs skip). Done on
   `CLOSE-DONE`.
8. If `CLOSE-DONE` has `ordering=` > 0, dispatch `implementer`
   `/resolve-findings` on `Temp/Sweep/Ordering.md` `## Findings` (fix all,
   never ledger). Done when every finding passes.
9. Run the static checks:

   ```powershell
   pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot '<worktree root>' -Baseline <baseline> -IncludeUntracked
   ```

   Done when they pass.
10. When the `CLOSE-DONE` line reads `cpp=yes`, dispatch one `builder` running
    `/compile` for its `builds=` targets: Client and Server in Debug and
    Release, plus Profile when the line reads `profile=yes`; the others in
    Release. Done when every build passes or the line reads `cpp=no`.
11. Report the stage per `## Handoff`, land the changed files (including any
    deferred-fixes record) through `/finalize-changes`, then remove `Temp/Sweep`
    and `Temp/sweep-stderr.txt` if present by exact literal paths. Done when
    `/finalize-changes` and that cleanup complete.

## Handoff

The stage report main gives the user, from the `CLOSE-DONE` line: units swept,
fixes applied, deferred entries, and the deferred-fixes record path the type's
`## Deferred fixes` names.

## Rules

- A sweep skips the Change Workflow; explicit `/sweep <spec path>` approves
  running it, landing still uses `/finalize-changes`'s confirmation, and a new
  sweep requires a clean tracked tree. No `/prepare-change`,
  `/plan-alternatives`, `/plan-audit`, `/plan-simplicity-review`, execution
  card, `/repo-code-review`, `/comment-review` dispatch, `/coherence-review`,
  `/update-claude-docs`, or `/verify-acceptance`. Keep per-batch/stage builds,
  static checks, `/resolve-findings` ordering fixes, and the landing gate.
  Follow any further review or runtime check the specification names, or ask
  the user before step 11 when it names none.
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
- A change that meets the AgentTools trigger follows `/finalize-changes`
  [`agenttools.md`](../finalize-changes/references/agenttools.md).
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
  [`Invoke-Codex.ps1`](../../scripts/Invoke-Codex.ps1)
  ([`codex-bridge.md`](../../references/codex-bridge.md)), because it runs
  detached parallel unit pipelines.

## References

- [scripts/Invoke-Sweep.ps1](scripts/Invoke-Sweep.ps1) — the coordinator; its
  header owns the modes and its body the status lines and `Temp/Sweep/` files.
- [references/types/style-guide.md](references/types/style-guide.md) — the
  style guide rules, user rulings, fix bound, and cleanup checklist.
- [references/types/cpp-adoption.md](references/types/cpp-adoption.md) — a C++
  feature adoption ruled by the specification's `## Sweep rules`.
- [references/types/document.md](references/types/document.md) — an `AGENTS.md`
  or documentation pass.
- [references/prompts/Find.md](references/prompts/Find.md),
  [Fix.md](references/prompts/Fix.md),
  [Propagate.md](references/prompts/Propagate.md),
  [Cleanup.md](references/prompts/Cleanup.md),
  [Ordering.md](references/prompts/Ordering.md) — the Codex prompt templates
  the coordinator fills and runs.
