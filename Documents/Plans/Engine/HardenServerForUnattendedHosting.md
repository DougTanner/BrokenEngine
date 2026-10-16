<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-12T22:40:24.056Z","dependsOn":[]} -->
# Harden the dedicated server for unattended internet hosting

## Context

This is the first of four Plans that take the dedicated server from the current code to a real, internet-reachable Azure deployment. The other three are `Documents/Plans/ChangeWorkflow/ProvisionAzureServerVm.md`, `Documents/Plans/Engine/ConnectClientToHostedServer.md`, and `Documents/Plans/ChangeWorkflow/DeployAndRunHostedServer.md`, and each depends on the one before it. They replace the deleted Azure hosting guide (formerly under `Documents/`), whose deployment steps the code could not support.

User decisions (this session, binding):

- The deployment chain runs after the current Plans by depending only on `Documents/Plans/Game/CollectionTimesToChrono.md`. The user chose this over depending on every end-of-chain Plan, knowing this chain may run before ShipTypeSplit and the flight-plane chain. Only this Plan carries that edge.
- The audience is anyone on the internet: the game port UDP 27015 is open to every source.

Main-session decisions the user did not object to (these are not user decisions):

- The server-side handshake deadline (gap G12) is included, because the game port is public.
- Unattended dialogs (gap G2) get no new flag: the deployed launch reuses `--agent-port`, which `DeployAndRunHostedServer.md` records.

Gap evidence, verified against the code at baseline `89640522`:

- G3, no session-end handling. `WindowProcedure` (`Engine/Source/Main.cpp:622`) handles neither `WM_QUERYENDSESSION` nor `WM_ENDSESSION`. The server quits only on `WM_CLOSE` or `WM_DESTROY` (`:795-808`), Debug F4 (`:692-702`, gated by `kbDebugInput`), or the agent `quit` command (`Engine/Source/Agent/AgentCommandsShared.cpp:107-111`). The final autosave runs only after a normal main-loop exit (`Main.cpp:489-494`). Logoff, OS shutdown, and a VM stop therefore skip it, and the world falls back to the last hourly autosave (`kAutosaveInterval = 3'600s`, `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.h:28`). Microsoft's `WM_ENDSESSION` reference (https://learn.microsoft.com/en-us/windows/win32/shutdown/wm-endsession) says of `wParam` TRUE: "the session can end any time after all applications have returned from processing this message". Its companion page (https://learn.microsoft.com/en-us/windows/win32/shutdown/shutting-down) says: "Applications should perform any required cleanup while processing **WM_ENDSESSION**." Setting `gbQuit` and returning is therefore not enough: the save must finish before the handler returns.
- G8, game-port bind failure is not fatal. `Server::Server` (`Engine/Source/Network/Server/Server.cpp:28-47`) logs a warning and returns when `enet_host_create` fails (`:37-42`), leaving `mpHost` null. `Flush` (`:67`) and `Poll` (`:78`) then return early, so the process runs with no network. `ServerSessionRuntime`'s constructor already expects `Server` construction to throw: it catches, closes its timer, and rethrows (`Engine/Source/Network/Server/ServerSessionRuntime.cpp:24-33`).
- G12, no handshake deadline. `Server::Connect` (`Server.cpp:164-182`) adds a `ClientConnection` with `bHandshakeComplete = false` (`Server.h:52`) for every ENet connect, out of 64 slots (`Server.cpp:37`). Pre-handshake gated packets drop silently (`Documents/Architecture/Network.md:59,62`). Nothing removes a peer that keeps its ENet session alive but never sends Hello, so 64 such peers hold every slot indefinitely.

## Design

This Plan closes gaps G3, G8, and G12, the three whose failure an unattended public server cannot recover from by its launch arguments: G3 loses up to one `kAutosaveInterval` of world on every session end, G8 leaves a running server with no network, and G12 lets anonymous peers hold every slot. `## Out of scope` lists the other gaps and which of them the later Plans cover.

### G3: session end runs the final autosave inside the handler

- `Main.cpp` server section: move the post-loop autosave block (`:489-494`) into one file-static `FinalAutosave()`. It returns at once when a file-static `sbFinalAutosaveDone` is set. Otherwise it sets the flag and calls `game::gpGame->mGameSaveLoad.Autosave()`, logging the existing `"Final autosave failed during shutdown"` error on failure. The post-loop site calls `FinalAutosave()`.
- `WindowProcedure`, server cases: add `WM_ENDSESSION`. When `uiWordParameter` is nonzero (the session is ending), log `LOG(kDefault, kInfo, "WM_ENDSESSION: final autosave")`, set `gbQuit = true`, and call `FinalAutosave()` when `game::gpGame != nullptr`. Return `0`, the documented return value. A `FALSE` `wParam` does nothing.
- Why the null check: `ProcessMessages` also runs in the `destroyWindow` teardown drain (`Main.cpp:236-252`). On exception unwinding that drain runs after the game is destroyed, so it is a reachable state. On a normal exit the drain finds `sbFinalAutosaveDone` already set.
- Why the save is consistent: the other `ProcessMessages` callers run at a tick boundary with the game constructed. These are the main-loop top (`:384`), boot after game creation (`:367`), and the post-loop drain (`:505`), which comes after `FinalAutosave()`. That is the same state the post-loop autosave saves from. A cross-process sent message is dispatched only inside these message-retrieval calls. The grid-save write is atomic, a temporary file renamed over the target (`Engine/Source/File/GridSave.cpp:19`, `Engine/Source/File/FileManager.cpp:359-376`), so a process terminated mid-write keeps the previous autosave.
- This Plan does not handle `WM_QUERYENDSESSION`, and `DefWindowProc` keeps returning TRUE (allow). A query can be followed by `WM_ENDSESSION` with `wParam` FALSE when the shutdown is cancelled, and quitting on the query would stop the server for a shutdown that never happens. The user's request named both messages. This Plan meets it by quitting and saving on the one message that confirms the session ends.

### G8: bind failure throws

In `Server::Server`, replace the warning and return (`Server.cpp:38-42`) with `throw std::runtime_error(std::format("Server::Server enet_host_create failed to bind UDP port {}", iPort))`. The exception follows the existing path: `ServerSessionRuntime` rethrows, `ProcessMain`'s `catch (const std::exception&)` calls `HandleException`, and the crash report holds the message. `--agent-port` already suppresses that path's dialog (`Engine/Source/CrashReport.cpp:114-115`). A non-null `mpHost` is now a constructor postcondition, so delete the null checks it makes unreachable: `~Server` (`:51`), `Flush` (`:67-70`), and `Poll` (`:78-81`). The handled-exception exit code stays `0`. The `crash_report_harness_rig` contract documents exit code 0 (`.agents/skills/agent-harness/references/command-reference.md:18`), and a supervisor of the deployed server keys on process presence, not exit codes.

### G12: handshake deadline

- Constant: `constexpr std::chrono::seconds kHandshakeTimeout = 10s;` beside `kBudgetStallGrace` (`Server.cpp:24`).
- `ClientConnection` (`Server.h:35-77`) gains `std::chrono::steady_clock::time_point handshakeDeadline {};`. `Server::Connect` sets it to `std::chrono::steady_clock::now() + kHandshakeTimeout`.
- `Server::Poll` runs one pass over `mClients` from the last index down to 0, after the `enet_host_service` drain and the delayed-packet processing (`:116-139`) and before `mPreviousPollEnd` (`:144`). Every client with `!bHandshakeComplete` and `now >= handshakeDeadline` gets three steps. First, `LOG(kNetwork, kWarning, "Server::Poll Handshake deadline Disconnecting Client: {}", iClientId)`. Then `enet_peer_disconnect(pPeer, 0)`, the same graceful disconnect `RecordContractViolation` uses (`:438`). Last, `RemoveClient(iClientId)`. Each `RemoveClient` call also runs the game client manager's `OnClientDisconnected` hook, which is a no-op for a client that never finished the handshake and never touches `mClients`. The pass records no contract violation, because silence is protocol state, not corruption (`Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy`). Iterating downward keeps the swap-and-pop `RemoveClient` (`:344-357`) from skipping an element.
- Why 10 s:
  - The check runs after the poll drains every queued event. A Hello that already arrived is therefore processed before its deadline is tested, so a server-side stall cannot expire a legitimate client.
  - The client sends Hello as a reliable packet in the same `Client::Poll` that observes its ENet connect (`Engine/Source/Network/Client/Client.cpp:111-116,939-958`).
  - ENet retransmits a lost reliable packet starting from a 500 ms round-trip estimate with backoff (`ENET_PEER_DEFAULT_ROUND_TRIP_TIME`, `ThirdParty/enet/include/enet/enet.h:214`). Ten seconds covers several retransmissions on a lossy internet path.
  - Ten seconds is twice the longest existing client protocol timeout: `kSubscriptionTransitionTimeout` (`Engine/Source/Network/Client/ClientSubscriptions.cpp:10`) and `kDesyncDebugTimeout` (`ClientDesyncCore.h:52`), both 5 s.
  - A hostile peer's slot hold becomes at most 10 s plus ENet's disconnect completion. A peer that never acknowledges is reset by ENet's timeout, `ENET_PEER_TIMEOUT_MAXIMUM` 30 s (`enet.h:226`). Before this change the hold was unbounded.
- The later ENet `DISCONNECT` event finds no record (gate 1) and only purges that peer's delayed packets (`Server.cpp:184-203`), as for a violation disconnect.
- No wire format, packet, or client behavior changes, so `kiProtocolVersion` is unchanged.
- `Documents/Architecture/Network.md` `## Client → Server Contract` gains a **Handshake deadline** bullet after **Pre-handshake silent drop**. It states the rule (10 s from ENet connect, graceful disconnect, no violation count) and that the deadline is the only removal path for a silent peer. `Engine/Source/Network/Server/AGENTS.md` `## Session Invariants` gains one clause on the deadline after its accept-a-client bullet. Both are documents the change makes incomplete. `Network.md` owns the protocol contract (`Documents/AGENTS.md` `## Reference Material`).

Risk tier: Tier 3. Triggers:

- Trust boundary: a new server-side admission rule for unauthenticated internet peers, and the documented client-to-server contract.
- Process-lifecycle and save timing: a grid save is written from a new call site at session end.
- Both touch engine Network and the engine process entry.

Not triggered: determinism and CRC (no simulation change), and wire, save, or replay layout (no format change).

## Critical files

- `Engine/Source/Main.cpp`
- `Engine/Source/Network/Server/Server.cpp`, `Engine/Source/Network/Server/Server.h`
- `Documents/Architecture/Network.md`
- `Engine/Source/Network/Server/AGENTS.md`

## In scope

- `Main.cpp`: the server-only `FinalAutosave()` helper and its `sbFinalAutosaveDone` flag, the post-loop autosave site (`:489-494`), and the server `WM_ENDSESSION` case in `WindowProcedure`.
- `Server.cpp`: the `Server::Server` bind failure (`:38-42`), the null checks it makes unreachable in `~Server`, `Flush`, and `Poll`, the `kHandshakeTimeout` constant, `Server::Connect`'s deadline assignment, and the deadline pass in `Server::Poll`.
- `Server.h`: `ClientConnection::handshakeDeadline`.
- `Network.md` `## Client → Server Contract`: the Handshake deadline bullet. `Engine/Source/Network/Server/AGENTS.md` `## Session Invariants`: the deadline clause. Any further AGENTS.md wording `/update-claude-docs` finds stale.

## Out of scope

- `WM_QUERYENDSESSION` handling, `ShutdownBlockReasonCreate`, and client (`BT_CLIENT`) session-end handling.
- The handled-exception exit code (stays `0`), the duplicate-instance exit code (`Main.cpp:848-862`), and every other exit code `ProcessMain` returns.
- Gap G9: `ProcessMain` returns `0` when `ParseLaunchOptions` fails (`Main.cpp:823-826`). Only a supervisor that reads exit codes would need a nonzero code, and this chain has none: the G8 design keys on process presence, and `DeployAndRunHostedServer.md`'s scheduled task does not wait on the server.
- Client-side `enet_host_create` failure (`Client.cpp:31-36`), and the discovery responder's bind (`Engine/Source/Network/NetworkDiscoveryResponder.cpp`).
- Per-source connection limits or flood throttling beyond the deadline. Authentication of the client GUID.
- Any wire, packet, or contract-table row change, and `kiProtocolVersion`.
- Gaps G2 (no new non-interactive flag), G4, G5, G6 (backup pruning), G7, G10 (minidumps), G11, G13, G14, and G16 from the gap survey. The later Plans in this chain own G5, G7, G11, and G13 by launch arguments, Release-only deployment, and an IP literal.
- Visual Studio remote debugging, IPv6, DNS names, and load testing.

## Acceptance criteria

- `/compile` builds `BrokenEngineSandbox` client and server in Debug and Release x64 with Shared data and no new warnings. Static checks pass.
- G8: launch the Debug server with `--agent-port <free port> --log-file <Temp path> --app-data-directory <empty Temp directory>` while a PowerShell `System.Net.Sockets.UdpClient` holds UDP 27015. The process exits on its own with no dialog. Its crash report under the app-data directory and its log both contain `failed to bind UDP port 27015`. With the port free, the same launch serves a harness client normally.
- G3: in one Debug server run with `--agent-port` and an empty Temp `--app-data-directory`, use a Temp PowerShell script that calls Win32 `SendMessage` on the server's top-level window (class `Broken Engine Sandbox Server`, owned by the launched PID).
  1. Sending `WM_ENDSESSION` with `wParam` 0 leaves the server running, with no autosave written.
  2. Sending `WM_ENDSESSION` with `wParam` 1 returns only after `ServerAutosave.save` exists under `<app-data-directory>\Broken Engine Sandbox Server\`. The process then exits on its own with exit code `0`.
  3. Afterwards that directory holds exactly one `ServerAutosave*.save` file, so no backup was made and the post-loop path did not save a second time. The log shows `WM_ENDSESSION: final autosave` and no `Final autosave failed`.
  4. Relaunching with the same app-data directory loads that autosave.
- G12, legitimate path: a harness client connects to a local Debug server, stays connected for more than 20 s, and the server log has no `Handshake deadline` line.
- G12, silent peer: no existing harness command reaches a peer that completes the ENet connect and never sends Hello, so this path is verified by code reading plus the build, with no temporary source edit (`.agents/references/change-workflow.md` `#### Step 5 — Run targeted pre-review checks`); the execution card states this. The reading confirms that `Server::Connect` sets `handshakeDeadline` to the connect time plus `kHandshakeTimeout`, and that every `Server::Poll` runs the deadline pass after the event drain, visiting `mClients` downward. For each client with `!bHandshakeComplete` past its deadline, the pass logs `Server::Poll Handshake deadline Disconnecting Client: <id>`, calls `enet_peer_disconnect(pPeer, 0)`, and calls `RemoveClient`, with no contract violation. The diff contains no client change.
- `Network.md` and `Engine/Source/Network/Server/AGENTS.md` state the deadline rule, and `/update-claude-docs` reports no stale reference.

## Coordination

- `Documents/Plans/Engine/EngineOwnedSaveSettingsAndSwitches.md` moves `GameSaveLoad`'s autosave entry points and `kAutosaveInterval` into `engine::ServerSaveLoad` (`Engine/Source/File/ServerSaveLoad.h/.cpp`), owned as `GameBase::mServerSaveLoad`, and changes the post-loop autosave call in `Main.cpp` to `pGame->mServerSaveLoad.Autosave()`. Whichever lands second rebases onto the other's edits; landed after that Plan, `FinalAutosave()` calls `game::gpGame->mServerSaveLoad.Autosave()`, and the `kAutosaveInterval` citation names `ServerSaveLoad.h`.
