# Projects - Game Implementations

## Overview

Each directory under `Projects/` is one game project (`game::`) built on the engine.

## Standard Project Layout

A game project is a `Projects/<Name>` directory holding `Platforms/VisualStudio2026/<Name>.vcxproj`. The tooling named below finds game projects from this layout and derives each path from the name instead of naming a project:

- `Projects/<Name>/Platforms/VisualStudio2026/`: `<Name>.sln` and `<Name>.vcxproj` (client), `<Name>Server.sln` and `<Name>Server.vcxproj` (server), each `.vcxproj` with its adjacent `.vcxproj.filters`, and the generated `Output/Data`
- `Projects/<Name>/Data/` — required: DataPacker reads it as the project's data input and fails without it; `Data/Shaders/` holds the project's shader sources
- `Projects/<Name>/Source/Pch.h`
- `Projects/<Name>/Source/Frame/Collections/`
- `Projects/<Name>/Documents/AgentHarness.md`

That tooling is the shared checkers `.agents/scripts/Test-CollectionLayout.ps1`
and `.agents/scripts/Test-IncludeOrder.ps1`, `/update-vcxproj`'s
`Update-EngineVcxprojItem.ps1`, `/compile`, the wrapper session-start data
refresh (`.agents/scripts/Bootstrap-AgentTools.ps1`), the `/agent-harness`
claim, DataPacker's zero-argument run, diagsession symbolization, and the
code-quality metrics history. `Get-AgentGameProjects` in
`.agents/scripts/AgentScriptCommon.psm1` and the project discovery of
DataPacker's zero-argument run (`DataPacker/Source/Main.cpp`) apply the same
vcxproj test, so a change to the rule changes both. A new game project that
follows this layout needs no change to that tooling.

## See Also

- Agent command field rules: `BrokenEngineSandbox/Source/Agent/AGENTS.md`
