# Build Worker

Build steps and rules for the worker dispatched with
[`../SKILL.md`](../SKILL.md), which owns the purpose, the triggers, the inputs,
and the handoff this run returns.

## Steps

1. Resolve identities, the changed-path set, and the data-mode directories by
   running the repository-owned read-only script. Done when exactly one
   `broken-engine-compile-context/v2` JSON object is captured from its stdout;
   human diagnostics go to stderr.

```powershell
pwsh -NoProfile -File .agents/skills/compile/scripts/Resolve-CompileContext.ps1
```

   - Append `-RepositoryRoot`/`-PrimaryCheckout`/`-Baseline` only when the
     caller explicitly supplied that input; an empty or unset value counts as
     not supplied, so omit the parameter and let the derived default apply.
     Pass the same explicitly supplied values to the build invocation.
   - Append `-Project <Name>` only as step 3's game selection directs.
   - For a requested ModelViewer build, append `-Target ModelViewer`; its context
     resolves BrokenEngineSandbox as data owner even with several game projects.
   - `-IncludeDevEnvDir` is not an agent-run option: the build entry point
     requests DevEnvDir itself for every game build.

2. Read that result as evidence of which targets to build and which data mode
   each game build uses. Done when every field below has been read and no
   blocking condition is present.

   - `status`/`code`/`message` with exit `0` pass, `2` structured blocked, `1`
     internal error. Any nonzero exit stops the build: report the exact `code`
     and `message`.
   - `repositoryRoot`, `primaryCheckout`, and `baseline` (the resolved commit) —
     an explicitly supplied baseline is authoritative and is used exactly as
     given; without one, a session worktree resolves its baseline through the
     session context, which may advance it to the session's real divergence
     point, and falls back to `HEAD` whenever that resolution fails.
   - `project` — the game project: the `-Project` name, otherwise the only game
     project discovered under `Projects/`; `null` when several exist and none
     was named, which leaves `gameDataDirectory` and
     `generatedDataIncludeRoot` `null` and `recommendedTargets` empty.
     `projects` — every discovered game project, in ordinal order. A `-Project`
     name that is not among them blocks under `project.not-found`.
   - `changedPaths`/`changedPathCount` — the complete changed-path set: the
     single baseline diff (committed, staged, and unstaged tracked changes) plus
     untracked files, separator-normalized. `changedPathsTruncated` and code
     `output.capacity-exceeded` are blocking conditions, never a partial answer
     to work from.
   - `dataBuildMode` and `dataBuildModeReason` — the data mode every game build
     uses and why, decided by the rule in
     [runtime-data-mode.md](runtime-data-mode.md) `## Mode selection`;
     `sharedDataStamp` — the data stamp's commit, or `null`;
     `dataInputChanges` (the first 20) and `dataInputChangeCount` — the
     differing data inputs. Plus `gameDataDirectory` and
     `generatedDataIncludeRoot`.
   - `recommendedTargets` — advisory `{project, configuration, reason}` rows
     derived from the changed paths.

3. Fix the target and configuration list from that evidence. Done when every
   target this request will build is named.

   - Start from `recommendedTargets`, which is advisory: with `<project>` the
     step 2 `project`, a `<project>` row is `-Target Client` and a
     `<project>Server` row is `-Target Server`, but you still fix the final
     list, because the rules below depend on session state and request inputs
     the script cannot see.
     A `ModelViewer` row maps to `-Target ModelViewer`. Build it once, with its
     fixed BrokenEngineSandbox data owner, separately from any multi-game loop.
   - Game selection: when the caller named a game, pass `-Project <Name>` to
     step 1 and to each Client/Server invocation. With one game and none
     named, the invocations stay as shown, with no `-Project`. When none is
     named and step 1 reports `project` `null` with two or more `projects`,
     build each requested non-game target once, without `-Project`; then, for
     each game in `projects` order, rerun step 1 with `-Project <Name>`, read
     it per step 2, and fix, compose, run, and classify that game's
     Client/Server invocations per steps 3, 5, and 7-10, each with
     `-Project <Name>`, so that game's own context (its data mode,
     `gameDataDirectory`, `recommendedTargets`) drives its builds. One game's
     block or failure does not stop the next game's builds. Every other step
     runs once per dispatch: step 4 clears every game's target executables
     before the first build, and step 9's one envelope file holds every game's
     envelopes.
   - If the session's approved plan or acceptance table includes an
     agent-harness scenario, build both client and server in the same request
     regardless of changed-file affinity — the harness launches both executables
     and hard-stops when either is missing.
   - ThirdParty builds run only in the primary checkout
     ([`../SKILL.md`](../SKILL.md) `## Inputs`); from any other checkout
     `-Target ThirdParty` is the typed block `thirdparty.worktree-not-primary`.
     Missing source or library links are provisioning failures and a routine
     `/compile` never rebuilds ThirdParty itself. Wrapper bootstrap does
     incremental-rebuild ThirdParty at every session start (see Bootstrap
     AgentTools), so a routine `/compile` normally finds it already current.
   - Build the Release-only targets that [`../SKILL.md`](../SKILL.md)
     `## Inputs` names in Release. WorktreeCli still supplies the normal
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

5. Read [runtime-data-mode.md](runtime-data-mode.md) for every build targeting
   a game project. Done when it has been read; the build invocation resolves
   the data mode itself, and you do not select it.

   - Client or server, full or selective, whichever source files a selective
     compile lists.
   - That reference is mandatory reading in that case, not optional background.
   - Only builds targeting a non-game project (ThirdParty, DataPacker,
     WorktreeCli, AgentHarness) read neither reference.

6. Read [prefast-mode.md](prefast-mode.md) before building when an approved plan
   explicitly requires PREfast verification, and
   [clang-tidy-mode.md](clang-tidy-mode.md) when one explicitly requires a
   clang-tidy run. Done when each such reference has been read, or stays
   unloaded because its authorization does not exist.

7. Compose one bundled-script invocation per target. Done when the script's exact
   arguments are chosen without any shell variable, composed MSBuild `/p:`
   switch, or state carried between shell calls.

   - Each call is independently valid in a fresh shell because it re-derives
     identities, data mode, directories, and tool paths itself.

```powershell
# ThirdParty: choose requested configuration (Debug, Profile, or Release).
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target ThirdParty -Configuration Debug

# DataPacker: Release only, and Release is the default.
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target DataPacker

# Game client, then server; add -Project <Name> per step 3. Substitute Profile/Release only when requested.
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target Client -Configuration Debug
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target Server -Configuration Debug

# ModelViewer: Debug or Release, always using BrokenEngineSandbox data.
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target ModelViewer -Configuration Debug

# AgentTools: Release candidates under the session-local ignored Temp tree.
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target WorktreeCli
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target AgentHarness
```

   Parameters:

   - `-Target` — `ThirdParty`, `DataPacker`, `Client`, `Server`, `ModelViewer`,
     `WorktreeCli`, or `AgentHarness`.
   - `-Configuration` — `Debug`, `Profile`, or `Release`; required for
     ThirdParty, Client, Server, and ModelViewer (Debug/Release only). Omit it for
     the Release-only targets
     [`../SKILL.md`](../SKILL.md) `## Inputs` names; an explicit non-Release
     value on those targets is a typed block.
   - `-Files` — selective compile, Client/Server/ModelViewer only; see step 8.
   - `-AllowGaeaExport`, `-ForbidExpensiveExport` — Client/Server/ModelViewer only;
     [runtime-data-mode.md](runtime-data-mode.md) `## Local generation` owns
     when each applies.
   - `-Project` — the game project per step 3; ModelViewer permits only
     BrokenEngineSandbox and selects it automatically when omitted.
   - `-Prefast` — Client/Server Release only; see
     [prefast-mode.md](prefast-mode.md).
   - `-ClangTidy` — Client/Server Debug or Release and DataPacker only, never
     with `-Prefast`; see [clang-tidy-mode.md](clang-tidy-mode.md).
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
   - A shader source change is a data-input change, so the build resolves
     Local and generates its own output
     ([runtime-data-mode.md](runtime-data-mode.md) `## Mode selection`).

9. Run each build invocation synchronously in the foreground and remain in-turn
   until its process exit code and single JSON result are captured. Done when
   every requested target has returned that pair and each build's block is in
   this dispatch's envelope file.

   - In a call of its own before the first build call, create
     `Temp/AgentBuildEnvelopes/` if absent and this dispatch's envelope file in
     it: compose the repo-relative path
     `Temp/AgentBuildEnvelopes/compile-<UTC yyyyMMddTHHmmssfffZ>-<process id>.md`
     from that call's own UTC time and process id, create the file holding the
     dispatch's single `##` heading, and print the path. `Temp/` is gitignored,
     so the directory is absent in a fresh worktree.
   - Every build call has the same shape, with that path written as a literal;
     never find the file by name pattern, process id, or modification time,
     because other dispatches' files share the directory. The build script is
     piped to `Tee-Object -Variable buildOutput`, so its envelope still reaches
     stdout for step 10, and one `Add-Content` statement then writes the
     call's whole fenced block after the script returns. Its value begins with
     an empty line, so the opening fence always starts its own line whether or
     not the line before it ended with a line break:

```powershell
pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 <arguments> | Tee-Object -Variable buildOutput; Add-Content -LiteralPath '<path>' -Value '', '```json', "$buildOutput", '```'
```

   - The block holds that call's stdout envelope verbatim, whichever schema it
     carries: `broken-engine-build-result/v1` or
     `broken-engine-compile-invoke-result/v1`. Keep the fence literals
     single-quoted and `"$buildOutput"` double-quoted, so an empty stdout
     still writes one line between the fences. Never write a block from a new
     script or a retyped envelope.
   - Give the call the maximum available execution timeout so the 500-second
     target-lock wait is not preempted. Never use `Start-Job`, a trailing `&`, a
     fire-and-forget watcher, or a host background-execution parameter such as
     `run_in_background`, and never end a delegated turn while a build is
     running — a background task's completion notification cannot resume a
     worker whose turn has ended.
   - If the host call times out while a build that is not a Local game build
     continues, re-invoke the identical command; target serialization and
     incremental tlogs carry it to completion.
   - A Local game build materializes Local data before it reaches
     that serialization, so first confirm the prior invocation's process has
     ended — its shell call returned, or the process is provably gone — and only
     then re-invoke the identical command, which regenerates rather than reusing
     the interrupted run's output.
   - When a blocking call is unavailable, poll the same invocation to completion
     in-turn.

10. Read the outcome from the schema name on stdout, never from the exit code
    alone. Done when each invocation is classified as a build result or a block.

    - `broken-engine-build-result/v1` — WorktreeCli ran and its envelope is
      passed through byte-verbatim, with its own exit code preserved. Its
      fields:
      - `status` (`success`/`fail`), `failureKind` (`none`/`tool`/`msbuild`),
        and `exitCode` — the process exit code keeps its existing meaning
        (MSBuild's exit code once launched; `1` for tool failures including a
        retained-log failure after a successful build).
      - `target`/`worktreeRoot` normalized identities, `arguments`,
        `selectedFiles`, and `invalidatedObjects` — the object paths
        invalidated for a `-Files` selection before MSBuild ran, empty without
        `-Files`; it never reports what MSBuild compiled, which only the
        retained log records.
      - `lock` outcome (`acquired`/`timeout`/`failed`) with the lock path and
        waited seconds.
      - `msbuild` discovery/launch state and MSBuild's own exit code.
      - `retainedLog` — the complete combined MSBuild stdout+stderr stream in
        observed read order, untruncated, below the invoking worktree's ignored
        `Temp/AgentBuildLogs/`. `complete: false` or a missing log is a
        build-result failure, never an omitted side effect.
      - `diagnostics` — structured MSBuild error/warning entries (`severity`,
        `code`, `file`, `line`, `column`, `project`, `message`, `raw`), capped
        with `diagnosticsTruncated: true` when the raw log holds more;
        `messages` carries tool failures and unmatched fatal lines.
      - `elapsedMilliseconds` and `startedAt`.
    - `broken-engine-compile-invoke-result/v1` — the build never ran.
      `status`/`code`/`message` name the block; exit `2` is a structured block
      (parameter contract, context, provisioning, missing AgentTools or target,
      ThirdParty output ownership, stale process
      Vulkan SDK as `vulkan-sdk.stale-environment`) and exit `1` is an
      internal error. Report the exact `code` and `message` and stop — inside
      step 3's several-games loop, stop only that game's invocations; never
      present it as a build result.

    - Both envelopes are guaranteed only from successful parameter binding
      onward.
    - A value the parameter contract rejects at binding time — an unlisted
      `-Target` or `-Configuration` value, or an unknown parameter — fails
      before the script runs, so PowerShell's own parameter error goes to stderr with a
      nonzero exit and stdout stays empty; fix the invocation rather than
      looking for a typed `code`.

11. Parse and report only the structured result described above after every
    requested target has returned. Done when the handoff `../SKILL.md` defines
    is returned.

## Rules

- `WorktreeCli build` serializes writers per target basename inside the current
  worktree, waits up to 500 seconds, preserves native Windows argument
  boundaries, and owns MSBuild through a kill-on-close Job Object.
- A `C1090` or `LNK1318` PDB-service failure is reported like any other
  build failure, with no process check and no retry: the build entry point
  gives each build its own PDB server (`_MSPDBSRV_ENDPOINT_`), so another
  build cannot cause it.
- A lock timeout means another WorktreeCli build still owns that target. Retry
  after it finishes; never delete `.claude/build-locks/` manually.
- A prior killed build's `unsuccessfulbuild` marker clears on the next
  successful run; rerun instead of deleting tlogs.
- Every invocation that clears the parameter, context, and ThirdParty
  output-ownership guards runs
  `.agents/scripts/Provision-WorktreeThirdParty.ps1` against the derived root
  and then requires both executables before anything else happens; a nonzero
  provisioning exit is the typed block `provisioning.failed`, and a missing
  executable is the typed block `agent-tools.missing`. Never run provisioning or
  the existence checks by hand as a separate agent command — the build entry
  point owns both.
- The build entry point re-resolves the same context internally on every call,
  so step 1's run informs your decision and never feeds values into a later
  command. Each of its inputs takes an explicitly supplied parameter first, then
  the derived default; step 2's `baseline` field states how the baseline
  resolves.
- Never reconstruct these operations inline: no hand-typed changed-path
  `git diff`/`git ls-files` commands, no hand-resolved repository root, primary
  checkout, or baseline, and no inline `vswhere`/`DevEnvDir` discovery. A
  blocked result is a stop, not a cue to redo its work by hand.
- Worktree session start seeds the worktree's DataPacker and ModelViewer
  Release Output by verified copy from the primary bootstrap prebuilds, and
  otherwise builds each locally, so `DataPacker.exe` and `ModelViewer.exe` are
  already present at session start. The seed conditions and local-build
  fallbacks are in `.agents/scripts/Build-WorktreeTarget.ps1`. A session that
  changes either target's sources performs a full local rebuild on its first
  build of that target.
  Bootstrap also refreshes primary Shared data per game
  ([Wrapper bootstrap Shared-data refresh](runtime-data-mode.md#wrapper-bootstrap-shared-data-refresh)),
  and a nonzero DataPacker exit fails session start. DataPacker's
  `"BrokenEngineDataPacker"` mutex and the AgentTools bootstrap mutex are the
  two PC-global build coordination points; add no others (the seed copies reuse
  the bootstrap mutex for their prebuilds). They now nest: bootstrap holds its own
  mutex while that refresh waits for `"BrokenEngineDataPacker"`, and that wait
  is deliberately unbounded, so a peer DataPacker run or a rare island bake can
  block session start long enough to time other session starts out.
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
  and ModelViewer seeds are one-time verified copies the worktree then owns and
  may rebuild over, not shared output).
- The AgentTools targets build into a session-local `OutDir` because the default
  Output is the shared immutable primary Output held by the running WorktreeCli
  driver; a default-output WorktreeCli build therefore fails with a structural
  LNK1104. They produce `Temp\AgentToolsCandidate\WorktreeCli.exe` and
  `Temp\AgentToolsCandidate\AgentHarness.exe` under the repository root for
  AgentTools promotion. The authoritative executables that promotion maintains
  are `Tools\WorktreeCli\Platforms\VisualStudio2026\Output\WorktreeCli.exe` and
  `Tools\AgentHarness\Platforms\VisualStudio2026\Output\AgentHarness.exe` under
  the resolved repository root.
- A WorktreeCli default-output `LNK1104` is structural because the running
  driver holds the shared primary Output; use the AgentTools targets in step 7.
  Other `LNK1104`, `LNK1168`, or EXE `LNK2019` failures can mean a live target
  process still holds the executable; report them rather than diagnosing unless
  asked.
- Builds do not run DataPacker, Gaea, or texture export; the Local generation
  build ([runtime-data-mode.md](runtime-data-mode.md) `## Local generation`)
  and the wrapper bootstrap's primary run are the only exceptions. Every invocation passes `EnableClangTidyCodeAnalysis=false` and
  `RunCodeAnalysis=false`; `-Prefast` and `-ClangTidy` are the only exceptions,
  and `-ClangTidy` keeps the rule-set path off. `RunCodeAnalysis=false` still
  leaves cl `/analyze` running with the compiler's default rules on every
  Release build whose project sets `EnablePREfast` (client, server,
  DataPacker), where any diagnostic fails the build under warnings-as-errors;
  only the rule-set path that reports the C26xxx codes needs `-Prefast`. The
  clang-tidy reason and both analysis paths (the "Microsoft code analysis"
  bullet) are in
  `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`
  `## Build Configuration`.
- A `builder` executing this skill runs the build itself and never dispatches
  another agent.
