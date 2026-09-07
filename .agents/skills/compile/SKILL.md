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
  agent-harness scenario; a delegator requesting the build states this trigger;
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
[`subagent-reporting.md`](../../references/subagent-reporting.md),
`## Handoffs`, extended with the build reporting below.

- For a delegated call, return the results after applying the execution and
  result discipline in [`references/worker.md`](references/worker.md). Keep
  overall/per-project status, data mode/path, and decisive blockers visible; the
  bullets below govern what each build carries inline.
- Every build's captured `broken-engine-build-result/v1` envelope is recorded
  verbatim in this dispatch's envelope file under `Temp/AgentBuildEnvelopes/`.
  Read every reported field from the envelope, never from scraped terminal text.
- Final status per project: `status` plus `exitCode` and `failureKind`.
- Every `severity: error` diagnostic's `raw` line verbatim, plus all `messages`
  entries; note `diagnosticsTruncated: true` and point at the retained log for
  the remainder.
- `severity: warning` diagnostics' `raw` lines verbatim only for files involved
  in the change.
- The exact `retainedLog.path` for each build, and `complete: false` as a
  failure.
- For game builds, report `DataBuildMode`, the `RunDataPacker` value for every
  build, normalized `GameDataDirectory`, and normalized
  `GeneratedDataIncludeRoot`. Read all of these from the invocation's own stderr
  summary lines; never reconstruct them. Report every mode-selection trigger,
  the Local generation-authorization trigger, and whether the Gaea guard was
  applied (or the exact explicit Gaea-regeneration authorization).

That shared form's other fields stay as it defines them; these are narrowed
here:

Each shared `Decisive checks` row names the build target, configuration, status,
exitCode, and failureKind. Shared `Evidence` carries the `retainedLog.path` for
each build and this dispatch's envelope file as path plus selector. Each shared
`Residuals` row names a failed or skipped required build; use `none` when
absent.

`Changed files` and `Build required` are `none` because this skill changes no
tracked file; the envelope file is ignored `Temp/` output.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Build steps and rules for the
  dispatched worker.
