<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:32:52.866Z","dependsOn":[]} -->
# Move the Server Monitor's mimalloc statistics off ProfileManager

## Context

Found by an architecture review of `Engine/Source/Server` (dependency and
cohesion lenses) and verified at commit `64ec627b`. Pre-existing debt; no
session changed these lines.

`ProfileManager` carries four public members that only the Server Monitor
host uses:

- `miMimallocCommittedMebibytes`, `miMimallocPeakCommittedMebibytes`,
  `miMimallocHeapUsedMebibytes`, and `miMimallocPeakHeapUsedMebibytes` are
  declared at `Engine/Source/Profile/ProfileManager.h:428-431`, inside the
  `#if defined(BT_SERVER) && !defined(ENABLE_CRT_DEBUG_HEAP)` block at
  `:427`/`:432`.
- They are written only in the file-static `UpdateStatistics`
  (`Engine/Source/Server/ServerMonitorHost.cpp:42-45`) and read only in
  `ServerMonitorHost::Capture` (`ServerMonitorHost.cpp:127`). A search of the
  whole worktree for `miMimalloc` finds no other use, and the server
  `query_profile` handler (`CommandQueryProfile`,
  `Engine/Source/Agent/AgentCommandsServerGeneric.cpp:175-223`) does not
  return them.
- `ProfileManager.h` is aggregated by `Engine.h` into the PCH of both the
  client and the server project, so private Server Monitor state sits on
  another subsystem's public interface.

Neither `Engine/Source/Server/AGENTS.md` nor `Engine/Source/Profile/AGENTS.md`
records this placement as deliberate. The finding rests on the members
having one user, in another subsystem. `Engine/Source/AGENTS.md`
`## Hub Conventions` (Internals) is only the nearest related rule: it covers
heavy private state whose header changes often, which these four values are
not.

Unmet criterion: Server Monitor private state lives with its only user, not
on `ProfileManager`.

## Design

Recommended: declare the four values as file-static variables in
`ServerMonitorHost.cpp`, beside `sTotalEntityCounts` (`:16`) and under the
same `!defined(ENABLE_CRT_DEBUG_HEAP)` condition the code that uses them
already has, point the four writes in `UpdateStatistics` and the read in
`Capture` at them, and delete the block at `ProfileManager.h:427-432`.
Rationale: it is the smallest move, it mirrors the existing
`sTotalEntityCounts` pattern in the same file, and it changes no behavior.

Computing the values inside `Capture` instead would also remove the state,
but it changes when `mi_stats_get` runs, which is the subject of the
dependent Plan
`Documents/Plans/Engine/ServerMonitorProfilingDisabledSnapshot.md`; this
Plan leaves that timing alone.

Before deleting the members, the executing session searches the private
Coretide game project (present only in the primary checkout) for
`miMimalloc`. A search of its `.h` and `.cpp` files at commit `64ec627b`
found no use. If a use exists at execution time, stop and return the Plan
for re-planning.

## Critical files

- `Engine/Source/Server/ServerMonitorHost.cpp`
- `Engine/Source/Profile/ProfileManager.h`

## In scope

- `ProfileManager.h`: delete the four `miMimalloc*Mebibytes` members and
  their `#if`/`#endif` pair (`:427-432`).
- `ServerMonitorHost.cpp`: four new file-static variables beside
  `sTotalEntityCounts`; the four assignments in `UpdateStatistics`
  (`:42-45`); the `std::format` arguments in `Capture` (`:127`).

## Out of scope

- When or how often `mi_stats_get` runs, and the `kbProfiling` gate in
  `UpdateStatistics`.
- `sTotalEntityCounts` and the entity-total loop.
- The memory section's text, color, and position in the snapshot.
- Any other `ProfileManager` member.
- `Engine/Source/Server/AGENTS.md` and `Engine/Source/Profile/AGENTS.md`:
  neither mentions the members, so neither becomes incorrect.

## Risk and invariants

Change Workflow Tier 2 — trigger: it removes public members from a header
`Engine.h` aggregates, which is a public interface change, so the Tier 1
"no public signature exposure" condition does not hold. Behavior is
unchanged.

- No determinism, CRC, wire, serialization, or trust exposure: the snapshot
  bytes are identical.
- Threading is unchanged: both the writes and the read stay on the main
  thread inside `ServerMonitorHost::Update`.
- Build: `ProfileManager.h` is in the PCH, so both the client and the server
  rebuild; the `ENABLE_CRT_DEBUG_HEAP` configuration must still compile with
  the variables absent.

## Acceptance criteria

The diff and a clean client and server build settle this Plan:

- A worktree-wide search for `miMimalloc` finds nothing outside
  `ServerMonitorHost.cpp`.
- The memory section `Capture` formats is byte-identical in text to the
  baseline's.

## Notes

- `Documents/Plans/Engine/ServerMonitorProfilingDisabledSnapshot.md` depends
  on this Plan because both rewrite `ServerMonitorHost.cpp:36-46` and
  `:126-130`.
