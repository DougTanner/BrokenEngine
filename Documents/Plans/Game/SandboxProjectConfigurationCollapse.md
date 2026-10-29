<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-29T12:13:24.856Z","dependsOn":[]} -->
# Collapse the Sandbox client and server project files' per-configuration repetition

## Context

A read-only duplication survey comparing ModelViewer with BrokenEngineSandbox
found that each Sandbox project file repeats its whole build configuration once
per configuration (Debug, Profile, Release), while
`Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj` writes the
configuration-independent settings once and only the per-configuration deltas
after them. This is pre-existing debt; no acceptance criterion is unmet. Line
numbers below are at commit `f91254a9`.

Facts in `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/`:

- `BrokenEngineSandbox.vcxproj` (client, 881 lines, UTF-8 BOM, LF) and
  `BrokenEngineSandboxServer.vcxproj` (server, 614 lines, no BOM, LF) each hold:
  three `Label="Configuration"` PropertyGroups (`:27-46` in both), three
  identical `Label="PropertySheets"` ImportGroups (`:52-60` in both), three
  per-configuration PropertyGroups (client `:72-114`, server `:72-112`), and
  three full ItemDefinitionGroups (client Debug `:119-174`, Profile `:175-244`,
  Release `:245-314`; server `:117-171`, `:172-240`, `:241-309`). Each
  ItemDefinitionGroup repeats the 17-entry (client) or 15-entry (server)
  `AdditionalIncludeDirectories`, the `Link` block, the three `CustomBuildStep`
  elements and the provisioning `PreBuildEvent`.
- Within one file, the Profile and Release ItemDefinitionGroups differ only in
  the configuration define (`BT_PROFILE`/`BT_RELEASE`), `EnablePREfast`
  (`false`/`true`) and one word of the 4702 comment. Debug differs by
  `Optimization`, `RuntimeLibrary`, the Debug-only defines
  (`_ITERATOR_DEBUG_LEVEL=0;_MSVC_STL_HARDENING_VECTOR=0;_MSVC_STL_HARDENING_SPAN=0`),
  `SupportJustMyCode`/`MinimalRebuild`, and the absence of the optimized-build
  settings (`InlineFunctionExpansion`, `FavorSizeOrSpeed`, `OmitFramePointers`,
  `EnableFiberSafeOptimizations`, `FunctionLevelLinking`, `BufferSecurityCheck`,
  `ControlFlowGuard`, `DisableSpecificWarnings` 4702, and the `Link`
  `GenerateDebugInformation`/`OptimizeReferences`/`EnableCOMDATFolding`/
  `LinkTimeCodeGeneration` keys).
- Client and server differ only in `ProjectGuid`/`RootNamespace` (`:19-20`),
  `ProcessorNumber` (8/4), `BT_CLIENT`/`BT_SERVER`, `SubSystem` and
  `EntryPointSymbol` (client `Windows` with an empty entry point; server
  `Console` with `wWinMainCRTStartup`), the server's include list omitting
  `ThirdParty\glm` and `ThirdParty\RenderDoc`, Profile
  `EnableClangTidyCodeAnalysis` (client `false` at `:94`, server `true` at
  `:93`), and `RunCodeAnalysis` below.
- Mirror drift: the client sets `<RunCodeAnalysis>false</RunCodeAnalysis>` in
  Debug (`:82`) and Profile (`:95`); the server sets neither. It has no effect:
  `Microsoft.Cpp.Analysis.props` in the VS 18 toolset defaults an empty
  `RunCodeAnalysis` to `false`.
- `Engine\Source\Pch.cpp` sets `PrecompiledHeader` `Create` under three
  configuration conditions (client `:684-688`, server `:536-540`).
- Empty filter declarations (a `<Filter Include>` with no item in it or below
  it), 17 in all: `BrokenEngineSandbox.vcxproj.filters` `Game\Agent\Commands`
  (`:58`) and `Game\Network\Server` (`:67`);
  `BrokenEngineSandboxServer.vcxproj.filters` `Engine\Graphics` (`:22`),
  `Engine\Graphics\Render` (`:37`), `Game\Network\Client` (`:52`),
  `Engine\Frame\Collections\SmokeTrails` (`:112`), `WindRadials` (`:115`),
  `WindTrails` (`:118`), `AreaLights` (`:124`), `Billboards` (`:127`),
  `HexShields` (`:133`), `PointLights` (`:136`), `Puffs` (`:139`), `Sounds`
  (`:145`), `Game\Input` (`:151`), `Engine\Ui\Screens` (`:157`); and
  `Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj.filters`
  `Engine\Network\Server` (`:60`, one line). The Sandbox declarations are three
  lines each. `AGENTS.md` `## vcxproj.filters Rules` in this directory names
  only `Engine\Graphics` as an empty leftover filter.

Tooling that reads these files, and what it requires:

- `.agents/scripts/Invoke-IncludeOrderCheck.ps1` (`Get-IncludeDirectory`)
  regex-reads the first `<AdditionalIncludeDirectories>` in the vcxproj text and
  keeps only `$(ProjectDir)` entries, so the include list stays in each vcxproj.
- `.agents/skills/update-vcxproj/scripts/Update-EngineVcxprojItem.ps1` line-parses
  LF-terminated, two-space-indented ItemGroups whose multi-line children start
  at four spaces and end at a matching four-space close tag;
  `Invoke-VcxprojPairCheck.ps1` reads items and filter declarations from each
  file's own XML and checks ancestors only for filters that hold items. Neither
  follows `<Import>`.
- No other tracked script or tool reads ItemDefinitionGroup settings.

## Design

Collapse each Sandbox vcxproj in place to the ModelViewer shape, keeping every
evaluated setting per configuration unchanged. The author recommends:

- One `Label="Configuration"` PropertyGroup holding the shared keys, with
  `UseDebugLibraries` and `WholeProgramOptimization` conditioned per
  configuration, as `ModelViewer.vcxproj:14-21` does.
- One unconditional `Label="PropertySheets"` ImportGroup.
- One unconditional PropertyGroup for `OutDir`, `IntDir`,
  `CustomBuildBeforeTargets`, `CustomBuildAfterTargets`, `LinkIncremental`,
  `EnableMicrosoftCodeAnalysis`, `EnableClangTidyCodeAnalysis` and
  `RunCodeAnalysis` `false`, plus conditioned properties for the remaining
  per-configuration values (`TargetName`, Profile `CodeAnalysisRuleSet` and the
  client's Profile clang-tidy `false`, the Release analysis group with its
  existing comment). Each configuration must evaluate every property it
  evaluates today, including the client Release group not setting
  `EnableMicrosoftCodeAnalysis`.
- One unconditional ItemDefinitionGroup with every ClCompile key the three
  configurations share (including the single include list), the shared `Link`
  keys, the `CustomBuildStep` elements and one `PreBuildEvent`; a Debug group;
  a `'$(Configuration)'!='Debug'` group with the optimized-build ClCompile and
  Link keys and the 4702 suppression with its comment; and two small Profile
  and Release groups holding only the configuration define and `EnablePREfast`.
  Defines compose through `%(PreprocessorDefinitions)` as the ModelViewer groups
  do; the resulting define list per configuration must be unchanged.
- `RunCodeAnalysis`: the single unconditional `false` with Release `true` makes
  client and server match; the server's evaluated value does not change because
  the toolset default is already `false`.
- `Pch.cpp`: one unconditional `<PrecompiledHeader>Create</PrecompiledHeader>`
  child, kept on its own line between the four-space open and close tags so
  `Update-EngineVcxprojItem.ps1` still parses the ItemGroup.
- Delete the 17 empty filter declarations, and change the leftover-filter
  sentence in `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`
  `## vcxproj.filters Rules` so it no longer claims an empty filter exists.

The author recommends against a shared `.props` file for the client/server pair:
`Invoke-IncludeOrderCheck.ps1` cannot read an include list from it, Visual
Studio property pages write an edit of an imported value into the `.vcxproj` as
an override (which would let `/fp:strict` or the defines diverge silently), and
`Get-SessionChangeInventory.ps1` classifies a `.props` file as `other`, so
vcxproj review routing would not fire. The existing rule in this directory's
`AGENTS.md` `## Build Configuration` (compile settings mirrored between the two
vcxprojs) already covers the two-copy pair.

Exposure: build configuration of both game executables. `/fp:strict`
(`FloatingPointModel` `Strict`), every define, `TreatWarningAsError`,
`EnablePREfast`, the Release analysis settings, and the PreBuildEvent and
CustomBuildStep text (ThirdParty provisioning and DataPacker export) must be
unchanged per configuration. No determinism/CRC, wire, serialization, `.pack`,
or membership change. Keep the client files' UTF-8 BOM, the server files'
absence of one, and LF line endings.

Risk tier: Tier 2. Trigger: build configuration of both Sandbox executables
(compile and link settings and the data-build PreBuildEvent/CustomBuildStep
plumbing), changed only in layout.

## Critical files

- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj.filters`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj.filters`
- `Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj.filters`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`

## In scope

- Both Sandbox vcxprojs: the `Label="Configuration"` PropertyGroups, the
  `Label="PropertySheets"` ImportGroups, the per-configuration PropertyGroups,
  the three ItemDefinitionGroups, and the `Engine\Source\Pch.cpp` ClCompile
  item's `PrecompiledHeader` children, at the line ranges listed in `## Context`.
- The 17 empty `<Filter Include>` declarations listed in `## Context`, in the
  two Sandbox `.filters` files and `ModelViewer.vcxproj.filters`.
- The empty-leftover-filter sentence in
  `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`
  `## vcxproj.filters Rules`; any other sentence in that file only if the new
  layout makes it false.

## Out of scope

- Any change to an evaluated compile, link, analysis, or build-event setting,
  apart from the inert server `RunCodeAnalysis` alignment.
- A new `.props` or `.targets` file; sharing settings with
  `ModelViewer.vcxproj` or `DataPacker.vcxproj`.
- `ModelViewer.vcxproj` and every ModelViewer setting divergence, including its
  Release 4702 suppression.
- Item membership, filter assignments, non-empty filter declarations and GUIDs,
  and the `.sln` files.
- `.agents/scripts/Invoke-IncludeOrderCheck.ps1` and every
  `.agents/skills/update-vcxproj/scripts/` script, including their stale
  comments and their known ModelViewer handling issues.

## Acceptance criteria

- For each of the six project/configuration pairs, the compiler and linker
  command lines are unchanged: the author recommends comparing each
  configuration's `CL.command.1.tlog` and `link.command.1.tlog` under
  `Build\<ProjectName>\<Configuration>\` from a `/compile` build before the
  change with the same files after it.
- For each of the six pairs, the evaluated values of `UseDebugLibraries`,
  `WholeProgramOptimization`, `OutDir`, `IntDir`, `TargetName`,
  `LinkIncremental`, `CustomBuildAfterTargets`, `EnableMicrosoftCodeAnalysis`,
  `EnableClangTidyCodeAnalysis`, `RunCodeAnalysis`, `CodeAnalysisRuleSet` and
  `CodeAnalysisTreatWarningsAsErrors` are unchanged (the server Debug and
  Profile `RunCodeAnalysis` already evaluate to `false`); the author recommends
  an evaluation-only MSBuild `-getProperty` query.
- The PreBuildEvent command and CustomBuildStep command, message and outputs
  text each appear once per vcxproj and are byte-identical to today's text.
- `Invoke-VcxprojPairCheck.ps1` exits 0 for both Sandbox projects and for
  ModelViewer, run as `/update-vcxproj` documents it.
- `Invoke-IncludeOrderCheck.ps1`, run as `/code-style-review` documents it on
  one client-only, one server-only and one shared Sandbox source file, resolves
  includes without the "No AdditionalIncludeDirectories" error, and the files'
  ItemGroups keep the layout `Update-EngineVcxprojItem.ps1` parses.
- `/compile` builds the client and the server in Debug, Profile and Release.

## Notes

- Expected saving: about 230 lines across the two vcxprojs (about 115 each) and
  about 49 lines of filter declarations.
- `ModelViewer.vcxproj:54-122` is the reference shape. Keep each moved comment
  (the analysis comment in the Release PropertyGroup, the `ClangTidyChecks`
  comment, the 4702 comment) beside its setting.
- No live `/agent-harness` run is needed; the command-line comparison settles
  behavior.
