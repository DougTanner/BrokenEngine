<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-13T12:18:50.496Z","dependsOn":[]} -->
# Tag client crash reports as sent and send an unsent one on the next confirmed connection

## Context

The existing crash-report upload sends a client crash report only from the crashing process. It does so from `MainThread`'s main-loop catch or the agent crash rig, and only when the client is connected. A report whose upload is not confirmed is never sent. That covers a timeout, a client that was not connected, a crash before acceptance, and every SIGABRT-written report. The user directed this follow-up: "we definitely want to be able to tag the crash report as sent or not, and send automatically on confirmed server connection (if not already tagged)." As recorded by the dispatching session, "confirmed server connection" means the client has `ClientStateFlags::kConnectionAccepted` (`Engine/Source/Network/Client/Client.h:90`), which the client clears wherever it clears `kConnected`. The in-crash upload uses the same gate.

This Plan builds on the existing crash-report upload, which follows the user's choice of Alternative A. It has one client-to-server engine packet, `ClientCrashReportMessage` (type, `int32_t iSize`, then the first 1 MiB of the report). The client sends it reliably, then calls `enet_peer_disconnect_later` and waits up to 5 s for `ENET_EVENT_TYPE_DISCONNECT`; there is no server acknowledgment packet. The server keeps at most one report per connection, where a repeat counts as a rate violation and is not stored (`ClientConnection::StateFlags::kCrashReportReceived`), and it stores at most 32 reports per run, written atomically to its app-data folder. Read the landed code for exact names before implementing.

Facts this design rests on:

- The report paths are fixed per machine. `ResolveCrashReportPaths` (`Engine/Source/CrashReport.cpp:65-114`) resolves `<Game>-Crash-Report.txt` on the Desktop and in the per-user app-data folder into two fixed buffers. Before the in-crash upload, `HandleException` (`CrashReport.cpp:116`) prompted first and wrote to whichever of them the answer picked. The crash path now runs in this order: `HandleException` writes the report to the per-user path, or to the working-directory-relative `kpcFallbackReportPath` (line 14) when that buffer is empty, with no prompt before the write; the in-crash upload runs; the game unwinds; and last, outside agent launches, the "Save crash report to desktop?" prompt runs, where Yes copies the already-written report to the Desktop path. The per-user (or fallback) file is therefore the report, and each crash overwrites it. The Desktop file is only an optional copy of it.
- `common::CrashFileWriter` opens the report with `CREATE_ALWAYS` (`Common/FileUtils.h:80`), which truncates an existing file. A freshly written report therefore never contains anything a previous session appended, so it is unsent by default without any work in `HandleException`.
- ENet marks a packet `ENET_PACKET_FLAG_SENT` only when the peer has acknowledged every one of its fragments. That happens in `enet_protocol_remove_sent_reliable_command` (`ThirdParty/enet/protocol.c:272-279`), immediately before `enet_packet_destroy`, which calls the packet's `freeCallback` (`ThirdParty/enet/packet.c:62-63`). A packet discarded by a peer reset is destroyed without that flag (`ThirdParty/enet/peer.c:267-273`). `NetworkManager::SendPacket` (`Engine/Source/Network/NetworkManager.h:45-56`) creates every engine packet and does not expose it.
- A `Client` is created per connection (`ClientSessionRuntime::Connect`, `Engine/Source/Network/Client/ClientSessionRuntime.cpp:133`) and destroyed on disconnect (line 152). `ClientSessionRuntime::PollAndDrain` (line 217) runs once per main-loop frame on the main thread.

## Design

### Sent tag: a fixed trailer appended to the report file

Recommended mechanism: a report is sent when its file ends with a fixed trailer line, for example `kpcCrashReportSentTrailer = "\nThis crash report was sent to the server.\n"`. Marking a report sent appends the trailer to the file. Checking it reads only the file's last `trailer length` bytes.

Rationale: the `CREATE_ALWAYS` truncation above makes every new report untagged, so `HandleException` stays unchanged and allocation-free. A separate app-data marker file would survive the next crash's overwrite, so it would need to record the report's identity to stay correct. Renaming the file would move the report away from the fixed path where the prompt's Desktop copy and the in-crash upload expect it. The trailer is appended after the upload, so it is never part of uploaded bytes. A partly written trailer reads as unsent, and the only cost is one duplicate report on the server.

### Shared send with a delivery callback

- `NetworkManager::SendPacket` gains a defaulted `ENetPacketFreeCallback pFreeCallback = nullptr` parameter. It assigns this to `pPacket->freeCallback` before `enet_peer_send`. Every existing caller is unchanged. Recommended over a second packet-creation path in `Client`, so that all engine sends keep one creation site.
- `Client::SendCrashReport(std::span<const uint8_t> report, ENetPacketFreeCallback pFreeCallback)` writes `ClientCrashReportMessage` and sends it on `kiChannelReliable` with `ENET_PACKET_FLAG_RELIABLE`, using the callback. It also sets the per-connection `Client::mbCrashReportSent`. The existing `Client::UploadCrashReport` replaces its own message write and send with `SendCrashReport(report, nullptr)` and keeps its disconnect-based wait unchanged.

### Delivery confirmation for the next-session send

Recommended: ENet transport delivery, observed through the packet's `freeCallback` and `ENET_PACKET_FLAG_SENT`. There is no new packet and no protocol change.

Rationale:

- The next-session send runs in a healthy session, so it cannot use the in-crash `enet_peer_disconnect_later` confirmation, which would end the live session.
- A server acknowledgment packet would add an engine packet type and a `kiProtocolVersion` bump, and it would reverse the user's Alternative A choice of no acknowledgment packet.
- The SENT flag confirms the same thing the user accepted for the in-crash path: the server's ENet received the whole report. Neither path confirms that the server stored it. The server can still drop a delivered report at its 32-per-run cap, or drop a delayed one under server network simulation when the client disconnects soon afterwards (`Server.cpp:195-201`), and the client still tags it sent. That limitation is the same one the in-crash upload accepts.

The callback is a client-only file-static function in `CrashReport.cpp`. It sets a file-static `bool sbPendingCrashReportDelivered` when `pPacket->flags & ENET_PACKET_FLAG_SENT` is set. It does no file I/O, because it runs inside `enet_host_service`. It can also run during `~Client`'s drain while a crash is unwinding, after `HandleException` has already rewritten the file.

### Next-session send

New client-only `void PollPendingCrashReport(Client* pClient)` in `CrashReport.h` / `CrashReport.cpp` (inside `#if defined(BT_CLIENT)`, where `TryUploadCrashReport` already sits). It is called once at the top of `ClientSessionRuntime::PollAndDrain`, with `mpClient.get()`, before the `mpClient == nullptr` return. It runs only from the main loop, which never runs again after a crash. Each call does the following:

1. If `sbPendingCrashReportDelivered` is set, clear it and append the trailer to the stored sent path (`spcPendingCrashReportPath`, a file-static `const wchar_t*` pointing at one of the fixed path buffers). Log one `kInfo` line.
2. Return unless `pClient` is non-null, has `kConnectionAccepted`, and has not yet run this check (`Client::mbCrashReportChecked`, per connection). Then set `mbCrashReportChecked`.
3. Check the candidates in this order: the per-user path, skipped when its buffer is empty, then `kpcFallbackReportPath`. Select the first file that exists, is non-empty, and does not end with the trailer. The Desktop path is never a candidate, because it holds only the prompt's copy of a report already on one of those two paths. Each connection carries at most one report, which matches the server's one-report-per-connection rule. Any other untagged report goes on a later connection.
4. Read at most `kiMaximumCrashReportSize` bytes from the start of the selected file. Reuse the read code in `TryUploadCrashReport`, extracted into one file-static helper that both callers use. Store the path, call `pClient->SendCrashReport(bytes, &OnCrashReportPacketFreed)`, and log one `kInfo` line.

The whole body runs under `ScopedSuppressAllocationTracking`. An I/O failure logs one `kWarning` and returns; nothing propagates.

### In-crash path changes

`TryUploadCrashReport` changes in two ways:

- It returns without action when `gpClient->mbCrashReportSent` is already set. The connection has already carried a report, so the server would count a repeat as a rate violation and drop it. The fresh report stays untagged and goes on the next session's connection.
- When `UploadCrashReport` returns `true`, it appends the trailer to the report path it just uploaded.

`HandleException` itself is unchanged.

## Critical files

- `Engine/Source/CrashReport.h`
- `Engine/Source/CrashReport.cpp`
- `Engine/Source/Network/NetworkManager.h`
- `Engine/Source/Network/Client/Client.h`
- `Engine/Source/Network/Client/Client.cpp`
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp`
- `Engine/Source/AGENTS.md`
- `Documents/Architecture/Network.md` (conditional; see `## In scope`)
- `.agents/skills/agent-harness/references/command-reference.md` (conditional; see `## In scope`)

## In scope

- `Engine/Source/CrashReport.h` / `CrashReport.cpp`:
  - `kpcCrashReportSentTrailer`
  - the file-static pending-path pointer, the delivered flag, and the free callback
  - `PollPendingCrashReport`
  - the shared prefix-read helper extracted from `TryUploadCrashReport`
  - in `TryUploadCrashReport`, the `mbCrashReportSent` early return and the trailer append on confirmed delivery
- `Engine/Source/Network/NetworkManager.h`: the defaulted free-callback parameter of `SendPacket` only.
- `Engine/Source/Network/Client/Client.h` / `Client.cpp`:
  - `Client::SendCrashReport`
  - the `mbCrashReportChecked` and `mbCrashReportSent` members
  - in `Client::UploadCrashReport`, only the replacement of its message write and send with `SendCrashReport(report, nullptr)`
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp`: the one `PollPendingCrashReport(mpClient.get())` call at the top of `PollAndDrain`.
- `Engine/Source/AGENTS.md` `## Crash Reporting`: the sent trailer, unsent by default through truncation, the next-session send, and the in-crash skip when the connection already carried a report.
- `Documents/Architecture/Network.md` `kClientCrashReport` contract row: only if it states when the client sends a report.
- `.agents/skills/agent-harness/references/command-reference.md` `crash_report_harness_rig` row: only if it describes the upload outcome.
- Mechanical necessities only: includes and declarations.

## Out of scope

- Any wire change: no new packet type, no server acknowledgment, no `kiProtocolVersion` bump, no change to `ClientCrashReportMessage` or `GetClientPacketContract`.
- Any server change (`Server.h`, `Server.cpp`, `Engine/Source/Network/Server/AGENTS.md`).
- `HandleException`, `ResolveCrashReportPaths`, the report's content, its file names, and the save-to-desktop prompt.
- Uploading from SIGABRT or any non-main thread.
- Keeping more than one report per path, an outbox or queue of reports, or sending more than one report per connection.
- Confirming that the server stored a report (as opposed to receiving it), and delivery under server-side network simulation.
- A separate ENet channel for the report.

## Risk triggers and invariants

Tier 3. Trigger: a change spanning independently owned subsystems. It touches crash reporting (`Engine/Source/AGENTS.md` `## Crash Reporting`) and the client network transport, including the engine-wide send helper `NetworkManager::SendPacket`. There is no wire or server change, so the server's trust boundary is unchanged.

- `HandleException` stays allocation-free and log-free, and a newly written report is untagged without any added work in it.
- A report is tagged only after confirmed delivery: `ENET_PACKET_FLAG_SENT` for the next-session send, or `UploadCrashReport` returning `true` for the in-crash send. File I/O never runs inside the ENet free callback.
- A connection carries at most one crash report. The in-crash path skips upload when `mbCrashReportSent` is set.
- Only the main thread uses ENet, and only through the existing host. Determinism, CRC, saves, and replays are untouched.
- Exposure: the 1 MiB report shares `kiChannelReliable` with subscription traffic. Later client reliable packets may wait for the transfer, which happens at most once per connection and only when an untagged report exists.

## Acceptance criteria

1. Client and server build (`/compile`).
2. `/agent-harness`: run the crash-report rig on a client with no server connection. The report is written and does not end with the trailer. Launch a new client with the same app-data directory, and connect it to a server. A `CrashReport-*.txt` file appears in the server's app-data directory, with bytes equal to the first `min(file size, 1 MiB)` bytes of the client report. The client log shows the send line and then the tagged line. The report file now ends with the trailer. The client stays connected, and its subscriptions proceed.
3. `/agent-harness`: disconnect and reconnect that client, or launch another client with the same app-data directory. No new file appears on the server, and the client log has no send line.
4. `/agent-harness`: run the crash-report rig on a connected client whose report is untagged. The in-crash upload is delivered, and the report ends with the trailer. A following connected launch with the same app-data directory sends nothing.
5. Checked by code reading: the free callback only sets the flag; the tag is written only from `PollPendingCrashReport` or after `UploadCrashReport` returns `true`; `TryUploadCrashReport` returns early when `mbCrashReportSent` is set; `HandleException` is unchanged.

## Notes

- A newer crash overwrites an older untagged report at the same path before it is sent. This is accepted, because the newest report is the one sent.
- This Plan records choices the authoring agent recommends: the trailer tag, transport-level delivery confirmation, the defaulted `SendPacket` parameter, and the candidate order. Each choice carries its rationale above. The only user decisions are the quoted directive and the connection gate.
