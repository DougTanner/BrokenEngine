# Build Worker

Build steps and rules for the worker dispatched with
[`../SKILL.md`](../SKILL.md), which owns the purpose, the triggers, the inputs,
and the handoff this run returns.

## Contents

- [Steps](#steps)
- [Rules](#rules)

## Steps

1. Resolve identities, the changed-path set, and the data-mode directories by
   running the repository-owned read-only script. Done when exactly one
   `broken-engine-compile-context/v1` JSON object is captured from its stdout;
   human diagnostics go to stderr.

```powershell
pwsh -NoProfile -File .agents/skills/compile/scripts/Resolve-CompileContext.ps1
```

   - Append `-RepositoryRoot`/`-PrimaryCheckout`/`-Baseline` only when the
     caller explicitly supplied that input; an empty or unset value counts as
     not supplied, so omit the parameter and let the env-hint/derived-default
     fallback apply. Pass the same explicitly supplied values to the build
     invocation.
   - `-IncludeDevEnvDir` is not an agent-run option: the build entry point
     requests DevEnvDir itself for an authorized Local generation build.

2. Read that result as judgment evidence — which targets to build, and whether
   your own judgment overrides the reported data mode. Done when every field
   below has been read and no blocking condition is present.

   - `status`/`code`/`message` with exit `0` pass, `2` structured blocked, `1`
     internal error. Any nonzero exit stops the build: report the exact `code`
     and `message`.
   - `repositoryRoot`, `primaryCheckout`, and `baseline` (the resolved commit) —
     an explicitly supplied baseline is authoritative and is used exactly as
     given; without one, a session worktree resolves its baseline through the
     session context, which may advance it to the session's real divergence
     point, and falls back to `HEAD` whenever that resolution fails.
   - `changedPaths`/`changedPathCount` — the complete changed-path set: the
     single baseline diff (committed, staged, and unstaged tracked changes) plus
     untracked files, separator-normalized. `changedPathsTruncated`,
     `triggerMatchesTruncated`, `deletionOnlyCandidatesTruncated`, and code
     `output.capacity-exceeded` are blocking conditions, never a partial answer
     to work from.
   - `triggerMatches` — each changed path with the Local path trigger it
     matched.
   - `deletionOnlyCandidates` — trigger-matching changed paths whose baseline
     diff status is a deletion, with rename sides excluded. Evidence for the
     deletion-only exception in [runtime-data-mode.md](runtime-data-mode.md),
     never its decision.
   - `dataBuildMode` with `dataBuildModeDerivation` `path-rules-only`, plus
     `gameDataDirectory` and `generatedDataIncludeRoot`.
   - `recommendedTargets` — advisory `{project, configuration, reason}` rows
     derived from the changed paths.

3. Fix the target and configuration list from that evidence. Done when every
   target this request will build is named.

   - Start from `recommendedTargets`, which is advisory: a `BrokenEngineSandbox`
     row is `-Target Client` and a `BrokenEngineSandboxServer` row is
     `-Target Server`, but you still fix the final list, because the rules below
     depend on session state and request inputs the script cannot see.
   - If the session's approved plan or acceptance table includes an
     agent-harness scenario, build both client and server in the same request
     regardless of changed-file affinity — the harness launches both executables
     and hard-stops when either is missing.
   - ThirdParty builds run only in the primary checkout, per Consumer
     Provisioning in
     `ThirdParty/Prebuilts/Platforms/VisualStudio2026/AGENTS.md`; from any other
     checkout `-Target ThirdParty` is the typed block
     `thirdparty.worktree-not-primary`. Missing source or library links are
     provisioning failures and a routine `/compile` never rebuilds ThirdParty
     itself. Wrapper bootstrap does incremental-rebuild ThirdParty at every
     session start (see Bootstrap AgentTools), so a routine `/compile` normally
     finds it already current.
   - DataPacker builds Release only. WorktreeCli still supplies the normal
     worktree-local target serialization.

4. Clear any live target executable before building. Done when no target
   executable is held by a process, or the run stops with contention reported.

   - When this session holds the harness lock, send `quit` with its own owner
     token and wait for its own retained exact PIDs before building — a live
     executable locks its image.
   - When a target executable is live under a process this session cannot prove
     it owns, stop and report contention; never quit or stop it
     (`/agent-harness`, Ownership and takeover). Do not discover this as a link
     error.

5. Select the runtime data mode for every build targeting a BrokenEngineSandbox
   project. Done when [runtime-data-mode.md](runtime-data-mode.md) has been read
   and its modes and authorization rules are mapped onto the build invocation's
   parameters.

   - Client or server, full or selective, whichever source files a selective
     compile lists.
   - That reference is mandatory reading in that case, not optional background.
   - Only builds targeting a non-game project (ThirdParty, DataPacker,
     WorktreeCli, AgentHarness) read neither reference.

6. Read [prefast-mode.md](prefast-mode.md) before building when an approved plan
   explicitly requires PREfast verification. Done when that reference has been
   read, or it stays unloaded because no such authorization exists.

7. Compose one bundled-script invocation per target. Done when the exact command
   is chosen without any shell variable, composed MSBuild `/p:` switch, or state
   carried between shell calls.

   - Each call is independently valid in a fresh shell because it re-derives
     identities, data mode, directories, and tool paths itself.

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

   - `-Target` — `ThirdParty`, `DataPacker`, `Client`, `Server`, `WorktreeCli`,
     or `AgentHarness`.
   - `-Configuration` — `Debug`, `Profile`, or `Release`; required for
     ThirdParty, Client, and Server. DataPacker, WorktreeCli, and AgentHarness
     are Release only, so omit it there; an explicit non-Release value on those
     targets is a typed block.
   - `-Files` — selective compile, Client/Server only; see step 8.
   - `-DataBuildMode`, `-RunDataPacker`, `-AllowGaeaExport`,
     `-ForbidExpensiveExport`, `-AcceptDeletionOnlyException` — Client/Server
     only; [runtime-data-mode.md](runtime-data-mode.md) owns when each one is
     authorized.
   - `-Prefast` — Client/Server Release only; see
     [prefast-mode.md](prefast-mode.md).
   - `-RepositoryRoot`, `-PrimaryCheckout`, `-Baseline` — pass only what the
     caller explicitly supplied, exactly as in step 1.

8. Add `-Files` when only specific project-member `.cpp` objects need
   invalidating before a normal project build of the matching `.vcxproj`. Done
   when every path is passed in one comma-separated quoted argument so the
   documented `-File` invocation form carries the whole list.

```powershell
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target Client -Configuration Debug -Files 'Engine/Source/Example.cpp,Projects/BrokenEngineSandbox/Source/Example.cpp'
```

   - Each path is repo-relative or absolute.
   - Only `.cpp` inputs already present in the target project are valid.
   - After a header edit, list the affected project-member `.cpp` files.
   - Add new files to the vcxproj/filters before compiling.
   - Shader changes require Local output generated through the authorized
     first-build path in [runtime-data-mode.md](runtime-data-mode.md) or
     prepared explicitly before the client build.

9. Run each build invocation synchronously in the foreground and remain in-turn
   until its process exit code and single JSON result are captured. Done when
   every requested target has returned that pair.

   - Give the call the maximum available execution timeout so the 500-second
     target-lock wait is not preempted. Never use `Start-Job`, a trailing `&`, a
     fire-and-forget watcher, or a host background-execution parameter such as
     `run_in_background`, and never end a delegated turn while a build is
     running — a background task's completion notification cannot resume a
     worker whose turn has ended.
   - If the host call times out while a build without `-RunDataPacker`
     continues, re-invoke the identical command; target serialization and
     incremental tlogs carry it to completion.
   - A `-RunDataPacker` invocation materializes Local data before it reaches
     that serialization, so first confirm the prior invocation's process has
     ended — its shell call returned, or the process is provably gone — and only
     then re-invoke the identical command, which regenerates rather than reusing
     the interrupted run's output.
   - When a blocking call is unavailable, poll the same invocation to completion
     in-turn.

10. Read the outcome from the schema name on stdout, never from the exit code
    alone. Done when each invocation is classified as a build result or a block.

    - `broken-engine-build-result/v1` — WorktreeCli ran and its envelope is
      passed through byte-verbatim, with its own exit code preserved.
    - `broken-engine-compile-invoke-result/v1` — the build never ran.
      `status`/`code`/`message` name the block; exit `2` is a structured block
      (parameter contract, context, provisioning, missing AgentTools or target,
      data-mode authorization, ThirdParty output ownership) and exit `1` is an
      internal error. Report the exact `code` and `message` and stop; never
      present it as a build result.

    - Both envelopes are guaranteed only from successful parameter binding
      onward.
    - A value the parameter contract rejects at binding time — an unlisted
      `-Target`, `-Configuration`, or `-DataBuildMode` value — fails before the
      script runs, so PowerShell's own parameter error goes to stderr with a
      nonzero exit and stdout stays empty; fix the invocation rather than
      looking for a typed `code`.

11. Parse and report only the structured result described above after every
    requested target has returned. Done when the handoff `../SKILL.md` defines
    is returned.

## Rules

- `WorktreeCli build` serializes writers per target basename inside the current
  worktree, waits up to 500 seconds, preserves native Windows argument
  boundaries, and owns MSBuild through a kill-on-close Job Object.
- A lock timeout means another WorktreeCli build still owns that target. Retry
  after it finishes; never delete `.claude/build-locks/` manually.
- A prior killed build's `unsuccessfulbuild` marker clears on the next
  successful run; rerun instead of deleting tlogs.
- Every invocation that clears the parameter, context, ThirdParty
  output-ownership, and data-mode authorization guards runs
  `.agents/scripts/Provision-WorktreeThirdParty.ps1` against the derived root
  and then requires both executables before anything else happens; a nonzero
  provisioning exit is the typed block `provisioning.failed`, and a missing
  executable is the typed block `agent-tools.missing`. Never run provisioning or
  the existence checks by hand as a separate agent command — the build entry
  point owns both.
- The build entry point re-resolves the same context internally on every call,
  so step 1's run informs your decision and never feeds values into a later
  command. Each of its inputs takes an explicitly supplied parameter first, then
  the wrapper environment hint (`BROKEN_ENGINE_WORKTREE_PATH`,
  `BROKEN_ENGINE_PRIMARY_CHECKOUT`), then the derived default. The baseline takes
  no environment hint; step 2's `baseline` field states how it resolves.
- Never reconstruct these operations inline: no hand-typed changed-path
  `git diff`/`git ls-files` commands, no hand-resolved repository root, primary
  checkout, or baseline, and no inline `vswhere`/`DevEnvDir` discovery. A
  blocked result is a stop, not a cue to redo its work by hand.
- Worktree session start seeds the worktree's DataPacker Release Output by
  verified copy from the primary bootstrap prebuild when the worktree's clean
  `DataPacker/`/`Common/`/`ThirdParty/` trees match the stamp, and otherwise
  builds it locally (`.agents/scripts/Build-WorktreeDataPacker.ps1`), so
  `DataPacker.exe` is already present at session start; a session that changes
  DataPacker-relevant sources performs a full local rebuild on its first
  DataPacker build. When that prebuild is stamped, bootstrap also runs the
  primary's DataPacker over the primary checkout's current asset inputs into the
  primary `Output\Data`, refreshing the data Shared-mode worktrees consume,
  first waiting up to its `-WaitSeconds` budget for any `BrokenEngineSandbox*`
  process to exit and skipping the run with a warning only if one is still alive
  when that budget expires; those inputs are not checked against HEAD, and a
  nonzero exit fails session start. DataPacker's `"BrokenEngineDataPacker"`
  mutex and the AgentTools bootstrap mutex are the two PC-global build
  coordination points; add no others (the seed copy reuses the bootstrap mutex
  for its prebuild). They now nest: bootstrap holds its own mutex while that run
  waits for `"BrokenEngineDataPacker"`, and that wait is deliberately unbounded,
  so a peer DataPacker run or a rare island bake can block session start long
  enough to time other session starts out.
- DataPacker's mutex coordinates across worktrees and its shared chunks live
  under `%LOCALAPPDATA%\BrokenEngine\DataPackerCache\<Project>`; do not add
  another PC-global DataPacker lock or a checkout-local cache copy (the
  session-start seed copies the built `DataPacker.exe` once into the worktree's
  own Output — a verified artifact seed, not a shared chunk cache). Gaea raw and
  split intermediates use the single mutable
  `%LOCALAPPDATA%\BrokenEngine\DataPackerCache\<Project>\Gaea\Islands` cache;
  source-tree island leaves retain only tracked BC outputs.
- The build entry point provisions the worktree itself before every build and
  stops on failure. Validated stable primary submodule trees plus shared
  immutable ThirdParty, WorktreeCli, and AgentHarness Output directories are the
  only exceptions to worktree-local build artifacts.
- Keep existing build serialization and the gated AgentTools promotion path; do
  not share mutable build output between checkouts (the session-start DataPacker
  seed is a one-time verified copy the worktree then owns and may rebuild over,
  not shared output).
- The AgentTools targets build into a session-local `OutDir` because the default
  Output is the shared immutable primary Output held by the running WorktreeCli
  driver; a default-output WorktreeCli build therefore fails with a structural
  LNK1104. They produce `Temp\AgentToolsCandidate\WorktreeCli.exe` and
  `Temp\AgentToolsCandidate\AgentHarness.exe` under the repository root for
  AgentTools promotion.
- A WorktreeCli default-output `LNK1104` is structural because the running
  driver holds the shared primary Output; use the AgentTools targets in step 7.
  Other `LNK1104`, `LNK1168`, or EXE `LNK2019` failures can mean a live target
  process still holds the executable; report them rather than diagnosing unless
  asked.
- Ordinary builds do not run DataPacker, Gaea, or texture export; the authorized
  Local-generation path and the wrapper bootstrap's primary run are the only
  exceptions. Every invocation passes `EnableClangTidyCodeAnalysis=false` and
  `RunCodeAnalysis=false`; `-Prefast` is the sole exception to
  `RunCodeAnalysis=false`. VS2026 clang-tidy crashes on this codebase.
- A `builder` executing this skill runs the build itself and never dispatches
  another agent.
