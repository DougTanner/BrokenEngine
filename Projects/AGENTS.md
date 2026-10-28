# Projects - Engine Applications

## Overview

Each directory under `Projects/` is an engine application. Games use `game::`;
`ModelViewer` is a standalone application whose build explicitly uses
BrokenEngineSandbox data. Its application contracts belong to
[ModelViewer/AGENTS.md](ModelViewer/AGENTS.md).

## Standard Project Layout

Project discovery includes every `Projects/<Name>` directory holding
`Platforms/VisualStudio2026/<Name>.vcxproj`. `Get-AgentGameProjects` in
`.agents/scripts/AgentScriptCommon.psm1` and DataPacker's zero-argument discovery
(`DataPacker/Source/Main.cpp`) apply the same vcxproj test and ordinal ordering;
a change to the discovery rule changes both.

`Projects/<Name>/Data/` identifies a project's own DataPacker inputs.
DataPacker's zero-argument run and the wrapper session-start data refresh
(`.agents/scripts/Bootstrap-AgentTools.ps1`) skip projects without that directory
before packing or changing export stamps. Explicit DataPacker invocations still
require their input directories. An application without `Data/` supplies data
routing through its build; discovery and Data presence do not imply a server
target or automatic engine-file membership.

Client/server games additionally provide:

- `Projects/<Name>/Platforms/VisualStudio2026/`: `<Name>.sln` and `<Name>.vcxproj` (client), `<Name>Server.sln` and `<Name>Server.vcxproj` (server), each `.vcxproj` with its adjacent `.vcxproj.filters`, and the generated `Output/Data`
- `Projects/<Name>/Data/`; `Data/Shaders/` holds the game's shader sources
- `Projects/<Name>/Source/Pch.h`
- `Projects/<Name>/Source/Frame/Collections/`
- `Projects/<Name>/Documents/AgentHarness.md`

Ordinary games retain the client/server pair requirement regardless of `Data/`
presence. `/compile` selects ordinary games separately from its dedicated
ModelViewer target. `/update-vcxproj`'s `Update-EngineVcxprojItem.ps1` enrolls
engine files in game pairs; ModelViewer retains its dedicated, selective
membership and is excluded from that enrollment and the game-pair requirement.

The shared checkers `.agents/scripts/Invoke-CollectionLayoutCheck.ps1` and
`.agents/scripts/Invoke-IncludeOrderCheck.ps1`, `/compile`, `/update-vcxproj`,
the wrapper data refresh, the `/agent-harness` claim, DataPacker's zero-argument
run, diagsession symbolization, and code-quality metrics history derive project
paths from discovery, applying their respective requirements above. A new game
following the standard layout needs no change to that tooling.

## See Also

- Agent command field rules: `BrokenEngineSandbox/Source/Agent/AGENTS.md`
- Agent game contract: a game project provides the names `../Engine/Source/Agent/AGENTS.md` `## Game contract` lists, and may take over an engine agent command (`## Architecture` there)
