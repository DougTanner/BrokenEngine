<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-16T13:05:00.441Z","dependsOn":["Documents/Plans/Engine/SharedSessionProtocol.md"]} -->
# Replace the `game::NetworkSessionContract` trait struct and its engine concept with direct names

Tier 2 (`.agents/references/risk-tiers.md`): a behavior-preserving change to the
engine/game Network interface (a public compile-time contract removed) spanning
the Network session runtimes, `GameBase`, and one Graphics header, with no
change to what any surface computes, carries, or trusts. It edits the
frame-stream and status-change codec call sites, so the verification below
proves their bytes and the per-tick CRC unchanged. Line numbers cite baseline
`45b5e347`. Where a statement here and the code disagree, the code wins; report
the contradiction instead of matching one side to the other. Bare `Network/`,
`Game.cpp` paths mean `Projects/BrokenEngineSandbox/Source/<path>`.

## Context

The user's direction this session, quoted by the dispatching session: "Now's the time to do any large refactors, before we create the second game project, NOTE: Avoid 'hooks' the engine can just call functions/globals it expects the game:: to have available." Engine code may name expected `game::` classes, globals, functions, and members; it must not name Sandbox gameplay.

`game::NetworkSessionContract` (`Network/NetworkSessionContract.h:14-40`) is a
trait struct the engine reaches through
`game::NetworkSessionContract::<member>`, checked by the engine concept
`engine::NetworkSessionContractType`
(`Engine/Source/Network/NetworkSessionContract.h:6-22`, included at
`Engine/Source/Engine.h:17`). Of its members, four only forward engine names
back to the engine: `kTickDuration` (`engine::kTickNanoseconds`,
`Engine/Source/Frame/TimeStep.h:9`), `kbDebugFrames` (the
`kbDesynchronizationDebugFrames` switch, which engine code already reads
directly at `ClientDesyncCore.cpp:21`, `ReconcileReplayTick.cpp:203`,
`ServerBroadcaster.cpp:150-151,202`), `CompressStatusChanges`, and
`DecompressStatusChanges`
(`engine::CompressStatusChangeBatch`/`DecompressStatusChangeBatch`). The others
alias `game::Frame`, `game::StatusChange`, `game::GamePacketType` (the
`GamePacket` alias has no reader outside the concept), the constant
`game::kiDesiredCoordinateSlots` (`:11`), and the frame codec
(`rStream << rFrame`, `rFrame.ServerRead(rStream)`). The indirection has no
runtime cost; removing it makes the engine name what it uses.

Call sites:

- `Frame::kiVersion`: `Engine/Source/Network/Client/Client.cpp:1021`, `Engine/Source/Network/Server/Server.cpp:600,603`.
- `kTickDuration`: `Engine/Source/GameBase.cpp:199,326`, `Engine/Source/Network/Client/ClientSessionRuntime.cpp:711`, `Engine/Source/Network/Server/ServerSessionRuntime.cpp:88`.
- `kiCoordinateSlots`: `Engine/Source/Graphics/Islands.h:18`, `Engine/Source/Network/Server/Server.cpp:724`, `Game.cpp:499,595`, `Network/Client/ClientSession.cpp:181`.
- `kbDebugFrames`: `Engine/Source/Network/Server/ServerBufferedFrames.cpp:503`.
- `ReadFrame`: `Client.cpp:391`. `WriteFrame`: `ServerBufferedFrames.cpp:112,180`.
- `CompressStatusChanges`: `ServerBufferedFrames.cpp:62`. `DecompressStatusChanges`: `Client.cpp:583`.

## Design

- Each call site names the target directly, with the same arguments and in the same position: `game::Frame::kiVersion`; `kTickNanoseconds`; `game::kiDesiredCoordinateSlots` (unqualified in the game files); `kbDesynchronizationDebugFrames`; `pFrame->ServerRead(frameStream)`; `mFrameStream << *rFrame.second` and `mFrameStream << *pFrame` (the `game::operator<<` found by argument-dependent lookup); `CompressStatusChangeBatch(...)`; `DecompressStatusChangeBatch(...)`.
- Delete the struct (`Network/NetworkSessionContract.h:13-40`). The file keeps `kiDesiredCoordinateSlots` and its three includes, and stays at `Pch.h:102`. The author recommends keeping the file and its name rather than moving the constant: the `Pch.h` include sequence before `Engine.h` is load-bearing (`Pch.h:97-98`), engine `Islands.h` needs the constant from it, and the file still holds the game Network names the engine session code expects.
- Delete `Engine/Source/Network/NetworkSessionContract.h`, its `Engine.h:17` include, and its membership in both projects.
- Rewrite the docs that name the struct: `Engine/Source/Network/AGENTS.md:40` (the runtimes name `game::Frame`, `game::StatusChange`, `game::GamePacketType`, and `game::kiDesiredCoordinateSlots` directly; keep "no virtual session base, runtime type erasure, or additional global manager") and `:41` (drop "and `game::NetworkSessionContract` wraps the codec"); `Documents/Architecture/Network.md:71,73` (name `game::Frame::kiVersion` and `game::kiDesiredCoordinateSlots`) and `:96`.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/NetworkSessionContract.h`; `Engine/Source/Network/NetworkSessionContract.h` (deleted); `Engine/Source/Engine.h`
- `Engine/Source/Network/Client/Client.cpp`, `ClientSessionRuntime.cpp`; `Engine/Source/Network/Server/Server.cpp`, `ServerSessionRuntime.cpp`, `ServerBufferedFrames.cpp`
- `Engine/Source/GameBase.cpp`; `Engine/Source/Graphics/Islands.h`
- `Projects/BrokenEngineSandbox/Source/Game.cpp`, `Network/Client/ClientSession.cpp`
- Both `.vcxproj`/`.vcxproj.filters` through `/update-vcxproj`
- `Engine/Source/Network/AGENTS.md`, `Documents/Architecture/Network.md`

## In scope

- The call sites listed in `## Context`, at whatever lines they occupy once the prerequisites have landed.
- Removal of `game::NetworkSessionContract`, `engine::NetworkSessionContractType`, the engine header, its `Engine.h` include, and its project membership.
- The four doc lines named under `## Design`.

## Out of scope

- Any change to frame or status-change bytes, the LZ4 framing, `kiProtocolVersion`, `FrameInput::kiVersion`, or `Frame::kiVersion`.
- The value or meaning of `kiDesiredCoordinateSlots`, the `Pch.h` include sequence, and the other includes of the game header.
- The game status-change payload codec (`Network/StatusChangeCodec.cpp`); the session-protocol moves, which `Documents/Plans/Engine/SharedSessionProtocol.md` owns.
- The other engine-to-game indirections recorded in `Documents/Plans/Engine/EngineGameHookSweep.md`.

## Risk triggers and invariants

- Wire and CRC: every replaced call resolves to the same function or constant the struct forwarded to, so frame-stream and status-change bytes are identical and no version moves. If any replacement would change a byte, stop and return it as a decision.
- Affinity: the call sites keep their current `BT_CLIENT`/`BT_SERVER` guards.

## Acceptance criteria

1. `rg -n "NetworkSessionContract::|NetworkSessionContractType|struct NetworkSessionContract" Engine Projects Documents/Architecture -g "*.h" -g "*.cpp" -g "*.md"` prints nothing.
2. `/compile` builds the client and server in Debug and Release with no new warnings; `/update-vcxproj` validation passes.
3. An `/agent-harness` replay determinism check on a Debug build reports an identical CRC on every tick.
4. A live `/agent-harness` session with a connected client that crosses cells logs no frame version mismatch, CRC mismatch, or desync.

## Notes

- Depends on `Documents/Plans/Engine/SharedSessionProtocol.md`, which reworks the session runtimes, `Client.cpp`, `Server.cpp`, and `GameBase.cpp`, lists the game `Network/NetworkSessionContract.h` among its critical files, and rewrites `Network/AGENTS.md` and `Network.md` session text; this mechanical rename is cheaper to apply after it than to have that Plan's citations go stale. `kbDesynchronizationDebugFrames` is already engine-owned (`Engine/Source/EngineSwitches.h`).
