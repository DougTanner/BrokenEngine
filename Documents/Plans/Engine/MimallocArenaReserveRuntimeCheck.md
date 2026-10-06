<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-06T01:50:19.158Z","dependsOn":[]} -->
# Verify the mimalloc process-init arena reserve at runtime

## Context

Commit 9daa482c moved the fixed 10 GiB mimalloc arena reserve into mimalloc's own process init: `ThirdParty/Prebuilts/Source/Engine/Mimalloc.cpp` defines `MI_DEFAULT_RESERVE_OS_MEMORY` and `MI_DEFAULT_ARENA_EAGER_COMMIT` before including mimalloc, and `MemoryInitializer::MemoryInitializer` (`Engine/Source/Memory/GlobalAllocator.cpp:246-252`) only verifies arena 0 through `mi_arena_area(1, ...)` and warns on refusal. After landing, the primary ThirdParty Debug/Profile/Release libraries rebuilt, DataPacker Release and the client and server Debug|x64 built against them, and the DataPacker executable carries no mimalloc message prefix.

The landed Plan's runtime acceptance check was never run. `/agent-harness` claiming failed with `claim.pack-version-mismatch`: `Common/DataFile.h:429` derives pack version 430 since commit 216639e3 (the AnimationHeader shrink), but all seven manifests in the primary Shared `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Output/Data` still carried 429. The user chose to defer the check until the shared game data is refreshed. Per `.agents/skills/compile/references/runtime-data-mode.md` `## Wrapper bootstrap Shared-data refresh`, a new wrapper session start re-exports the primary Shared data.

Change Workflow tier: **Tier 1**. Trigger: verification only; no tracked change is expected. Any fix this check proves necessary is a separate change routed under `## Design` step 4, classified on its own.

## Design

1. Run in a fresh wrapper session, whose start performs the Shared-data refresh. Rebuild the Debug|x64 server and client through `/compile`, stating the agent-harness scenario trigger.
2. If the harness claim still fails `claim.pack-version-mismatch`, stop and report the blocker to the user. Do not generate or export data from this session.
3. Through `/agent-harness`, run four scenarios, each ending with `quit` so `~MemoryInitializer` logs its shutdown line (`GlobalAllocator.cpp:286`):
   - server idle: start the game and let it tick, then quit;
   - server workload: `replay_record`, `spawn_players`, and `inject_outward_transfer` (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`), the workload the earlier reserve runs used, then quit;
   - client idle, connected to a server, then quit;
   - client connected to the server while it runs the workload above, then quit.
   Read each process's shutdown line `Mimalloc peak heap usage: ..., peak committed: ..., arenas: ... (after reserve: ...), arena reserve: ... MiB` from its captured log output. The line exists only in Debug, where `kbMimallocDiagnostics` is true (`Projects/BrokenEngineSandbox/Source/Pch.h:30`).
4. If any run fails the acceptance criteria, present the user a choice between reverting the memory part of 9daa482c and a fix Plan through `/create-follow-up-plans`. Do not fix it in this change.

When all four runs meet the acceptance criteria, an empty realized change is the expected outcome.

Exposure: runtime allocation and startup of both executables only. No determinism/CRC, serialization, `.pack`/`kiVersion`, replay format, wire, or threading exposure.

## Critical files

- `Engine/Source/Memory/GlobalAllocator.cpp` (read only)
- `ThirdParty/Prebuilts/Source/Engine/Mimalloc.cpp` (read only)

## In scope

- The four `/agent-harness` runs in `## Design` step 3 against the Debug|x64 server and client, and their shutdown memory lines.

## Out of scope

- Any edit to `GlobalAllocator.cpp`, `Mimalloc.cpp`, `Engine/Source/Memory/AGENTS.md`, or upstream `ThirdParty/mimalloc/`.
- Generating or exporting game data, and pack version changes.
- mimalloc's default arena purge (`Documents/Investigations/MimallocArenaPurge.md`).
- The reserve size, the commit-everything policy, and the continue-on-refusal policy, which are fixed user decisions.

## Acceptance criteria

- Each of the four runs logs at shutdown peak committed >= 10240 MiB, the decisive reserve-success signal, together with `arenas: 1 (after reserve: 1)` and `arena reserve: 10240 MiB`, and does not hit the arena-growth `DEBUG_BREAK` (`GlobalAllocator.cpp:289-292`).
- A refused reserve can also log `arenas: 1 (after reserve: 1)`, so peak committed below 10240 MiB means the reserve failed; report that, not arena overflow.

## Notes

- Origin: unrun runtime acceptance check (3) of the change landed as 9daa482c (Plan `Documents/Plans/Engine/MimallocArenaReserveInProcessInit.md`, completed by that commit), deferred by the user until the shared game data is refreshed.
- Recording session: client `claude`; worktree/branch UUID `9ea538a0-3063-4771-bcb7-e33a7401e7b7`; session branch `claude/9ea538a0-3063-4771-bcb7-e33a7401e7b7`; worktree locator `.claude/worktrees/BrokenEngine/9ea538a0-3063-4771-bcb7-e33a7401e7b7`; conversation session ID `3213d2ee-4585-4fe6-8eab-d138fe1f2cbe`; landing ref 9daa482c.
