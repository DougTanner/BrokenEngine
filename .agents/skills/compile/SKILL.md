---
name: compile
description: Builds Broken Engine projects through WorktreeCli's serialized MSBuild driver and governs immutable prebuilt AgentTools bootstrap/maintenance policy. Use whenever you need to build, rebuild, compile, or check for compile/link errors in ThirdParty, DataPacker, AgentHarness, WorktreeCli, or BrokenEngineSandbox (client or server).
allowed-tools: [PowerShell]
---

# Build

## Purpose

Builds the requested targets through the current checkout's WorktreeCli
executable and returns each build's structured result. Runs solely inside one
delegated `builder`; separate-role requirements return to the manager.

## When to use

- Any request to build, rebuild, compile, or check for compile/link errors in
  ThirdParty, DataPacker, AgentHarness, WorktreeCli, or BrokenEngineSandbox
  (client or server).
- PREfast verification runs only when an approved plan explicitly requires it;
  never infer PREfast authorization from a routine compile, rebuild, or
  link-error check.
- Not for AgentTools bootstrap or promotion. The authoritative executables are
  `Tools\WorktreeCli\Platforms\VisualStudio2026\Output\WorktreeCli.exe` and
  `Tools\AgentHarness\Platforms\VisualStudio2026\Output\AgentHarness.exe` under
  the resolved repository root.
- When the changed set contains a non-Markdown path under `Tools/WorktreeCli/`,
  `Tools/AgentHarness/`, or `Tools/ToolCommon/`, the rebuilt tools are promoted
  through `/finalize-changes`, which owns that promotion and bootstrap policy —
  [`agenttools.md`](../finalize-changes/references/agenttools.md).

## Inputs

Consumes the task-brief fields in
[`subagent-reporting.md`](../../references/subagent-reporting.md) plus these
skill-specific inputs:

- the targets and configurations to build, and any selective `.cpp` file list;
- the repository root, primary checkout, and baseline the caller explicitly
  supplied, or none;
- whether the session's approved plan or acceptance table includes an
  agent-harness scenario; a delegator requesting the build states this trigger,
  counting any acceptance criterion or check settled by running `/agent-harness`
  as such a scenario;
- for a BrokenEngineSandbox build, the resolved data mode, carried with the
  Local generation authorization a user-approved plan or acceptance
  criterion grants, the deletion-only reference-search evidence, or the stated
  basis for Shared, plus any Gaea authorization the same plan or criterion
  grants — [references/runtime-data-mode.md](references/runtime-data-mode.md)
  owns which authorizations are valid;
- whether an approved plan explicitly requires PREfast verification —
  [references/prefast-mode.md](references/prefast-mode.md).

For a BrokenEngineSandbox build, resolve that data mode before writing the
brief, with one read-only run of the resolver from the worktree root —
`pwsh -NoProfile -File .agents/skills/compile/scripts/Resolve-CompileContext.ps1`,
adding `-RepositoryRoot`, `-PrimaryCheckout`, or `-Baseline` only for an input
the caller explicitly supplied. Its `dataBuildMode` is the path-rule answer;
[references/runtime-data-mode.md](references/runtime-data-mode.md) owns the
remaining judgment triggers, which can still select Local when the script
reports Shared. A Local mode needs that reference's Local generation
authorization present in the brief before the build may generate. The worker
re-resolves the same context itself, so the brief carries the mode and its
authorization, never the resolver's JSON.

## Handoff

Return the shared handoff form in
[`subagent-handoff.md`](../../references/subagent-handoff.md),
`## Handoffs`, extended with the rows below and nothing else, after applying
the execution and result discipline in
[`references/worker.md`](references/worker.md).

- `Decisive checks` — one row per build: target, configuration, `status`,
  `exitCode`, `failureKind`, and, for a failing build with a `severity: error`
  diagnostic, the first one's `code` and `file`.
- `Decisive checks` — for a game build, one row only when the resolved data
  mode differs from the mode the dispatch brief fixed, naming the brief's mode,
  the resolved mode as the envelope's `/p:DataBuildMode` switch, and, when your
  own mode selection overrode the brief, your reason for it. Report nothing
  about the data mode when the two match; the effective mode, the
  `RunDataPacker` value, and both normalized directories already reach the
  caller as the `/p:` switches in each envelope block's `arguments`.
- `Evidence` — one row per build carrying its `retainedLog.path`, plus one row
  for this dispatch's envelope file as path plus `##` selector.
- `Residuals` — one row per failed or skipped required build, counting
  `complete: false` as a failure; `none` when absent.

`Changed files` and `Build required` are `none` because this skill changes no
tracked file; the envelope file is ignored `Temp/` output.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Build steps and rules for the
  dispatched worker.
