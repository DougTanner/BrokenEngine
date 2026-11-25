<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-25T00:02:03.587Z","dependsOn":["Documents/Plans/Engine/RemoveFlightPlanes.md"]} -->
# Exclude vendored submodules and non-shader Data assets from agent search

## Context

The repository root `.ignore` file currently holds a single negation line
(`!/Projects/Coretide/`). Because `.gitignore` tracks `ThirdParty/` submodules
and `Data/` assets, ripgrep-based agent search tools (Claude Code `Grep` and
`Glob`, Codex search, and standalone `rg`) scan two noisy trees by default:

- In the primary checkout, all 24 materialized `ThirdParty/` submodules contain
  16,028 files (6,600 code/text files across Assimp, OpenEXR, SPIRV-Cross,
  ImGui, DirectXTK, GLM, zlib, and others). Linked worktrees already skip them
  because their submodule entries are directory symlinks, creating an
  inconsistency where primary-checkout searches flood context with upstream
  code agents must not edit.
- In every checkout, `Projects/BrokenEngineSandbox/Data/Models/` holds 14
  tracked `.gltf` JSON files (up to 93.7 KB each, dense with common engine
  terms such as `position`, `normal`, `material`, `matrix`, `node`, `mesh`,
  `scale`, `rotation`, `texture`, `color`, and `camera`), and `Engine/Data/`
  and `Projects/*/Data/` hold over 1,000 tracked binary and intermediate asset
  files (`.MODEL`, `.PreExport`, `.BC*`, `.R16*`, `.r32`, `.png`, `.wav`,
  `.bin`, `BakedIsland.json`, etc.) that pollute `Grep` and `Glob` results.

No tracked files outside `Engine/Data/`, `Projects/*/Data/`, and `ThirdParty/`
use those asset extensions or `BakedIsland.json`.

## Design

Extend root `.ignore` to exclude vendored `ThirdParty/` submodule directories
and raw/intermediate asset files from default agent search while keeping
Broken Engine's own wrappers, shaders, `AGENTS.md` files, and authored metadata
(`*.model.json`, `Island.json`) searchable:

1. Preserve `!/Projects/Coretide/` as the first rule in `.ignore`.
2. Add directory exclusions for upstream third-party trees:
   - `/ThirdParty/*/` (directory-only pattern, so `ThirdParty/AGENTS.md` stays
     searchable)
   - `!/ThirdParty/Prebuilts/` (keeps Broken Engine's own third-party wrappers,
     config shims, `.vcxproj`, and `AGENTS.md` searchable)
3. Add asset-file exclusions for raw and intermediate data files so shaders
   (`**/Shaders/**`), `**/AGENTS.md`, `*.model.json`, and `Island.json` remain
   searchable without per-project path maintenance:
   - `*.gltf`, `*.glb`, `*.fbx`, `*.bin`, `*.MODEL`, `*.PreExport`,
     `*.BC4_UNORM_BLOCK`, `*.BC5_UNORM_BLOCK`, `*.BC7_UNORM_BLOCK`,
     `*.R16G16B16A16_SFLOAT`, `*.r32`, `*.png`, `*.jpg`, `*.jpeg`, `*.tga`,
     `*.ktx`, `*.wav`, `*.otf`, `*.zip`, `*.terrain`, `*.docx`,
     `BakedIsland.json`
4. Update the `ThirdParty` search bullet in root `AGENTS.md` `## Environment`
   to note that `.ignore` excludes upstream `ThirdParty/*/` directories (keeping
   `ThirdParty/AGENTS.md` and `ThirdParty/Prebuilts/`) and raw/intermediate
   `Data/` assets from default `Glob`/`Grep`/`rg` searches in both primary and
   worktree checkouts, and that explicit-path reads, `Select-String`, or
   `rg --no-ignore` (plus `-L` in worktrees) still reach them when needed.

## Critical files

- `.ignore`
- `AGENTS.md`

## In scope

- `.ignore`: add `/ThirdParty/*/`, `!/ThirdParty/Prebuilts/`, and the asset
  extension and `BakedIsland.json` exclusion rules listed in `## Design`,
  preserving `!/Projects/Coretide/`.
- `AGENTS.md` (`## Environment`): update the existing `ThirdParty` search bullet
  to reflect the `.ignore` exclusions across primary and worktree checkouts.

## Out of scope

- Modifying `.gitignore`, `.gitattributes`, `.gitmodules`, or worktree
  sparse-checkout rules in `.agents/scripts/Start-AgentWorktreeSession.ps1`.
- Excluding `Projects/BrokenEngineSandbox/Data/Models/` from Git sparse
  checkout (ModelViewer reads and writes `.model.json` sidecars there).
- Changing any C++, GLSL, DataPacker, or build files.

## Risk triggers and invariants

- Tier 1 (mechanical agent-tooling configuration and root `AGENTS.md` prose).
- Invariant: `!/Projects/Coretide/` must remain active in `.ignore`.
- Invariant: `ThirdParty/AGENTS.md`, `ThirdParty/Prebuilts/**`,
  `Engine/Data/Shaders/**`, `Projects/*/Data/Shaders/**`, `**/AGENTS.md`,
  `*.model.json`, and `Island.json` must remain visible to default `Glob` and
  `Grep` searches.
- Invariant: `git status` and Git tracking behavior must be unchanged (`.ignore`
  affects ripgrep-based search tools only, never Git).

## Acceptance criteria

- `.ignore` preserves `!/Projects/Coretide/`, excludes `/ThirdParty/*/` while
  re-including `!/ThirdParty/Prebuilts/`, and excludes the asset extensions and
  `BakedIsland.json` listed in `## Design`.
- `git status --ignored` shows no change in Git-ignored or tracked file sets.
- A default directory search (`Glob` / `Grep` or `rg --files`) from repository
  root returns matches under `ThirdParty/Prebuilts/`, `ThirdParty/AGENTS.md`,
  `Engine/Data/Shaders/`, and `Projects/BrokenEngineSandbox/Data/Models/`
  (`*.model.json`), and returns zero matches under `ThirdParty/assimp/`,
  `ThirdParty/imgui/`, `*.gltf`, `*.MODEL`, or `BakedIsland.json`.
- Root `AGENTS.md` `## Environment` accurately describes the `.ignore` behavior
  and passes `Invoke-StaticChecks.ps1` (including prose line-length checks).
