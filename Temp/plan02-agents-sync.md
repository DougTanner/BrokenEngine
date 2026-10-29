# Plan 02 AGENTS synchronization

## Scope and result

Sync mode; baseline b56f20fa44d0424c2fdf5003157563684ed747f5; authoritative 210 entries in Temp/plan02-inventory.json plus Temp/plan02-validator.md Modified ranges. Discovery passed for all entries in bounded batches; 41 governing documents were read. No AGENTS edits are necessary. Preserve the prior C3 Frame bounds correction.

Publication, preparation submission, capture fence/export, visible-only terrain, shared ray offset, material query policy, gameplay zero stub, original sprite texture use, and ordinary-RAM allocation policy match the current implementation. The seven applied removals remove private stored state or unused parameters and add no durable invariant. The shared TerrainVertexCoordinates extraction preserves existing documented coordinate rules.

## Governing documents

- AGENTS.md
- Common/AGENTS.md
- DataPacker/Source/AGENTS.md
- DataPacker/Source/ExportJobs/AGENTS.md
- Documents/AGENTS.md
- Documents/Investigations/AGENTS.md
- Engine/Data/AGENTS.md
- Engine/Data/Shaders/AGENTS.md
- Engine/Data/Shaders/Lighting/AGENTS.md
- Engine/Data/Shaders/Model/AGENTS.md
- Engine/Data/Shaders/Objects/AGENTS.md
- Engine/Data/Shaders/Quads/AGENTS.md
- Engine/Data/Shaders/Terrain/AGENTS.md
- Engine/Data/Shaders/Water/AGENTS.md
- Engine/Source/Agent/AGENTS.md
- Engine/Source/AGENTS.md
- Engine/Source/Frame/AGENTS.md
- Engine/Source/Frame/Collections/AGENTS.md
- Engine/Source/Frame/Collections/AreaLights/AGENTS.md
- Engine/Source/Frame/Collections/HexShields/AGENTS.md
- Engine/Source/Frame/Collections/PointLights/AGENTS.md
- Engine/Source/Graphics/AGENTS.md
- Engine/Source/Graphics/Managers/AGENTS.md
- Engine/Source/Graphics/Objects/AGENTS.md
- Engine/Source/Graphics/Render/AGENTS.md
- Engine/Source/Profile/AGENTS.md
- Engine/Source/Ui/AGENTS.md
- Engine/Source/Ui/Screens/AGENTS.md
- Engine/Source/Ui/Screens/TweaksScreen/AGENTS.md
- Projects/AGENTS.md
- Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md
- Projects/BrokenEngineSandbox/Source/AGENTS.md
- Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md
- Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md
- Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/AGENTS.md
- Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/AGENTS.md
- Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/AGENTS.md
- Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/AGENTS.md
- Projects/BrokenEngineSandbox/Source/Ui/AGENTS.md
- Projects/BrokenEngineSandbox/Source/Ui/Screens/AGENTS.md
- Projects/ModelViewer/AGENTS.md

## Advisory sizes

The discovery script reports these retained advisory excesses. Scope authorizes accuracy synchronization only; no unrelated trimming was performed. Values are current bt-token-v1 tokens / advisory budget.

- Common/AGENTS.md: 2292 / 2267
- DataPacker/Source/AGENTS.md: 2912 / 2063
- DataPacker/Source/ExportJobs/AGENTS.md: 4500 / 2838
- Engine/Data/Shaders/Water/AGENTS.md: 1668 / 1317
- Engine/Source/Agent/AGENTS.md: 2735 / 1849
- Engine/Source/AGENTS.md: 3846 / 3529
- Engine/Source/Frame/AGENTS.md: 4126 / 3021
- Engine/Source/Graphics/AGENTS.md: 3880 / 2641
- Engine/Source/Graphics/Managers/AGENTS.md: 5735 / 4345
- Engine/Source/Graphics/Render/AGENTS.md: 1854 / 1830
- Engine/Source/Ui/AGENTS.md: 2021 / 1844
- Engine/Source/Ui/Screens/AGENTS.md: 1580 / 1423
- Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md: 2414 / 2003
- Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md: 1953 / 1673
- Projects/ModelViewer/AGENTS.md: 3768 / 1000

## Static checks

Invoke-StaticChecks.ps1 -RepositoryRoot C:/Users/dougt/Documents/BrokenEngine -Baseline b56f20fa44d0424c2fdf5003157563684ed747f5 -IncludeUntracked: exit 1. validate-skill passes (2 packages); markdown-links passes (80 links); line-length passes (38 files, limit 500); file-hygiene fails only Engine/Data/Shaders/Terrain/TerrainVertex.h:5, indent with spaces. Main independently observed the same failure and is assigning its fix. No AGENTS file fails. git diff --check passes.

A search for removed lighting pipeline names and seven-removal members in AGENTS.md files returned no matches. No compile, runtime, unit tests, branch changes, commits, or subagents were used.
