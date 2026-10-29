# Projects - Engine Applications

## Overview

Each directory under `Projects/` is an engine application. Game projects use `game::`;
`ModelViewer` is a standalone application whose build explicitly uses
BrokenEngineSandbox data. Its application contracts belong to
[ModelViewer/AGENTS.md](ModelViewer/AGENTS.md).

## Standard Project Layout

A game project is a `Projects/<Name>` directory holding both
`Platforms/VisualStudio2026/<Name>.vcxproj` (client) and `<Name>Server.vcxproj`
(server). `Get-AgentGameProjects` in `.agents/scripts/AgentScriptCommon.psm1`
and DataPacker's zero-argument discovery (`DataPacker/Source/Main.cpp`) apply
the same vcxproj test and ordinal ordering; a change to the discovery rule
changes both. ModelViewer has no server project, so it is not a game project:
`/compile` builds it as its own target and `/update-vcxproj` keeps its
selective membership.

A game project provides:

- `Projects/<Name>/Platforms/VisualStudio2026/`: `<Name>.sln` and `<Name>.vcxproj` (client), `<Name>Server.sln` and `<Name>Server.vcxproj` (server), each `.vcxproj` with its adjacent `.vcxproj.filters`, and the generated `Output/Data`
- `Projects/<Name>/Data/` — required: DataPacker reads it as the game project's data input; `Data/Shaders/` holds the game project's shader sources
- `Projects/<Name>/Source/Pch.h`
- `Projects/<Name>/Source/Frame/Collections/`
- `Projects/<Name>/Documents/AgentHarness.md`

The shared checkers `.agents/scripts/Invoke-CollectionLayoutCheck.ps1` and
`.agents/scripts/Invoke-IncludeOrderCheck.ps1`, `/update-vcxproj`'s
`Update-EngineVcxprojItem.ps1`, `/compile`, the wrapper session-start data
refresh (`.agents/scripts/Bootstrap-AgentTools.ps1`), the `/agent-harness`
claim, DataPacker's zero-argument run, diagsession symbolization, and
code-quality metrics history derive each game project's paths from its name. A
new game project that follows this layout needs no change to that tooling.

## See Also

- Agent command field rules: `BrokenEngineSandbox/Source/Agent/AGENTS.md`
- Agent game contract: a game project provides the names `../Engine/Source/Agent/AGENTS.md` `## Game contract` lists, and may take over an engine agent command (`## Architecture` there)
