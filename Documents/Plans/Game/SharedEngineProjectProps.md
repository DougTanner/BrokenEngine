<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:35:09.047Z","dependsOn":[]} -->
# Share the Engine Project Settings Through One .props File

## Context

Three Visual Studio projects build the engine and each carries its own copy of
the engine build settings: the BrokenEngineSandbox client
(`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`),
its server (`BrokenEngineSandboxServer.vcxproj` beside it), and the ModelViewer
(`Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj`). A new
game project (Coretide) would add a fourth and fifth copy, so the user asked
for this to be planned before that project starts.

Nothing was built or run for this Plan: every statement below is from a code
read at commit `b5c75660`. Line numbers are from that tree; locate by element
name if they moved.

The repeated blocks:

| Block | Client | Server | ModelViewer |
|---|---|---|---|
| Data properties | 44-53 | 44-53 | 27-35 |
| Output, analysis and clang-tidy properties | 54-84 | 54-83 | 36-53 |
| Shared `ClCompile` settings | 86-106 | 85-105 | 55-73 |
| Shared `Link` settings | 107-113 | 106-111 | 74-78 |
| DataPacker `CustomBuildStep` | 114-122 | 112-120 | 93-97 |
| `PreBuildEvent` | 123-134 | 121-132 | 79-92 |
| Per-configuration `ClCompile` and `Link` | 136-177 | 134-175 | 99-124 |

The blocks are near-identical, not identical. Differences read from the tree:

- The client and server have Debug, Profile and Release; the ModelViewer has
  Debug and Release only.
- The server links `SubSystem` Console with `EntryPointSymbol`
  `wWinMainCRTStartup`, defines `BT_SERVER`, uses `ProcessorNumber` 4 where the
  other two use 8, and its include list omits directories the client lists.
  The client turns clang-tidy off in Profile; the server does not.
- The ModelViewer puts `_ITERATOR_DEBUG_LEVEL=0` and the two
  `_MSVC_STL_HARDENING_*` definitions in every configuration (line 66); the
  client and server define them in Debug only (client line 142).
- The ModelViewer Release block (lines 111-124) sets `FunctionLevelLinking`
  true and omits `FavorSizeOrSpeed`, `OmitFramePointers`,
  `EnableFiberSafeOptimizations`, `BufferSecurityCheck`, `ControlFlowGuard`
  and `GenerateDebugInformation`, which the client sets at lines 145-165 with
  `FunctionLevelLinking` false.
- The ModelViewer `PreBuildEvent` addresses DataPacker through
  `$(DataRepositoryRoot)` and exits on a failed DataPacker build
  (`if errorlevel 1 exit /b 1`, lines 85 and 89); the client and server use
  `$(SolutionDir)..\..\..\..` and have no such check.
- The ModelViewer imports `Build\ResolvedData.props` (line 26), defaults
  `GameDataDirectory` to the BrokenEngineSandbox output (line 33), adds
  `$(ProjectDir)Build` and the BrokenEngineSandbox `Data` directory to its
  include list, and names the BrokenEngineSandbox rule set by repository path
  (line 51).
- The client and server name the rule set through
  `$(MSBuildThisFileDirectory)` (client line 71), which is the directory of
  the file that contains the reference: the project's own directory today,
  where the rule set file sits, and the `.props` file's directory once the
  line sits in a `.props` file.

No `.props` file is tracked in the repository today, and no project imports
one except the generated, untracked `Build\ResolvedData.props`.

Originating gap: a read-only audit of code that a second game would have to
copy. It is not an unmet acceptance criterion of any change.

## Design

Recommended shape (the author's recommendation; rationale given):

- One tracked `.props` file owned by the engine, for example
  `Engine/Platforms/VisualStudio2026/Engine.props`, imported by all three
  projects. Rationale: one owner for the settings the determinism rules depend
  on (`FloatingPointModel` Strict, `Documents/FloatingPointDeterminism.txt`),
  instead of the "when changing one, change the other" mirror rule in
  `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`
  `## Build Configuration`.
- The shared file carries only settings whose values are the same in every
  importing project. A setting that differs stays in the project that differs,
  as an override after the import. Rationale: the change is then
  behavior-preserving, and no project's compiler or linker options change.
- Any setting the executing session would rather unify than override (the
  ModelViewer Release differences above are the candidates) is surfaced to
  the user as a choice and not unified silently.
- Item lists (`ClCompile`, `ClInclude`, `None`) stay inline in each project.
- Paths in the shared file are written so they resolve from every importing
  project; `$(MSBuildThisFileDirectory)` is not carried over unchanged.

Investigation portion, the one open part of this Plan. Before editing a
project file, the executing session confirms by reading the scripts, then by
running them, that these keep working with settings in a shared file, and
includes any required script change in this Plan's change:

- `.agents/scripts/Invoke-IncludeOrderCheck.ps1` `Get-IncludeDirectory` (lines
  98-112) reads the first `<AdditionalIncludeDirectories>` element from the
  `.vcxproj` text with a regular expression and throws when the project file
  has none. Moving the include list out of the project file breaks it as
  written.
- `.agents/skills/update-vcxproj/scripts/Update-EngineVcxprojItem.ps1` (line
  301 iterates `Get-AgentGameProjects`) and `Invoke-VcxprojPairCheck.ps1`: the
  code read found item handling only, but that is unconfirmed for the
  setting blocks.
- `/compile`: `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1`,
  `.agents/scripts/Build-WorktreeTarget.ps1`, `Write-AgentModelViewerResolvedData`
  in `.agents/scripts/AgentScriptCommon.psm1` (line 302), and
  `Tools/WorktreeCli/BuildCommand.cpp`, including whether a changed `.props`
  file triggers the rebuild a changed `.vcxproj` triggers.
- `.agents/scripts/Get-SessionChangeInventory.ps1` line 248 classifies only
  `.vcxproj` and `.vcxproj.filters` as the `vcxproj` class, so a `.props` file
  would fall into `other`; decide whether it joins that class.
- Session worktree provisioning: whether the new file reaches a session
  worktree like any tracked file.

If a tool cannot be kept working inside this scope, surface that for
re-planning instead of widening the change.

Risk tier: Tier 3 (the author's classification). Trigger: build/bootstrap
coordination that can block other sessions — the file would carry the
`PreBuildEvent` that provisions ThirdParty and the DataPacker export step for
every engine build — and the compiler settings client/server floating-point
determinism depends on. Exposure: build only. No source, `.pack`, `kiVersion`,
wire, save, replay, or threading change; determinism is exposed only through
an unintended compiler-option change, which the first acceptance criterion
guards.

## Critical files

- `Engine/Platforms/VisualStudio2026/Engine.props` (new; name and place are
  the author's suggestion)
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj`
- `Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`
- `.agents/scripts/Invoke-IncludeOrderCheck.ps1`

## In scope

- The setting blocks listed in `## Context` in the three project files: the
  property groups and item definition groups, replaced by one import plus the
  per-project overrides.
- The new shared `.props` file.
- `Invoke-IncludeOrderCheck.ps1` `Get-IncludeDirectory`, and any other script
  the investigation proves broken by the move, changed only as far as needed
  to keep its current result.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`
  `## Build Configuration`: the mirror rule and the statements about where
  settings live; and `Projects/ModelViewer/AGENTS.md` where it describes the
  project's settings.

## Out of scope

- Every `ItemGroup` of source, header and shader items, and both
  `.vcxproj.filters` files' content.
- The value of any compiler, linker, analysis or data setting, unless the user
  chooses a unification the executing session surfaced.
- `DataPacker`, `ThirdParty`, and the `Tools/` projects.
- A project template or generator for new games, and any Coretide file.
- The solution files, beyond what an import requires.

## Acceptance criteria

1. For the client, the server and the ModelViewer, in every configuration each
   has, the compiler and linker command lines the build records (the
   `CL.command.1.tlog` and `link.command.1.tlog` files in the intermediate
   directory, or an equivalent the executing session names) are the same
   before and after the change, apart from differences the user approved.
2. `/compile` builds the client, the server and the ModelViewer in the
   configurations it supports, in a session worktree, including one Local
   generation build that runs the DataPacker step.
3. `Invoke-IncludeOrderCheck.ps1`, run as its owner documents, returns the
   same result on the unchanged source tree before and after.
4. `/update-vcxproj` validation passes on the three projects.
5. The replay determinism recipe in
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md`, run
   through `/agent-harness`, shows no CRC mismatch. The supplied evidence
   records no harness setup; any recording that recipe accepts serves.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- `Projects/AGENTS.md` `## Standard Project Layout` lists what a game project
  provides ("A game project provides:", lines 21-31). It lists files and
  directories, not what a project file contains, so the import line does not
  make it incorrect; adding the import line to that list is not part of this
  Plan.
