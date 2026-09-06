<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T21:35:19.666Z","dependsOn":[]} -->
# Split the packet fault fixtures out of AgentCommandsServer.cpp

## Context

`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp` is over the
10,000 `bt-token-v1` reduction threshold for a `.cpp` file and has been for at
least one landed change.

Measurements from `pwsh -NoProfile -Command "& '.agents/scripts/Measure-Tokens.ps1' -Path '<paths>' -Json"`:

- Baseline `37fe4867cfcd80c24bd764ee0d5643910b7f453e` (extracted with
  `git show 37fe4867cfcd80c24bd764ee0d5643910b7f453e:Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp`):
  1,122 lines, 40,859 bytes, **10,215** tokens — already above the threshold.
- Head of the network corruption-response change: 1,180 lines, 43,406 bytes,
  **10,852** tokens.

That change added 58 lines (`CommandEnginePacketFaultFixture` plus its
dispatcher entry); it did not put the file over the threshold, so the size is a
pre-existing residual recorded here rather than a defect of that change.  No
acceptance criterion was unmet, and no reduction was prescribed — a
`/repo-code-review` size observation only.

The file already has a sibling precedent: the frame-read query commands live in
`AgentCommandsServerQueries.h` / `AgentCommandsServerQueries.cpp` (236 + 1,700
tokens), whose header comment states that the remaining sim-control, injection,
and dispatcher commands stay in `AgentCommandsServer.cpp`.

## Design

Recommended approach, because it reuses the existing precedent exactly and moves
one cohesive cluster rather than scattering ownership: run `/reduce-file` as the
standalone reduction route on
`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp`, with the
packet fault fixtures as the author's recommended boundary.

The recommended boundary is the two adjacent fault-fixture command handlers,
currently lines 118-277 of the head file (1,577 `bt-token-v1`):

- `CommandGamePacketFaultFixture` — builds malformed `mReceivedGamePackets`
  entries and drives `gpServerSession->ParseReceivedGamePackets()`.
- `CommandEnginePacketFaultFixture` — pushes a malformed `kClientAckStream`
  packet through `engine::gpServer->Receive()`.

They form one responsibility (inject a malformed inbound packet through the real
receive path and report what was injected), share no helper with the rest of the
file, and neither is called from anywhere except the `ExecuteAgentCommandServer`
dispatcher.

Recommended shape, mirroring the Queries pair: a new
`AgentCommandsServerFaultFixtures.h` / `.cpp` pair in the same directory,
`#pragma once` plus a `#if defined(BT_SERVER)` guard, declaring the two handlers
in `namespace game` with the existing signatures, and a header comment stating
what the pair owns and what stays behind — as `AgentCommandsServerQueries.h`
does.  `AgentCommandsServer.cpp` keeps its two `if (cmd == ...)` dispatcher
entries and includes the new header.

Expected result: `AgentCommandsServer.cpp` drops to roughly 9,300
`bt-token-v1`, below the 10,000 threshold, with the new `.cpp` around 1,600.

Exposure: behavior-preserving code motion inside one server-only translation
unit.  No wire format, serialization, `.pack`/`kiVersion`, determinism/CRC,
replay, threading, or allocation surface changes; the two handlers keep their
current parameter validation, so the agent-command trust boundary is unchanged.
The moved handlers change from internal (anonymous-namespace) linkage to
declared `namespace game` functions, which is exactly what the Queries split
already did.  The implementing session must also update the fixed includes each
file needs after the move.

The harness command documentation under
`Projects/BrokenEngineSandbox/Documents/AgentHarness/` is unaffected: the command
names, parameters, and results do not change.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:118-277` —
  the two fault-fixture handlers to move.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:1079-1089` —
  the dispatcher entries that stay and call the moved handlers.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.h`,
  `.cpp` — the sibling-pair precedent to mirror.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj`
  and `.filters` — server-only membership for the new pair, reconciled through
  `/update-vcxproj`, never hand-edited.

## In scope

- Create `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerFaultFixtures.h`
  and `AgentCommandsServerFaultFixtures.cpp`.
- Move `CommandGamePacketFaultFixture` and `CommandEnginePacketFaultFixture`,
  with their leading comments, verbatim out of `AgentCommandsServer.cpp` into the
  new `.cpp`, and declare them in the new header.
- Adjust the `#include` lines of `AgentCommandsServer.cpp` and the new `.cpp` to
  what each file needs after the move.
- Add the new pair to the server project and filters through `/update-vcxproj`.

## Out of scope

- Any change to the behavior, parameters, validation messages, or JSON results of
  the two handlers.
- Moving, renaming, or reorganizing any other command handler, helper
  (`PathToUtf8`, `IsWindowsReservedDeviceBasename`, `BareFilenameParam`,
  `CoordFromParam`), or the `ExecuteAgentCommandServer` dispatcher structure
  beyond the added `#include`.
- Client-side `AgentCommandsClient.cpp` and the shared `AgentCommands.h`.
- Changes to the network receive paths the fixtures drive
  (`engine::Server::Receive`, `ServerSession::ParseReceivedGamePackets`).
- Documentation of the harness commands under
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/`.

## Acceptance criteria

- `pwsh -NoProfile -Command "& '.agents/scripts/Measure-Tokens.ps1' -Path 'Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp' -Json"`
  reports at or below 10,000 `bt-token-v1`.
- The server target `BrokenEngineSandboxServer` compiles through `/compile`.
- `game_packet_fault_fixture` and `engine_packet_fault_fixture` still dispatch
  and return their existing result fields, verified through `/agent-harness`
  against a server with exactly one handshaken client.

## Notes

Change Workflow tier: **Tier 1** — mechanical, project membership plus local
behavior-preserving motion within one server-only subsystem, with no public
signature or invariant exposure (root `AGENTS.md` risk trigger "Tier 1 —
mechanical").  A reviewer may escalate if the implementing session finds the
move cannot preserve behavior with the includes and linkage available.

Reduction route: `/reduce-file`, standalone mode
(`.agents/skills/reduce-file/SKILL.md`), which owns measurement, boundary
analysis, and the `/update-vcxproj` and `Build required` requests.

Line numbers cited above are from the network corruption-response head; re-measure
and re-locate before implementing.
