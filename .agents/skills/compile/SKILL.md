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
- for a BrokenEngineSandbox build, any Local generation or Gaea authorization a
  user-approved plan or acceptance criterion grants, and any deletion-only
  reference-search evidence the caller supplies; routine Shared builds need
  neither — [references/runtime-data-mode.md](references/runtime-data-mode.md)
  owns which authorizations are valid;
- whether an approved plan explicitly requires PREfast verification —
  [references/prefast-mode.md](references/prefast-mode.md).

## Handoff

Return the shared handoff form in
[`subagent-reporting.md`](../../references/subagent-reporting.md),
`## Handoffs`, extended with the build reporting below.

- For a delegated call, return the results after applying the execution and
  result discipline in [`references/worker.md`](references/worker.md). Keep
  overall/per-project status, data mode/path, and decisive blockers visible; the
  bullets below govern what each build carries inline.
- Write every build's captured `broken-engine-build-result/v1` envelope verbatim
  into one `Temp/AgentBuildEnvelopes/` Markdown file unique to this dispatch —
  the directory created beforehand if absent — under a single `##` heading for
  the dispatch with one fenced block per build: the first build's own PowerShell
  call creates that file and heading, and each later build's own call appends
  its block to the same file, each using that call's own captured output rather
  than a new script or a retyped envelope. Read every reported field from the
  envelope, never from scraped terminal text.
- Include a build's envelope verbatim inline as well, unless that build is a
  clean success — `status: success`, `failureKind: none`, `exitCode: 0`,
  `retainedLog.complete: true`, and no `severity: error` diagnostic — for which
  the handoff carries only the fields the bullets below require.
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

- `Decisive checks` — one row per build: target, configuration, status,
  exitCode, failureKind.
- `Evidence` — the `retainedLog.path` per build, and this dispatch's envelope
  file as path plus selector.
- `Residuals` — a failed or skipped required build, or none; last.

`Changed files` and `Build required` are `none` because this skill changes no
tracked file; the envelope file is ignored `Temp/` output.

### Structured build result

`WorktreeCli build` writes exactly one schema-versioned
`broken-engine-build-result/v1` JSON object to stdout; human progress goes to
stderr. The JSON — not scraped terminal text — is the authoritative result.
Capture stdout, parse it, and read:

- `status` (`success`/`fail`), `failureKind` (`none`/`tool`/`msbuild`), and `exitCode` — the process exit code keeps its existing meaning (MSBuild's exit code once launched; `1` for tool failures including a retained-log failure after a successful build).
- `target`/`worktreeRoot` normalized identities, `arguments`, `selectedFiles`, `invalidatedObjects`.
- `lock` outcome (`acquired`/`timeout`/`failed`) with the lock path and waited seconds.
- `msbuild` discovery/launch state and MSBuild's own exit code.
- `retainedLog` — the complete combined MSBuild stdout+stderr stream in observed read order, untruncated, below the invoking worktree's ignored `Temp/AgentBuildLogs/`. `complete: false` or a missing log is a build-result failure, never an omitted side effect.
- `diagnostics` — structured MSBuild error/warning entries (`severity`, `code`, `file`, `line`, `column`, `project`, `message`, `raw`), capped with `diagnosticsTruncated: true` when the raw log holds more; `messages` carries tool failures and unmatched fatal lines.
- `elapsedMilliseconds` and `startedAt`.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Build steps and rules for the
  dispatched worker.
