---
name: compile
description: Builds Broken Engine projects through WorktreeCli's serialized MSBuild driver and governs immutable prebuilt AgentTools bootstrap/maintenance policy. Use whenever you need to build, rebuild, compile, or check for compile/link errors in ThirdParty, DataPacker, AgentHarness, WorktreeCli, ModelViewer, ServerMonitor, or a game project such as BrokenEngineSandbox (client or server).
allowed-tools: [PowerShell]
---

# Build

## Purpose

Builds requested targets via WorktreeCli and returns each build's structured
result in one delegated `builder`; a dispatch assigned an execution-card
acceptance check whose steps interleave file edits with builds runs these builds
itself.

## When to use

- Any request to build, rebuild, compile, or check for compile/link errors in
  ThirdParty, DataPacker, AgentHarness, WorktreeCli, ModelViewer, ServerMonitor,
  or a game project under `Projects/` (client or server).
- PREfast verification runs only when an approved plan explicitly requires it;
  never infer PREfast authorization from a routine compile, rebuild, or
  link-error check.
- Clang-tidy verification runs only when an approved plan explicitly requires
  a clang-tidy run; never infer it from a routine compile, rebuild, or
  link-error check.
- Not for AgentTools bootstrap or promotion.
- When the changed set contains a non-Markdown path under `Tools/WorktreeCli/`,
  `Tools/AgentHarness/`, or `Tools/ToolCommon/`, or `Common/Sha256Hasher.h`, the
  rebuilt tools are promoted through `/finalize-changes`, which owns that
  promotion and bootstrap policy —
  [`agenttools.md`](../finalize-changes/references/agenttools.md).

## Inputs

Consumes the task-brief fields in
[`subagent-reporting.md`](../../references/subagent-reporting.md) plus these
skill-specific inputs:

- the targets and configurations to build, and any selective `.cpp` file list.
  Agents build DataPacker, WorktreeCli, AgentHarness, and ServerMonitor in
  Release only, and the build script refuses any other configuration for them.
  Agent tooling runs only the Release executables, since each Debug build is
  named `<Project>.Debug.exe`. Those Debug configurations are for humans
  debugging the tools, and DataPacker's Debug export is very slow. ThirdParty
  builds run only in the primary checkout, per
  [`ThirdParty/Prebuilts/Platforms/VisualStudio2026/AGENTS.md`](../../../ThirdParty/Prebuilts/Platforms/VisualStudio2026/AGENTS.md)
  `## Consumer Provisioning`: from any other checkout `-Target ThirdParty` is
  blocked, so never request it from a session worktree;
- optionally, the game project (`Projects/<Name>`) a client or server build
  targets. With one game project none is needed; with several and none named,
  a game build request builds every game project in turn;
- ModelViewer builds require Debug or Release x64 and always use
  BrokenEngineSandbox as their data owner; they do not add another game project.
  The worker's target routing and the runtime-data reference govern invocation;
- the repository root, primary checkout, and baseline the caller explicitly
  supplied, or none;
- whether the session's approved plan or acceptance table includes an
  agent-harness scenario; a delegator requesting the build states this trigger,
  counting any acceptance criterion or check settled by running `/agent-harness`
  as such a scenario. An included agent-harness scenario adds the client and
  the server of every game built to the targets, so name both. One that
  captures the ModelViewer adds ModelViewer Debug x64, per
  [`model-viewer.md`](../agent-harness/references/model-viewer.md) `## Capture`;
- for a game build, any Gaea authorization (`-AllowGaeaExport`) that a
  user-approved plan or acceptance criterion requiring an island re-bake
  grants; the build resolves the data mode itself —
  [references/runtime-data-mode.md](references/runtime-data-mode.md)
  `## Mode selection` and `## Local generation`;
- whether an approved plan explicitly requires PREfast verification —
  [references/prefast-mode.md](references/prefast-mode.md)
  `## Authorization and when to use`;
- whether an approved plan explicitly requires a clang-tidy run —
  [references/clang-tidy-mode.md](references/clang-tidy-mode.md)
  `## Authorization and when to use`.

A dispatch that runs this skill under the `## Purpose` exception receives these
inputs for its builds in its own brief.

## Handoff

Return the shared handoff form in
[`subagent-handoff.md`](../../references/subagent-handoff.md),
`## Handoffs`, extended with the rows below and nothing else, after applying
the execution and result discipline in
[`references/worker.md`](references/worker.md).

- `Decisive checks` — one success row, whether or not any build fails: the
  count of successful builds, then each one as
  `target|configuration[|game for a client or server build]`, with no per-build
  status fields.
- `Decisive checks` — one row per failing build: target, configuration, the
  game for a client or server build, `status`, `exitCode`, `failureKind`, and,
  with a `severity: error` diagnostic, the first one's `code` and `file`.
- `Decisive checks` — for a game build, one row naming the game and the
  envelope's `/p:DataBuildMode`, `/p:RunDataPacker`, and `/p:GameDataDirectory`
  switch values.
- `Decisive checks` — for a `-ClangTidy` build, one row naming whether analysis
  executed, the de-duplicated warning count, and the `clang-diagnostic-error`
  parse-error count with the first unparsable file.
- `Evidence` — one row for this dispatch's envelope file as path plus `##`
  selector; that file holds each build's envelope block and its `retainedLog`.
- `Residuals` — one row per failed or skipped required build, counting
  `complete: false` as a failure; `none` when absent.

A `builder`'s `Changed files` lists the tracked paths the builds newly
modified, as `scripts/Get-CompileRewrittenPaths.ps1 -Compare` prints them
(`references/worker.md` step 9), or `none`; its `Build required` is `none`.
When either run of that script exits non-zero, the `builder` returns
`Status: BLOCKED` with a `Residuals` row naming that run's failure and stderr,
because its `Changed files` is then unknown. The envelope and snapshot
files are ignored `Temp/` output.

When an execution-card acceptance check has its assigned dispatch run this
skill inside that dispatch's own task, as `## Purpose` routes, the decisive
field is the build's entry in the success row above, or its failing-build row.
That dispatch adds one row per build under its own shared `Decisive checks` —
whether the expected observation was met, that run's `Status`, and the build's
success-row entry or failing-build row — plus one row summarizing the resulting
working-tree state, never pasting `git status --porcelain` output inline. It
writes every build's complete handoff, not only past the character cap, to one
gitignored `Temp/` file with one `## Run <n>` heading per build, by the overflow
rule in
[`subagent-handoff.md`](../../references/subagent-handoff.md) `## Handoffs`;
that handoff counts as that build's complete `/compile` handoff for landing. A
running skill whose own `## Handoff` declares a field for the run's result puts
that run's row there instead of under `Decisive checks`; one that also places
each run's complete handoff follows its own placement instead of those
per-build `Decisive checks` rows and this `Temp/` file. The dispatch reports
its own `Changed files` and `Build required` instead of the `builder` values
above; an `/implement-plan` slice keeps returning `Build required`.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Build steps and rules for the
  dispatched worker.
