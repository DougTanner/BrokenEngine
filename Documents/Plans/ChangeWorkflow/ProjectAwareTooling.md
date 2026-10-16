<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-15T23:31:24.074Z","dependsOn":[]} -->
# Select the game project in repository tooling instead of hardcoding BrokenEngineSandbox

## Context

The user's direction, relayed by the dispatch brief that commissioned this Plan:
a second game project will be created beside `Projects/BrokenEngineSandbox`, and
repository tooling must support it. Today the build, data, bootstrap, and
harness-claim scripts name `BrokenEngineSandbox` literally, so a second project
under `Projects/` cannot be built, packed, bootstrapped, or harness-claimed
through them. Verified at baseline `f1d140ae`:

- `/compile`: `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1:53-66`
  fixes the Client/Server `.sln` and `.vcxproj` paths and
  `$script:DataOutputSuffix` to `Projects\BrokenEngineSandbox\...`; `:277` and
  `:285` limit the deletion-only exception to
  `Projects/BrokenEngineSandbox/Data/**`; `:434` passes
  `Projects\BrokenEngineSandbox\Data` to `--materialize-data`.
  `.agents/skills/compile/scripts/Resolve-CompileContext.ps1:31` fixes the same
  data suffix, `:36` makes `Projects/BrokenEngineSandbox/Data/**` the only
  project Local trigger, and `Get-ContextRecommendedTargets` (`:182-197`)
  returns only `BrokenEngineSandbox`/`BrokenEngineSandboxServer` rows.
- Session-start bootstrap: `.agents/scripts/Bootstrap-AgentTools.ps1:189,199`
  waits only for `BrokenEngineSandbox*` processes; `:207,218,227,241` refresh,
  dirty-check, and stamp only the Sandbox data and shader trees.
  `Get-AgentShaderSourceFingerprint` in `.agents/scripts/AgentScriptCommon.psm1:186-191`
  lists only `Engine/Data/Shaders` and `Projects/BrokenEngineSandbox/Data/Shaders`.
- Harness claim: `.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1:256`
  reads the executable names only from
  `Projects\BrokenEngineSandbox\Documents\AgentHarness.md`, although
  `.agents/skills/agent-harness/references/worker.md` step 3 selects a project
  and step 4 reads `Projects/<Project>/Documents/AgentHarness.md`.
- DataPacker: `DataPacker/Source/FileManager.cpp:363-369` makes a zero-argument
  run pack `Projects/BrokenEngineSandbox/Data` into the Sandbox Output, via
  `GetRepositoryRootFromExecutable` (`:95`, whose only caller is `:365`).
  Every tooling and vcxproj invocation already passes the three paths.
  `DataPacker/Source/Main.cpp:119,124` name Sandbox in a comment and in the
  locked-pack message.
- Smaller sites that would be wrong for a second project:
  `.agents/skills/analyze-diagsession/scripts/Invoke-DiagSessionSymbolization.ps1:36`
  puts only the Sandbox Output on `_NT_SYMBOL_PATH`, and
  `.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1:24`
  (with `references/HistoryContract.md:44`) puts only
  `Projects/BrokenEngineSandbox/Source/Agent/` in the AgentTools history area.
- `Tools/WorktreeCli`, `Tools/AgentHarness`, and `Tools/ToolCommon` already take
  project paths from their callers and need no change. `Projects/AGENTS.md`
  `## Standard Project Layout` already states the layout, but says it covers
  only `Test-CollectionLayout.ps1` and `Test-IncludeOrder.ps1`.

## Design

The recommendations below are the author's, each with its rationale; none is a
user decision beyond the direction quoted in `## Context`.

1. **One discovery helper.** Recommended: add `Get-AgentGameProjects([string] $Root)`
   to `AgentScriptCommon.psm1`, which returns the ordinal-sorted names of the
   `Projects/<Name>` directories that hold
   `Platforms/VisualStudio2026/<Name>.vcxproj` (the test `Test-IncludeOrder.ps1`
   already applies). Bootstrap, `/compile`, and diagsession symbolization all
   need the same list, so one helper avoids three copies. Every project path is
   then derived from the layout: `<Name>.sln`/`<Name>.vcxproj` (client),
   `<Name>Server.sln`/`<Name>Server.vcxproj` (server), `Projects/<Name>/Data`,
   `Projects/<Name>/Data/Shaders`, and
   `Projects/<Name>/Platforms/VisualStudio2026/Output/Data`.
2. **`/compile` project selection.** Recommended: add `[string] $Project` to
   both `Resolve-CompileContext.ps1` and `Invoke-CompileBuild.ps1`.
   - `Resolve-CompileContext.ps1` resolves the project once: a supplied name
     that is not a discovered project blocks under `project.not-found`; an
     omitted name resolves to the only discovered project; with several
     discovered and none supplied, the project is `null`. The result gains a
     `project` field. A resolved project adds the
     `Projects/<Project>/Data/**` path trigger, derives `gameDataDirectory` and
     `generatedDataIncludeRoot` from its Output path, and drives
     `recommendedTargets` (shared prefixes `Common/`, `Engine/`,
     `Projects/<Project>/Source/`; rows `<Project>` and `<Project>Server`). A
     `null` project leaves those fields `null` and `recommendedTargets` empty.
     Rationale: with today's single project every documented invocation keeps
     working unchanged, and a second project makes the omission an explicit
     block instead of a silent Sandbox build. The schema name stays
     `broken-engine-compile-context/v1`: the field is additive, only
     `Invoke-CompileBuild.ps1` and the compile worker read the envelope within
     one invocation, and no persisted file carries it.
   - `Invoke-CompileBuild.ps1` passes `-Project` through to the context. For a
     Client or Server target it blocks under `project.unresolved` (message
     lists the discovered projects) when `context.project` is `null`, and it
     builds every path it now hardcodes — target `.sln`, selective `.vcxproj`,
     data suffix, deletion-only asset trigger and message, shader stamp and
     fingerprint, and `--materialize-data` project input — from
     `context.project`. `-Project` on any other target blocks under
     `parameter.project-invalid`, mirroring the existing
     `parameter.expensive-export-invalid` guard.
3. **Shader fingerprint per project.** Recommended:
   `Get-AgentShaderSourceFingerprint([string] $Root, [string] $Revision, [string] $Project)`
   lists `Engine/Data/Shaders` and `Projects/<Project>/Data/Shaders`. For
   `BrokenEngineSandbox` the listing, and therefore the stamp value, stays
   byte-identical, so existing stamps stay valid.
4. **Bootstrap loops over projects.** Recommended: in `Bootstrap-AgentTools.ps1`
   the game-process wait matches `<Name>*` for every discovered project in one
   wait. This keeps the single `-WaitSeconds` budget and the existing loop and
   warning shape. The stamp delete, shader pre/post snapshots, DataPacker run,
   and stamp write then repeat per project with that project's paths. A failed
   DataPacker run still throws as today. The closing message lists every
   refreshed directory.
5. **Harness claim takes the project from `-GameDataDirectory`.** Recommended:
   `Invoke-HarnessClaim.ps1` derives `<Name>` from the canonical
   `-GameDataDirectory`, which must end in
   `Projects\<Name>\Platforms\VisualStudio2026\Output\Data`, and requires
   `Projects\<Name>\Platforms\VisualStudio2026\<Name>.vcxproj` under
   `-RepositoryRoot`. Otherwise it blocks under `claim.project-underivable`
   before provisioning and before the lock. It then reads
   `Projects\<Name>\Documents\AgentHarness.md` with the unchanged parser and
   reports `project` in its result. Rationale: no new parameter, and the
   executables checked can never belong to a different project than the data
   the pairing check validates, because the caller already passes the
   `/compile` result's `GameDataDirectory`.
6. **DataPacker requires explicit paths.** Recommended: `RunCommand` in
   `Main.cpp` prints `DataPacker requires exactly <engine-data> <project-data> <output-data>`
   and fails when a non-`--materialize-data` run does not have exactly three
   arguments, matching the `--materialize-data` arity check. The zero-argument
   branch in the `FileManager` constructor and `GetRepositoryRootFromExecutable`
   are deleted, and the constructor keeps `ASSERT(argvSpan.size() == 4)`. The
   comment and message at `Main.cpp:119,124` say "a running game client or
   server".
7. **Smaller sites.** Recommended: diagsession symbolization puts every
   discovered project's `Platforms\VisualStudio2026\Output` on
   `_NT_SYMBOL_PATH`, `;`-joined before the `srv*` entry, through the shared
   helper. The metrics history replaces its Sandbox row with a match for
   `Projects/<any>/Source/Agent/`, keeping first-match order. Existing history
   values are unchanged because only Sandbox exists.
8. **Documentation.** `Projects/AGENTS.md` `## Standard Project Layout` lists
   the derived paths from step 1 plus `Projects/<Name>/Documents/AgentHarness.md`.
   It names the tooling that now discovers or derives projects from the layout,
   replacing the "covers only those two checkers" sentence. Compile and harness
   docs describe the project input instead of Sandbox paths, as listed in
   `## In scope`.

Risk: Tier 3. Trigger: build/bootstrap coordination that can block other
sessions. `Bootstrap-AgentTools.ps1` runs at every wrapper session start under
the bootstrap mutex, and a failure there aborts session start. The change also
spans independently owned subsystems: the `/compile` skill, the
`/agent-harness` skill, the wrapper bootstrap, and DataPacker. Invariants:
Sandbox's resolved paths, data mode, `GameDataDirectory`, shader fingerprint,
packed output, and harness claim stay exactly as before. Determinism/CRC,
wire, `.pack`/`kiVersion`, save, and replay formats are untouched.

Build and data exposure: the DataPacker source edit is a `DataPacker/**` Local
trigger in `runtime-data-mode.md` `## Mode selection`, so the implementing
session's game builds need Local generation authorization (`-RunDataPacker`).
Request it at plan approval, before the first game build.

## Critical files

- `.agents/scripts/AgentScriptCommon.psm1`
- `.agents/scripts/Bootstrap-AgentTools.ps1`
- `.agents/skills/compile/scripts/Resolve-CompileContext.ps1`
- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1`
- `.agents/skills/compile/SKILL.md`, `references/worker.md`, `references/runtime-data-mode.md`
- `.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1`
- `.agents/skills/agent-harness/references/worker.md`
- `DataPacker/Source/FileManager.cpp`, `DataPacker/Source/Main.cpp`, `DataPacker/Source/AGENTS.md`
- `.agents/skills/analyze-diagsession/scripts/Invoke-DiagSessionSymbolization.ps1`
- `.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1`, `references/HistoryContract.md`
- `Projects/AGENTS.md`

## In scope

- `AgentScriptCommon.psm1`: new `Get-AgentGameProjects` (exported);
  `Get-AgentShaderSourceFingerprint` gains `$Project`.
- `Resolve-CompileContext.ps1`: `-Project` parameter, project resolution and
  `project.not-found`, `project` result field, `$script:DataOutputSuffix` and
  the Sandbox `$script:PathTriggers` row replaced by project-derived values,
  `Get-ContextRecommendedTargets`, the `gameDataDirectory` derivation, header
  comment.
- `Invoke-CompileBuild.ps1`: `-Project` parameter and pass-through,
  `parameter.project-invalid` and `project.unresolved`, `$script:TargetFiles`
  Client/Server, `$script:SelectiveTargetFiles`, `$script:DataOutputSuffix` and
  its uses, the deletion-only asset trigger list and message (`:277,285`), the
  shared shader stamp check's fingerprint calls, the `--materialize-data`
  project input (`:434`).
- `Bootstrap-AgentTools.ps1`: the game-process wait and the shared-data refresh
  block (`:184-252`), looped per discovered project; its comments.
- `Invoke-HarnessClaim.ps1`: project derivation from `-GameDataDirectory`,
  `claim.project-underivable`, hub path, `project` result field, and the
  comment above the hub read.
- `DataPacker/Source/Main.cpp`: `RunCommand` arity check; the comment and
  `lockedFailure` text of `PublishManifestAndPack`.
  `DataPacker/Source/FileManager.cpp`: the zero-argument branch of the
  `FileManager` constructor and `GetRepositoryRootFromExecutable`.
  `DataPacker/Source/AGENTS.md`: the CLI sentence.
- `Invoke-DiagSessionSymbolization.ps1`: the `_NT_SYMBOL_PATH` value and the
  module import it needs.
- `Invoke-CodeQualityMetricsHistory.ps1`: the Sandbox
  `$script:HistoryAreaPrefixes` row and the match at `:306`.
  `HistoryContract.md`: the AgentTools row of the history-area table.
- `.agents/skills/compile/SKILL.md`: description, `## When to use`, and
  `## Inputs` wording that names BrokenEngineSandbox as the game target. Add the
  game-project input, required when more than one game project exists.
- `.agents/skills/compile/references/worker.md`: steps 1, 2 (`project` field),
  3 (row-to-target mapping), 5, and 7 (`-Project` usage note); the bootstrap
  process-wait sentence (`:286`).
- `.agents/skills/compile/references/runtime-data-mode.md`: Sandbox paths in
  `## Mode selection`, the bootstrap refresh and stamp paragraphs, and
  `## Local generation`, restated per project.
- `.agents/skills/agent-harness/references/worker.md`: step 3's "Default today"
  sentence and step 5's executable-name bullet, which now say the claim takes
  the project from `-GameDataDirectory`.
- `Projects/AGENTS.md`: `## Standard Project Layout`.

## Out of scope

- Agent-harness routing links that point at Sandbox docs as the generic route:
  `.agents/skills/agent-harness/SKILL.md:27,42`, `references/worker.md:13,502,563-567`,
  `references/renderdoc.md:17`, and `references/command-reference.md:3,7,9`.
  `Documents/Plans/Engine/AgentHarnessIntoEngine.md` owns moving those docs.
- `.agents/skills/optimize-build-time/` (`SKILL.md` and
  `scripts/Measure-BuildTrace.ps1`): the skill is defined as a Sandbox rebuild
  measurement, so generalizing it needs a new skill input. That is a separate
  decision.
- `.agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1:33`: a
  budget override for one specific file, not project selection.
- `Test-CollectionLayout.ps1` and `Test-IncludeOrder.ps1`: their discovery
  stays as it is.
- Prose that names BrokenEngineSandbox as an example or as the current project:
  `.agents/skills/next-plan/SKILL.md:116`, the example comments in
  `prefast-mode.md` and `clang-tidy-mode.md`, the `-Files` example in compile
  `worker.md:155`, `Documents/FreshMachineSetup.md`, and the Sandbox project
  docs and vcxproj files.
- Creating the second project, and any change to WorktreeCli, AgentHarness,
  ToolCommon, Engine, or game C++.

## Acceptance criteria

1. Before the first edit, record `Resolve-CompileContext.ps1` output from the
   unedited tree. After the change, the same invocation without `-Project`
   reports `project: BrokenEngineSandbox` and identical `dataBuildMode`,
   `gameDataDirectory`, `generatedDataIncludeRoot`, trigger labels, and
   `recommendedTargets` for the same changed paths. With
   `-Project BrokenEngineSandbox` it reports the same values; with
   `-Project Missing` it exits 2 with `project.not-found`.
2. `/compile` builds Client and Server Debug with and without
   `-Project BrokenEngineSandbox`, and the Local generation build
   (`-DataBuildMode Local -RunDataPacker`) succeeds. Each game envelope's
   `/p:GameDataDirectory` is the Sandbox Output `Data` path the baseline
   context reported for that mode. `-Target DataPacker -Project BrokenEngineSandbox`
   blocks with `parameter.project-invalid`.
3. With a scratch untracked `Projects/Scratch/Platforms/VisualStudio2026/Scratch.vcxproj`
   present, a Client build without `-Project` blocks with `project.unresolved`,
   and its message names both projects. The scratch directory is deleted
   afterward and is never committed.
4. `Get-AgentShaderSourceFingerprint <worktree> <baseline> BrokenEngineSandbox`
   returns the same value as the baseline module's two-argument function for
   the same root and revision. Get the baseline module with
   `git show <baseline>:.agents/scripts/AgentScriptCommon.psm1` into `Temp/`.
5. `/agent-harness` claims with the Sandbox `GameDataDirectory`, reports
   `project: BrokenEngineSandbox`, and launches and pings the client and server
   as before. A `-GameDataDirectory` outside the layout exits 2 with
   `claim.project-underivable`, leaving no owner token or claim.
6. The worktree Release `DataPacker.exe` run with no arguments prints the usage
   line, exits non-zero, and writes no output. Three-argument packing is
   covered by criterion 2.
7. `Bootstrap-AgentTools.ps1` parses without errors
   (`[Management.Automation.Language.Parser]::ParseFile`), and a code read
   confirms that the per-project loop reproduces today's Sandbox arguments,
   stamp path, and dirty-check pathspecs. It runs only against the primary
   checkout, so the live observation is the first wrapper session start after
   landing. That start reports `Refreshed primary data at` the Sandbox Output
   `Data` path and leaves `SharedDataShaderStamp.txt` beside it.
8. `rg -n BrokenEngineSandbox` over the in-scope scripts and DataPacker sources
   (`AgentScriptCommon.psm1`, `Bootstrap-AgentTools.ps1`,
   `Resolve-CompileContext.ps1`, `Invoke-CompileBuild.ps1`,
   `Invoke-HarnessClaim.ps1`, `Invoke-DiagSessionSymbolization.ps1`,
   `Invoke-CodeQualityMetricsHistory.ps1`, `DataPacker/Source/FileManager.cpp`,
   `DataPacker/Source/Main.cpp`) finds no match.

## Notes

- No dependency on `Documents/Plans/Engine/SharedEngineProjectFiles.md`.
  `/compile` derives paths from the directory and file-name layout, not from
  the shared `.vcxitems`/`.props` that Plan introduces.
- `Documents/Plans/Engine/AgentHarnessIntoEngine.md` also edits
  `.agents/skills/agent-harness/references/worker.md`, in different regions.
  If it moves the machine-readable launch configuration out of
  `Projects/<Name>/Documents/AgentHarness.md`, the hub path in design step 5
  follows that move.
- `Documents/Plans/ChangeWorkflow/WrapRuntimeDataModeSelection.md` rewraps the
  same `runtime-data-mode.md` `## Mode selection` bullets. Whichever Plan lands
  second rebases its wording onto the other; neither changes the other's
  meaning.
