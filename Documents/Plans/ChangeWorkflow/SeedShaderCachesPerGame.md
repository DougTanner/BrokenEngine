<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-16T14:15:38.865Z","dependsOn":[]} -->
# Seed the harness shader caches for every game project at session start

## Context

When the wrapper creates a session worktree, `.agents/scripts/Start-AgentWorktreeSession.ps1:149-169` seeds the shared harness app-data root `Temp\AppData` with `pipeline.cache` and `BrdfLut.cache`, so the first client launch skips their cold rebuild. The block hardcodes the Sandbox client's app-data child: `$cacheDirectory = 'AppData\Broken Engine Sandbox'` (`:155`) and `Join-Path $env:APPDATA "Broken Engine Sandbox\$cacheName"` (`:159`).

That child name is not the project directory name. The client process appends
`game::kGameName` to its app-data root
(`Engine/Source/File/FileManager.cpp:87`), and the Sandbox defines the client
value `"Broken Engine Sandbox"` in the `BT_CLIENT` branch of
`Projects/BrokenEngineSandbox/Source/Game.h:24-28` (the server value,
`"Broken Engine Sandbox Server"`, is the other branch). Repository tooling now
finds every game project through `Get-AgentGameProjects` in
`.agents/scripts/AgentScriptCommon.psm1` (`Projects/AGENTS.md`
`## Standard Project Layout`), but that layout yields only `<Name>`, never the
display name. So a second game project gets no seeded caches.

The gap was found as an out-of-scope residual of the change that made repository tooling discover game projects by the Standard Project Layout. That change excluded this seed (branch `claude/965c0c6c-0185-449d-8bf2-5c1e0bbcf561`).

Impact: speed only. The runtime validates both caches when it loads them and regenerates them on any mismatch (comment at `:150-153`). A missing seed costs one cold rebuild on a second game's first launch, and nothing is ever incorrect.

## Design

1. Read each game's client display name from its own source. The recommended
   source is the client `kGameName` literal in `Projects/<Name>/Source/Game.h`,
   the value the runtime itself uses for the app-data child. Match the literal
   that directly follows `#if defined(BT_CLIENT)`, so the server branch is never
   picked. A second copy of the name in a project doc (for example the
   `Projects/<Name>/Documents/AgentHarness.md` assignment block) is not
   recommended: it would duplicate `kGameName` and could drift from it.
2. In the seeding block, loop over `Get-AgentGameProjects $root` (the module is
   already imported at `:27`). For each project whose client name resolves, seed
   `Temp\AppData\<client name>\` from the same two sources in today's order: the
   primary checkout's `Temp\AppData\<client name>\<cache>`, then
   `%APPDATA%\<client name>\<cache>`. Each file is still taken independently,
   and a file present in neither source stays cold. If a project's name does not
   resolve, skip that project and emit one `Write-Warning` naming it.
3. Keep the whole block, project enumeration included, inside the existing `try`/`catch`, so no seeding failure can abort session start. Keep the `if ($worktreeCreated)` gate.
4. Add `Source/Game.h` (the client `kGameName` in its `BT_CLIENT` branch) to the layout list in `Projects/AGENTS.md` `## Standard Project Layout`, and add the session-start cache seed to that section's list of consuming tooling.

Change Workflow tier: Tier 3. Trigger (`.agents/references/risk-tiers.md`): build/bootstrap coordination that can block other sessions. `Start-AgentWorktreeSession.ps1` runs from the primary checkout at every wrapper session start, so a parse error or an uncaught throw there aborts session start for every later session.

Exposure: no determinism/CRC, serialization, `.pack`, replay, wire, threading, shader, or C++ build surface. The script only copies files into the new worktree's `Temp`.

## Critical files

- `.agents/scripts/Start-AgentWorktreeSession.ps1` (`:149-169`)
- `.agents/scripts/AgentScriptCommon.psm1` (`Get-AgentGameProjects`, read only)
- `Projects/BrokenEngineSandbox/Source/Game.h` (`:24-28`, read only)
- `Projects/AGENTS.md` (`## Standard Project Layout`)

## In scope

- The `if ($worktreeCreated)` cache-seeding block in `Start-AgentWorktreeSession.ps1` (`:149-169`): per-project enumeration and client-name resolution, as Design items 1-3 describe.
- `Projects/AGENTS.md` `## Standard Project Layout`: the `Source/Game.h` layout entry and the consuming-tooling list entry (Design item 4).

## Out of scope

- Any C++ change, including `game::kGameName`, `FileManager`, and how the runtime validates or regenerates the caches.
- Seeding the server app-data child, or any file besides `pipeline.cache` and `BrdfLut.cache`.
- `Get-AgentGameProjects` and the Standard Project Layout's vcxproj test.
- The Sandbox-specific prose in `Projects/BrokenEngineSandbox/Documents/AgentHarness/launch.md:111`, which stays accurate for the Sandbox.
- Creating a second game project.
- Any other part of `Start-AgentWorktreeSession.ps1`.

## Acceptance criteria

1. On a Sandbox-only tree, with `pipeline.cache` and `BrdfLut.cache` present under the primary checkout's `Temp\AppData\Broken Engine Sandbox`, a worktree newly created through the wrapper holds both files under `Temp\AppData\Broken Engine Sandbox` with the primary's bytes, and the wrapper emits no seeding warning.
2. A code read shows that the seeding block takes its project list from `Get-AgentGameProjects` and takes each client name from that project's `Source/Game.h` `BT_CLIENT` literal, and that no Sandbox name or path literal remains in the block.
3. A code read shows that enumeration, name resolution, and copying all stay inside the existing `try`/`catch`.

## Notes

- `Documents/Plans/Engine/AgentHarnessIntoEngine.md` adds a `## Launch values` table whose entries include the client app-data child name. That table is documentation for human launch recipes, not a machine-readable source, and this Plan does not read it.
