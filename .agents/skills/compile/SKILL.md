---
name: compile
description: Builds Broken Engine projects through WorktreeCli's serialized MSBuild driver and governs immutable prebuilt AgentTools bootstrap/maintenance policy. Use whenever you need to build, rebuild, compile, or check for compile/link errors in ThirdParty, DataPacker, AgentHarness, WorktreeCli, or BrokenEngineSandbox (client or server).
allowed-tools: [PowerShell]
---

# Build

Run solely inside one delegated `builder`; separate-role requirements return to
the manager.

Builds through the current checkout's WorktreeCli executable. `WorktreeCli build` serializes writers per target basename inside the current worktree, waits up to 500 seconds, preserves native Windows argument boundaries, and owns MSBuild through a kill-on-close Job Object.

## Structured build result

`WorktreeCli build` writes exactly one schema-versioned `broken-engine-build-result/v1` JSON object to stdout; human progress goes to stderr. The JSON — not scraped terminal text — is the authoritative result. Capture stdout, parse it, and read:

- `status` (`success`/`fail`), `failureKind` (`none`/`tool`/`msbuild`), and `exitCode` — the process exit code keeps its existing meaning (MSBuild's exit code once launched; `1` for tool failures including a retained-log failure after a successful build).
- `target`/`worktreeRoot` normalized identities, `arguments`, `selectedFiles`, `invalidatedObjects`.
- `lock` outcome (`acquired`/`timeout`/`failed`) with the lock path and waited seconds.
- `msbuild` discovery/launch state and MSBuild's own exit code.
- `retainedLog` — the complete combined MSBuild stdout+stderr stream in observed read order, untruncated, below the invoking worktree's ignored `Temp/AgentBuildLogs/`. `complete: false` or a missing log is a build-result failure, never an omitted side effect.
- `diagnostics` — structured MSBuild error/warning entries (`severity`, `code`, `file`, `line`, `column`, `project`, `message`, `raw`), capped with `diagnosticsTruncated: true` when the raw log holds more; `messages` carries tool failures and unmatched fatal lines.
- `elapsedMilliseconds` and `startedAt`.

## AgentTools trigger

The authoritative executables are `Tools\WorktreeCli\Platforms\VisualStudio2026\Output\WorktreeCli.exe` and `Tools\AgentHarness\Platforms\VisualStudio2026\Output\AgentHarness.exe` under the resolved repository root. Routine work consumes these immutable primary Outputs and never builds into or writes through them.

Every invocation that clears the parameter, context, and data-mode authorization guards runs `.agents/scripts/Provision-WorktreeThirdParty.ps1` against the derived root and then requires both executables before anything else happens; a nonzero provisioning exit is the typed block `provisioning.failed`, and a missing executable is the typed block `agent-tools.missing`. Never run provisioning or the existence checks by hand as a separate agent command — the build entry point owns both.

If either executable is missing, stop: wrappers own bootstrap. When the changed
set contains a non-Markdown path under `Tools/WorktreeCli/`,
`Tools/AgentHarness/`, or `Tools/ToolCommon/`, the rebuilt tools are promoted
through `/finalize-changes`' AgentTools promotion — waiting until there is no
activity, plus backup/rollback under the shared mutex — which also owns bootstrap policy. This
compile run only builds; it never promotes or copies tools. An agent changing
shared tool infrastructure pauses and warns the user only if the change could
cause problems for other live worktrees.

## Determine what to build

Read identities, the changed-path set, and the data-mode directories as judgment evidence — which targets to build, and whether your own judgment overrides the reported data mode — by running the repository-owned read-only script. The build entry point re-resolves the same context internally on every call, so this run informs your decision and never feeds values into a later command:

```powershell
pwsh -NoProfile -File .agents/skills/compile/scripts/Resolve-CompileContext.ps1
```

Append `-RepositoryRoot`/`-PrimaryCheckout`/`-Baseline` only when the caller explicitly supplied that input; an empty or unset value counts as not supplied, so omit the parameter and let the env-hint/derived-default fallback apply. Pass the same explicitly supplied values to the build invocation. `-IncludeDevEnvDir` is not an agent-run option: the build entry point requests DevEnvDir itself for an authorized Local generation build.

The script writes exactly one `broken-engine-compile-context/v1` JSON object to stdout; human diagnostics go to stderr. Each input takes an explicitly supplied parameter first, then the wrapper environment hint (`BROKEN_ENGINE_WORKTREE_PATH`, `BROKEN_ENGINE_PRIMARY_CHECKOUT`, `BROKEN_ENGINE_BASELINE`), then the derived default. Environment values are wrapper-provided identity hints, so the `BROKEN_ENGINE_BASELINE` hint resolves through the session context and may advance to the session's real divergence point, while an explicitly supplied baseline never moves. Read from the result:

- `status`/`code`/`message` with exit `0` pass, `2` structured blocked, `1` internal error. Any nonzero exit stops the build: report the exact `code` and `message`.
- `repositoryRoot`, `primaryCheckout`, and `baseline` (the resolved commit) — an explicitly supplied baseline is authoritative and is used exactly as given; a `BROKEN_ENGINE_BASELINE` hint in a session worktree resolves through the session context, which may advance it to the session's real divergence point, and falls back to the hint itself whenever that resolution fails.
- `changedPaths`/`changedPathCount` — the complete changed-path set: the single baseline diff (committed, staged, and unstaged tracked changes) plus untracked files, separator-normalized. `changedPathsTruncated`, `triggerMatchesTruncated`, `deletionOnlyCandidatesTruncated`, and code `output.capacity-exceeded` are blocking conditions, never a partial answer to work from.
- `triggerMatches` — each changed path with the Local path trigger it matched.
- `deletionOnlyCandidates` — trigger-matching changed paths whose baseline diff status is a deletion, with rename sides excluded. Evidence for the deletion-only exception in [references/runtime-data-mode.md](references/runtime-data-mode.md), never its decision.
- `dataBuildMode` with `dataBuildModeDerivation` `path-rules-only`, plus `gameDataDirectory` and `generatedDataIncludeRoot`.

Never reconstruct these operations inline: no hand-typed changed-path `git diff`/`git ls-files` commands, no hand-resolved repository root, primary checkout, or baseline, and no inline `vswhere`/`DevEnvDir` discovery. A blocked result is a stop, not a cue to redo its work by hand.

- Default: BrokenEngineSandbox client Debug.
- If any changed file is shared (`Common/`, `Engine/`, or non-exclusive game code), build both client and server.
- If the session's approved plan or acceptance table includes an agent-harness scenario, build both client and server in the same request regardless of changed-file affinity — the harness launches both executables and hard-stops when either is missing. A delegator requesting the build states this trigger.
- ThirdParty builds only on explicit request in agent-driven `/compile`; missing source or library links are provisioning failures and a routine `/compile` never rebuilds ThirdParty itself. Wrapper bootstrap does incremental-rebuild ThirdParty at every session start (see Bootstrap AgentTools), so a routine `/compile` normally finds it already current.
- DataPacker builds Release only. WorktreeCli still supplies the normal worktree-local target serialization. Worktree session start seeds the worktree's DataPacker Release Output by verified copy from the primary bootstrap prebuild when the worktree's clean `DataPacker/`/`Common/`/`ThirdParty/` trees match the stamp, and otherwise builds it locally (`.agents/scripts/Build-WorktreeDataPacker.ps1`), so `DataPacker.exe` is already present at session start; a session that changes DataPacker-relevant sources performs a full local rebuild on its first DataPacker build. When that prebuild is stamped, bootstrap also runs the primary's DataPacker over the primary checkout's current asset inputs into the primary `Output\Data`, refreshing the data Shared-mode worktrees consume, first waiting up to its `-WaitSeconds` budget for any `BrokenEngineSandbox*` process to exit and skipping the run with a warning only if one is still alive when that budget expires; those inputs are not checked against HEAD, and a nonzero exit fails session start. DataPacker's `"BrokenEngineDataPacker"` mutex and the AgentTools bootstrap mutex are the two PC-global build coordination points; add no others (the seed copy reuses the bootstrap mutex for its prebuild). They now nest: bootstrap holds its own mutex while that run waits for `"BrokenEngineDataPacker"`, and that wait is deliberately unbounded, so a peer DataPacker run or a rare island bake can block session start long enough to time other session starts out.

The build entry point provisions the worktree itself before every build and stops on failure. Validated stable primary submodule trees plus shared immutable ThirdParty, WorktreeCli, and AgentHarness Output directories are the only exceptions to worktree-local build artifacts. When this session holds the harness lock, send `quit` with its own owner token and wait for its own retained exact PIDs before building — a live executable locks its image. When a target executable is live under a process this session cannot prove it owns, stop and report contention; never quit or stop it (`/agent-harness`, Ownership and takeover). Do not discover this as a link error.

For routine work, build the checkout supplied by the caller. An isolated session worktree remains appropriate for queue operations, concurrent work, or a landing gate, but is not a prerequisite for a targeted build. Keep existing build serialization and the gated AgentTools promotion path; do not share mutable build output between checkouts (the session-start DataPacker seed is a one-time verified copy the worktree then owns and may rebuild over, not shared output).

For a delegated call, return the complete build result inline as Report results
below requires. A `builder` executing this skill runs the build itself and never
dispatches another agent.

DataPacker's mutex coordinates across worktrees and its shared chunks live under `%LOCALAPPDATA%\BrokenEngine\DataPackerCache\<Project>`; do not add another PC-global DataPacker lock or a checkout-local cache copy (the session-start seed copies the built `DataPacker.exe` once into the worktree's own Output — a verified artifact seed, not a shared chunk cache). Gaea raw and split intermediates use the single mutable `%LOCALAPPDATA%\BrokenEngine\DataPackerCache\<Project>\Gaea\Islands` cache; source-tree island leaves retain only tracked BC outputs.

### Execution and result discipline

Run each build invocation synchronously in the foreground and remain in-turn until its process exit code and single JSON result are captured. Give the call the maximum available execution timeout so the 500-second target-lock wait is not preempted. Never use `Start-Job`, a trailing `&`, a fire-and-forget watcher, or end a delegated turn while a build is running. If the host call times out while a build without `-RunDataPacker` continues, re-invoke the identical command; target serialization and incremental tlogs carry it to completion. A `-RunDataPacker` invocation materializes Local data and rewrites the oracle state before it reaches that serialization, so first confirm the prior invocation's process has ended — its shell call returned, or the process is provably gone — and only then re-invoke the identical command. A generation interrupted mid-run leaves the completion flag false and the persisted receipt no longer matching the Data directory, and the authorized retry regenerates and replaces that receipt; a retry that finds its authorized generation already complete for the same inputs downgrades itself to an ordinary Local build instead of regenerating. When a blocking call is unavailable, poll the same invocation to completion in-turn.

Read the outcome from the schema name on stdout, never from the exit code alone:

- `broken-engine-build-result/v1` — WorktreeCli ran and its envelope is passed through byte-verbatim, with its own exit code preserved.
- `broken-engine-compile-invoke-result/v1` — the build never ran. `status`/`code`/`message` name the block; exit `2` is a structured block (parameter contract, context, provisioning, missing AgentTools or target, data-mode authorization, oracle state) and exit `1` is an internal error. Report the exact `code` and `message` and stop; never present it as a build result.
- Exit `4` — the build ran and its envelope is on stdout, but post-build oracle work (verification, receipt issuance, or state persistence) failed; stderr names the failure, because stdout already carries the single build envelope. A failed verification marks the receipt breached in the state file, and every later build fails closed until the authorized clearing transition runs. Treat it as a failed build.

Both envelopes are guaranteed only from successful parameter binding onward. A value the parameter contract rejects at binding time — an unlisted `-Target`, `-Configuration`, or `-DataBuildMode` value — fails before the script runs, so PowerShell's own parameter error goes to stderr with a nonzero exit and stdout stays empty; fix the invocation rather than looking for a typed `code`.

Parse and report only the structured result described above after every requested target has returned. Ordinary builds do not run DataPacker, Gaea, or texture export; the authorized Local-generation path and the wrapper bootstrap's primary run are the only exceptions. Every invocation passes `EnableClangTidyCodeAnalysis=false` and `RunCodeAnalysis=false`; `-Prefast` is the sole exception to `RunCodeAnalysis=false`. VS2026 clang-tidy crashes on this codebase.

## Select runtime data mode

Every build targeting a BrokenEngineSandbox project — client or server, full or selective, whichever source files a selective compile lists — must read [references/runtime-data-mode.md](references/runtime-data-mode.md) and select the runtime data mode before building; that reference is mandatory reading in that case, not optional background, and it maps every mode, authorization, and oracle rule onto the build invocation's parameters and the oracle identities the report requires. Only builds targeting a non-game project (ThirdParty, DataPacker, WorktreeCli, AgentHarness) read neither reference.

## Explicit Microsoft PREfast verification mode

PREfast verification runs only when an approved plan explicitly requires it, and in that case reading [references/prefast-mode.md](references/prefast-mode.md) before building is mandatory, not optional background; otherwise that reference stays unloaded, and never infer PREfast authorization from a routine compile, rebuild, or link-error check.

## Full-build commands

Every build runs through one bundled script, one target per call. Each call is
independently valid in a fresh shell: it re-derives identities, data mode,
directories, and tool paths itself, so no agent command ever sets a variable,
composes an MSBuild `/p:` switch, or carries state between shell calls.

```powershell
# ThirdParty: choose requested configuration (Debug, Profile, or Release).
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target ThirdParty -Configuration Debug

# DataPacker: Release only, and Release is the default.
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target DataPacker

# BrokenEngineSandbox client, then server. Substitute Profile/Release only when requested.
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target Client -Configuration Debug
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target Server -Configuration Debug

# AgentTools: Release candidates under the session-local ignored Temp tree.
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target WorktreeCli
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target AgentHarness
```

Parameters:

- `-Target` — `ThirdParty`, `DataPacker`, `Client`, `Server`, `WorktreeCli`, or `AgentHarness`.
- `-Configuration` — `Debug`, `Profile`, or `Release`; required for ThirdParty, Client, and Server. DataPacker, WorktreeCli, and AgentHarness are Release only, so omit it there; an explicit non-Release value on those targets is a typed block.
- `-Files` — selective compile, Client/Server only; see [Selective file compile](#selective-file-compile).
- `-DataBuildMode`, `-RunDataPacker`, `-AllowGaeaExport`, `-AcceptDeletionOnlyException`, `-ReissueSharedReceipt` — Client/Server only; [references/runtime-data-mode.md](references/runtime-data-mode.md) owns when each one is authorized.
- `-Prefast` — Client/Server Release only; see [references/prefast-mode.md](references/prefast-mode.md).
- `-RepositoryRoot`, `-PrimaryCheckout`, `-Baseline` — pass only what the caller explicitly supplied, exactly as in the context script above.

The AgentTools targets build into a session-local `OutDir` because the default
Output is the shared immutable primary Output held by the running WorktreeCli
driver; a default-output WorktreeCli build therefore fails with a structural
LNK1104. They produce `Temp\AgentToolsCandidate\WorktreeCli.exe` and
`Temp\AgentToolsCandidate\AgentHarness.exe` under the repository root for
AgentTools promotion.

## Selective file compile

`-Files` invalidates specific project-member `.cpp` objects before a normal project build of the matching `.vcxproj`. Pass every path in one comma-separated quoted argument — repo-relative or absolute — so the documented `-File` invocation form carries the whole list:

```powershell
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target Client -Configuration Debug -Files 'Engine/Source/Example.cpp,Projects/BrokenEngineSandbox/Source/Example.cpp'
```

Only `.cpp` inputs already present in the target project are valid. After a header edit, list the affected project-member `.cpp` files. Add new files to the vcxproj/filters before compiling. Shader changes require Local output generated through the authorized first-build path in [references/runtime-data-mode.md](references/runtime-data-mode.md) or prepared explicitly before the client build.

## Report results

- For a delegated call, return the complete results inline after applying the execution/result discipline above. Keep overall/per-project status, data mode/path, and decisive blockers visible.
- Include every build's captured `broken-engine-build-result/v1` envelope verbatim in the handoff, and read every reported field from it, never from scraped terminal text.
- Final status per project: `status` plus `exitCode` and `failureKind`.
- Every `severity: error` diagnostic's `raw` line verbatim, plus all `messages` entries; note `diagnosticsTruncated: true` and point at the retained log for the remainder.
- `severity: warning` diagnostics' `raw` lines verbatim only for files involved in the change.
- The exact `retainedLog.path` for each build, and `complete: false` as a failure.
- A WorktreeCli default-output `LNK1104` is structural because the running driver holds the shared primary Output; use the AgentTools targets in [Full-build commands](#full-build-commands). Other `LNK1104`, `LNK1168`, or EXE `LNK2019` failures can mean a live target process still holds the executable; report them rather than diagnosing unless asked.
- A prior killed build's `unsuccessfulbuild` marker clears on the next successful run; rerun instead of deleting tlogs.
- A lock timeout means another WorktreeCli build still owns that target. Retry after it finishes; never delete `.claude/build-locks/` manually.
- For game builds, report `DataBuildMode`, the `RunDataPacker` value for every build, normalized `GameDataDirectory`, normalized `GeneratedDataIncludeRoot`, and each selected oracle's exact receipt path, SHA-256, Data path, mode, baseline, and aggregate digest. Read all of these from the invocation's own stderr summary lines, which the script emits from the persisted `Temp/compile-oracle-state.json` identities and the oracle scripts' typed results; never reconstruct them. In Local mode also report the independent oracle for the primary Shared data. Report every mode-selection trigger, the Local prepared-data or generation-authorization trigger, any `RunDataPacker` downgrade the summary states, authorized content-delta outcome, whether the Gaea guard was applied (or the exact explicit Gaea-regeneration authorization), and all post-build oracle verification results. A harness run must consume these exact identities; it must not infer, substitute, or compare Shared and Local for equality.

End with:

```text
Files changed: none
Functions/regions touched: none
Residuals:
- <failed or skipped required build, or none>
```
