# Projects - Game Implementations

## Overview

Each directory under `Projects/` is one game project (`game::`) built on the engine.

## Standard Project Layout

The shared checkers `.agents/scripts/Test-CollectionLayout.ps1` and `.agents/scripts/Test-IncludeOrder.ps1` discover game projects from this layout instead of a project path:

- `Projects/<Name>/Platforms/VisualStudio2026/<Name>.vcxproj`
- `Projects/<Name>/Source/Pch.h`
- `Projects/<Name>/Source/Frame/Collections/`

A new game project that follows this layout needs no change to those two checkers. This covers only those two checkers.

## See Also

- Agent command field rules: `BrokenEngineSandbox/Source/Agent/AGENTS.md`
