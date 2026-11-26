<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-25T18:56:18.957Z","dependsOn":["Documents/Plans/Engine/FixEngineClientServerAffinityMismatches.md"]} -->
# Add Client/Server Affinity and Header-Reachability Checks to Static Checks

## Context

The repository already has `.agents/scripts/SourceAffinity.psm1`
(`Resolve-FileAffinity`) and
`.agents/skills/update-vcxproj/scripts/Invoke-VcxprojPairCheck.ps1`, plus the
Change Workflow static-checks runner `.agents/scripts/Invoke-StaticChecks.ps1`.
However, none of them currently catch three classes of client/server project
drift that accumulated in `BrokenEngineSandboxServer.vcxproj`:

1. A one-sided `.vcxproj` member whose source/header preprocessor guard does
   not match its project membership (for example,
   `Engine/Source/Audio/AudioManager.cpp` placing `#if defined(BT_DEBUG)` before
   `#if defined(BT_CLIENT)` so `Resolve-FileAffinity` fails to classify it as
   `client`).
2. A whole-file `BT_CLIENT`-guarded file listed in a server `.vcxproj` or a
   whole-file `BT_SERVER`-guarded file listed in a client `.vcxproj`.
3. A `<ClInclude>` listed in a game project's client or server `.vcxproj` that
   no `<ClCompile>` in that project transitively includes under that project's
   target define (`BT_CLIENT` or `BT_SERVER`), such as the ten client-only UI
   headers that remained in `BrokenEngineSandboxServer.vcxproj` with zero
   server includers.

## Design

The recommended approach is to extend
`.agents/skills/update-vcxproj/scripts/Invoke-VcxprojPairCheck.ps1` and
`.agents/scripts/Invoke-StaticChecks.ps1` using
`.agents/scripts/SourceAffinity.psm1`:

1. Add a reusable affinity-and-reachability verification check (either inside
   `Invoke-VcxprojPairCheck.ps1` for paired game projects discovered via
   `Get-AgentGameProjects` in `.agents/scripts/AgentScriptCommon.psm1`, or in a
   shared `.psm1` called by both `Invoke-VcxprojPairCheck.ps1` and
   `Invoke-StaticChecks.ps1`) that validates, for each game project's client
   and server `.vcxproj` pair:
   - Every one-sided `<ClCompile>` and `<ClInclude>` member — excluding
     generated `$(GameDataDirectory)\*.h` headers, deliberate documented
     exceptions such as `Mimalloc.cpp` or force-included PCH headers, and the
     game project's `Projects/<Name>/Data/Shaders/ShaderLayouts.h` wrapper
     (which uses `#if defined(BT_ENGINE)` to separate C++ from GLSL rather than
     `BT_CLIENT` from `BT_SERVER` and is included by shared `Pch.h` while listed
     only as a client `<ClInclude>`) — matches `Resolve-FileAffinity`:
     client-only members resolve to `client`, and server-only members resolve to
     `server`.
   - No file that `Resolve-FileAffinity` classifies as `client` appears in the
     server `.vcxproj`, and no file it classifies as `server` appears in the
     client `.vcxproj`.
   - Every `<ClInclude>` in the client or server `.vcxproj` (excluding
     `$(GameDataDirectory)\*.h` generated headers) is transitively reachable
     from at least one `<ClCompile>` of that `.vcxproj` when traversing
     `#include "..."` directives with `Pch.h` (force-included) and `Engine.h`
     evaluated under the project's target define (`BT_CLIENT` or `BT_SERVER`).
2. Wire the check into `.agents/scripts/Invoke-StaticChecks.ps1` so it triggers
   whenever a session adds, deletes, renames, or edits any `.vcxproj`, `.h`, or
   `.cpp` under a game project, `Engine/Source/`, or `Common/` (staying
   read-only and respecting the runner's JSON schema, exit codes, and
   `-Head`/`-IncludeUntracked` semantics).
3. Update `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`
   (`## vcxproj File Inclusion Rules`) to document the client-only `<ClInclude>`
   exception for the game project's `Data/Shaders/ShaderLayouts.h` wrapper, and
   update `.agents/skills/update-vcxproj/SKILL.md` (and its skill references if
   needed), script header comments, and affected `AGENTS.md` files to document
   the new check without restating script mechanics in prose.

## Critical files

- `.agents/scripts/SourceAffinity.psm1`
- `.agents/scripts/Invoke-StaticChecks.ps1`
- `.agents/skills/update-vcxproj/scripts/Invoke-VcxprojPairCheck.ps1`
- `.agents/skills/update-vcxproj/SKILL.md`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`
  (`## vcxproj File Inclusion Rules`)

## In scope

- Implementing the three affinity and `<ClInclude>` reachability checks for
  paired game client/server `.vcxproj` files using `SourceAffinity.psm1`,
  encoding the exemption for the game project's `Data/Shaders/ShaderLayouts.h`
  wrapper so it does not false-positive.
- Integrating the check into `Invoke-VcxprojPairCheck.ps1` and
  `Invoke-StaticChecks.ps1` (preserving read-only behavior, JSON output
  contract, and `Get-AgentGameProjects` discovery).
- Updating `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`
  (`## vcxproj File Inclusion Rules`) to document the client-only `<ClInclude>`
  exception for the game project's `Data/Shaders/ShaderLayouts.h` wrapper, plus
  `.agents/skills/update-vcxproj/SKILL.md` (and its skill references if needed),
  script header comments, and affected `AGENTS.md` files for the new check.

## Out of scope

- Modifying C++ source files or `.vcxproj` membership (existing violations are
  resolved before this Plan runs).
- Checking `ModelViewer.vcxproj`, `DataPacker.vcxproj`, or `Tools/` projects
  for client/server pairing (they have no server counterpart).
- Adding unit tests or C++ compiler invocations to `Invoke-StaticChecks.ps1`.

## Acceptance criteria

- Running `Invoke-VcxprojPairCheck.ps1` on the clean tree passes with zero
  violations (including zero false positives on
  `Projects/BrokenEngineSandbox/Data/Shaders/ShaderLayouts.h`).
- Running `Invoke-StaticChecks.ps1` on a diff touching a `.vcxproj`, `.h`, or
  `.cpp` triggers the new check and passes on the clean tree.
- Verification (via temporary verification edit or dry-run check) confirms each
  of the three failure modes is caught:
  (a) a one-sided `.vcxproj` member whose guard does not match
      `Resolve-FileAffinity`,
  (b) a whole-file `BT_CLIENT`-guarded file listed in the server `.vcxproj`,
  (c) an unreachable `<ClInclude>` listed in the server `.vcxproj`.

## Notes

- Change Workflow tier: Tier 2 (scoped tool/workflow behavior — adds a new
  deterministic check to `Invoke-StaticChecks.ps1` and
  `Invoke-VcxprojPairCheck.ps1`; no runtime engine, determinism/CRC, wire,
  serialization, or bootstrap-lock surface).
