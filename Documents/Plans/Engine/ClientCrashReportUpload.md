<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-12T22:52:18.092Z","dependsOn":[]} -->
# Upload a client crash report to the connected server

## Context

A client crash today writes a report file on the client machine only (`engine::HandleException`, `Engine/Source/CrashReport.cpp`); nothing reaches the server. The user asked that the client send crash reports to the server it is connected to, reliably, and then directed: "save safely and cleanly to disk first, then attempt a quick-upload if there is a valid server socket".

Facts the design rests on:

- `HandleException` has three callers: `ProcessMain`'s two catches (`Engine/Source/Main.cpp` around lines 911-926, main thread), the SIGABRT handler installed in `Engine/Source/Memory/GlobalAllocator.cpp` (any thread, heap possibly corrupt), and `Engine/Source/Agent/Commands/CrashReportHarnessRig.cpp` (main thread, inside the main loop's agent drain).
- By the time `ProcessMain`'s catch runs, `MainThread` has unwound: `game::Game` (which owns `ClientSessionRuntime` and its `std::unique_ptr<Client>`, `ClientSessionRuntime.h:83`) has already run `~Client` (graceful disconnect), and `NetworkManager` (`Main.cpp:150`) has deinitialized ENet. No connection exists at that point, so the upload must run inside `MainThread`'s main loop scope, before unwinding.
- ENet is main-thread-only and allocates through plain `malloc` on every send (`ThirdParty/enet/packet.c:22`, `ThirdParty/enet/peer.c:160`). The SIGABRT path therefore cannot upload safely.
- `engine::gpClient` (`Client.h:170`) is the live client peer; `Client::mStateFlags` carries `ClientStateFlags::kConnected`; `Client::mpHost` / `mpServerPeer` are the ENet host and server peer.
- Server admission: `Server::Receive` (`Server.cpp:206-342`) applies the per-update packet/byte budget (`kiMaximumClientInboundBytesPerTick = 64 KiB`, `NetworkProtocol.h:146`), with a handshaken-subscribe exemption at `Server.cpp:230`, then the contract row from `GetClientPacketContract` (`NetworkProtocol.h:171-184`), then dispatch.

## Design

### Client crash path

1. **Re-entry guard and remembered path in `HandleException`** (`CrashReport.cpp`). A file-static `constinit std::atomic<bool>` is exchanged to `true` at entry; if it was already `true`, return immediately. This keeps a second entry — `ProcessMain`'s catch after the rethrow below, or SIGABRT raised during the upload — from overwriting the already-written report. `HandleException` also stores the path it selected (one of the existing fixed buffers or `kpcFallbackReportPath`) in a file-static `const wchar_t*`. Both are allocation-free, preserving the `## Crash Reporting` rule in `Engine/Source/AGENTS.md`.
2. **`TryUploadCrashReport()`** — new client-only (`BT_CLIENT`) free function declared in `CrashReport.h`, defined in `CrashReport.cpp`. The whole body is inside `try { ... } catch (...) { }` so it can never escalate the crash. It returns without action when no report path was stored, when `gpClient` is null, or when `gpClient` lacks `kConnected`. Otherwise it reads the stored report file in binary, keeping at most `kiMaximumCrashReportSize` bytes from the start (recommended 1 MiB: the header, exception text, and callstack are at the top of the report, while the full file can reach tens of MiB from the log dump; 1 MiB stays far below ENet's 32 MiB packet cap), under `ScopedSuppressAllocationTracking`, and calls `gpClient->UploadCrashReport(bytes)`.
3. **`Client::UploadCrashReport(std::span<const uint8_t>)`** (`Client.h` / `Client.cpp`). Writes a `ClientCrashReportMessage`, sends it with `NetworkManager::SendPacket` on `kiChannelReliable` with `ENET_PACKET_FLAG_RELIABLE`, then loops `enet_host_service(mpHost, &event, remaining)` until a bounded deadline (recommended 5 s: long enough for a 1 MiB reliable transfer over an ordinary connection, short enough not to stall the crashing process noticeably). On `ENET_EVENT_TYPE_RECEIVE` it checks whether the first byte is `kServerCrashReportReceived`, then always calls `enet_packet_destroy`; every other received packet is discarded, because the session is ending. The loop returns `true` as soon as `kServerCrashReportReceived` arrives, stops with `false` on `ENET_EVENT_TYPE_DISCONNECT`, and stops with `false` at the deadline. The loop bypasses `Client::Poll` and the network-simulation delay queue on purpose. `TryUploadCrashReport` logs the outcome once at `kInfo` (acknowledged) or `kWarning` (not acknowledged).
4. **Main-loop catch in `MainThread`** (`Main.cpp`, client build only). Wrap the existing `while (true)` main loop in `try`. Add `catch (const std::exception& rException)` and `catch (...)`. Each one, when no debugger is attached (`IsDebuggerPresent() == 0`), calls `HandleException` (with `&rException` in the first) and then `TryUploadCrashReport()`. Each then rethrows with `throw;` in all cases. Unwinding, `ProcessMain`'s catch (now a guarded no-op), the exit code, and debugger behavior stay exactly as today. The server build keeps its loop unchanged.
5. **Agent crash rig** (`CrashReportHarnessRig.cpp`): call `TryUploadCrashReport()` between `HandleException()` and `ExitProcess(0)`, so the existing harness command exercises the upload. The rig is compiled into both the client and server projects, so the added call sits inside `#if defined(BT_CLIENT)`; the server rig keeps its current behavior.

### Protocol

- `NetworkProtocol.h` `PacketType`: insert `kClientCrashReport` and `kServerCrashReportReceived` immediately before `kGamePacketStart`. Add both to `PacketTypeName`. Bump `kiProtocolVersion` from 21 to 22 (`Engine/Source/Network/AGENTS.md` `## Transport Contracts` requires it for an added engine packet type).
- `NetworkProtocol.h`: add `kiMaximumCrashReportSize = 1 MiB`.
- `NetworkMessages.h`: add `ClientCrashReportMessage` (`keType`, `kiMinSize = kiPacketTypeSize + 1`, `kiMaxSize = kiPacketTypeSize + kiMaximumCrashReportSize`, one `PacketPayload report`; `Visit` writes the type, then `Payload`). The reader sets `report.iSize` to the packet size minus `kiPacketTypeSize` before `NetworkMessages::Read`. Add `ServerCrashReportReceivedMessage` (`kiFixedSize = kiPacketTypeSize`, type only, shaped like `ClientResyncRequestMessage`).
- `GetClientPacketContract`: row for `kClientCrashReport`: `iMinimumSize = kiMinSize`, `iMaximumSize = kiMaxSize`, `iMaximumPerTick = 1`, handshake required, over-cap counts as a violation.
- `Server::Receive` `bBudgetExempt` (`Server.cpp:230`): also exempt a handshaken `kClientCrashReport`, because one report exceeds the 64 KiB per-update byte budget. Update that block's comment. Gates 3-5 still bound size and per-update count.
- `Documents/Architecture/Network.md` `## Client → Server Contract`: add the row and the budget exemption, and change the current wire-protocol version sentence from 21 to 22.

### Server

- `Server`: add `int64_t miStoredCrashReportCount = 0` and `kiMaximumStoredCrashReports` (recommended 32 per server run, so clients cannot fill the disk however many times they connect or send; 32 MiB worst case). Together with the contract row's one-per-update cap, this is the server's only bound on reports; there is no per-connection limit.
- `Server::ClientCrashReport(std::span<const uint8_t>, ClientConnection&)`, dispatched from the `Server::Receive` switch:
  - Parse with `NetworkMessages::Read`.
  - If `miStoredCrashReportCount >= kiMaximumStoredCrashReports`, log one `kWarning` and return without acknowledging.
  - Otherwise write the payload bytes verbatim with `gpFileManager->WriteFileAtomically({FileFlags::kAppDataDirectory, FileFlags::kWrite}, ...)`, under `ScopedSuppressAllocationTracking`. Filename: `CrashReport-<UTC yyyyMMddTHHmmssZ>-<miStoredCrashReportCount>-<guid uiHigh hex><guid uiLow hex>.txt`, placed in the app-data root.
  - On success, increment the count, log one `kInfo` line with the client id and filename, and send `ServerCrashReportReceivedMessage` on `kiChannelReliable` with `ENET_PACKET_FLAG_RELIABLE`, following the `ServerUnsubscribeAckMessage` send at `Server.cpp:751-753`. On write failure, log `kWarning` and do not acknowledge.
- The payload is opaque text written as received. The server never parses or executes it, so validation is the contract size bound plus the per-run cap.
- The application-level acknowledgment is kept, rather than inferring delivery from an ENet `enet_peer_disconnect_later` completion, for two reasons. It confirms the server stored the report, not just that ENet received it. And it survives server-side network simulation, whose disconnect handling purges the peer's delayed packets (`Engine/Source/Network/Server/AGENTS.md` `## Affinity and Lifetime`); that would drop a still-delayed report.

## Critical files

- `Engine/Source/CrashReport.h`
- `Engine/Source/CrashReport.cpp`
- `Engine/Source/Main.cpp`
- `Engine/Source/Agent/Commands/CrashReportHarnessRig.cpp`
- `Engine/Source/Network/NetworkProtocol.h`
- `Engine/Source/Network/NetworkMessages.h`
- `Engine/Source/Network/Client/Client.h`
- `Engine/Source/Network/Client/Client.cpp`
- `Engine/Source/Network/Server/Server.h`
- `Engine/Source/Network/Server/Server.cpp`
- `Documents/Architecture/Network.md`
- `Engine/Source/AGENTS.md`
- `Engine/Source/Network/Server/AGENTS.md`
- `.agents/skills/agent-harness/references/command-reference.md` (conditional; see `## In scope`)

## In scope

- `Engine/Source/CrashReport.h`: declare `TryUploadCrashReport` (client only).
- `Engine/Source/CrashReport.cpp`: `HandleException` re-entry guard and stored path; new `TryUploadCrashReport`.
- `Engine/Source/Main.cpp`: `MainThread`'s client main loop `try`/catches as described. No other region.
- `Engine/Source/Agent/Commands/CrashReportHarnessRig.cpp`: the one added `TryUploadCrashReport()` call, inside `#if defined(BT_CLIENT)`.
- `Engine/Source/Network/NetworkProtocol.h`: `PacketType`, `PacketTypeName`, `kiProtocolVersion`, `kiMaximumCrashReportSize`, `GetClientPacketContract` row.
- `Engine/Source/Network/NetworkMessages.h`: `ClientCrashReportMessage`, `ServerCrashReportReceivedMessage`.
- `Engine/Source/Network/Client/Client.h` / `Client.cpp`: `Client::UploadCrashReport` only.
- `Engine/Source/Network/Server/Server.h` / `Server.cpp`: `miStoredCrashReportCount`, `kiMaximumStoredCrashReports`, `Server::ClientCrashReport`, its `Server::Receive` dispatch case, and the `bBudgetExempt` line and comment.
- Docs: `Documents/Architecture/Network.md` `## Client → Server Contract` (the row, the budget exemption, and the current wire-protocol version 21 → 22); `Engine/Source/AGENTS.md` `## Crash Reporting` (upload after the write, re-entry guard, SIGABRT stays disk-only); `Engine/Source/Network/Server/AGENTS.md` `## Session Invariants` (per-run cap, acknowledgment only after a successful write); `.agents/skills/agent-harness/references/command-reference.md` crash-rig row, only if it describes the rig's behavior.
- Mechanical necessities only: includes, declarations, and any exhaustive switch over `PacketType` the compiler flags.

## Out of scope

- Retrying a failed upload on the next launch (store-and-forward).
- Uploading from the SIGABRT path or from any non-main thread; that path stays disk-only.
- Server crash reports (the server has no upstream server).
- Compressing the report, splitting it into chunks, or sending the part beyond the first `kiMaximumCrashReportSize` bytes.
- Any change to the report's content, its local file paths, the save-to-desktop prompt, or `ProcessMain`.
- Handling `kServerCrashReportReceived` in `Client::Receive`; it is consumed only inside `UploadCrashReport`'s loop.

## Risk triggers and invariants

Tier 3: wire protocol (new engine packet types, `kiProtocolVersion` bump) and trust boundary (new client-to-server packet).

- The crash-handler write stays allocation-free and runs before any upload attempt. The upload runs only on the main thread, only from the main-loop catch or the agent rig, and never from SIGABRT.
- Nothing in `TryUploadCrashReport` may propagate an exception or overwrite the written report.
- The server admits a report only through the contract row, bounds stored reports by the per-run cap, and never interprets the payload.
- Determinism and CRC are untouched.

## Acceptance criteria

1. Client and server build (`/compile`).
2. `/agent-harness`: launch a server and a client, connect, then run the client crash-report rig. The client's report file exists as today. A `CrashReport-*.txt` file appears in the server's app-data directory, and its bytes equal the first `min(file size, 1 MiB)` bytes of the client's report. The client log shows the acknowledged outcome. The server keeps running. The client exits with code 0.
3. `/agent-harness`: run the crash-report rig on a client with no server connection. The local report is written, no upload is attempted, and the client exits with code 0.
4. Checked by code reading (no existing harness trigger): the `MainThread` catch path rethrows and `ProcessMain`'s second `HandleException` call returns at the guard; SIGABRT never reaches `TryUploadCrashReport`; the server's per-run cap.
