<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T23:16:58.578Z","dependsOn":[]} -->
# Remove over-engineered checks in Network and Server

## Context

A repo-wide over-engineering (YAGNI) sweep looked for useless hashing, excessively defensive checks, fallbacks for cases that cannot happen, and ultra-rare edge-case protection that should be a plain operation plus an `ASSERT` or a hard error. A low-effort finder model flagged candidates file by file, and one validation pass by a stronger model confirmed or rejected each against the code, citing callers and invariants. The main session spot-checked only two of the 27 validation groups. This Plan carries the 23 confirmed candidates under `Engine/Source/Network/**` and `Engine/Source/Server/**`, with line numbers as of commit `7514890c`. The sweep's working files were session-local and are gone, so every fact the executor needs is restated below.

`.agents/references/cpp-conventions.md` (first bullet) assumes parameters from within the codebase are valid and puts validation only at trust boundaries; each candidate below is a check, branch, or parameter inside our own code that the validator found guarded by an earlier check or by every caller.

**Every candidate is unverified input, not an approved change.** The validator confirmed about 95% of what it saw, and only two groups were spot-checked. When this Plan was recorded, the user asked that the executing agent not trust these recommendations blindly.

## Design

### Per-candidate verification

For each candidate, in order:

1. Re-locate the code by its function and quoted condition; line numbers will have drifted.
2. Independently prove the claim against current code: cite the invariant (its single writer, the asserting constructor, the gate that ran first) or enumerate every caller. Do not reuse the validator's evidence without re-reading it.
3. Apply the proposed simpler form only when step 2 proves it. Where the proof rests on an invariant held elsewhere rather than in the same few lines, the author recommends the `ASSERT` form over plain deletion. `ASSERT` (`Common/ErrorUtils.h:20`) is active in every build configuration, so it turns a wrong proof into an immediate hard failure, never silent undefined behavior.
4. When the proof fails, or the change would touch a boundary listed below, drop the candidate and report it with the reason in the completion summary. A dropped candidate is a normal outcome, not a failure of this Plan.

### Boundaries that must never be removed

- Server validation of every client-to-server record and every grid save, and the rejection of bad values from packets or files under the bad-value rule in `.agents/references/cpp-conventions.md`; owners: `Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy`, `Engine/Source/File/AGENTS.md` `## Grid Saves`. In particular, the gates in `Server::Receive` (`Server.cpp:225-289`: client lookup, contract-violation gates, and the handshake gate) stay exactly as they are; several candidates below rely on them.
- The per-tick determinism CRC and everything feeding it.
- `.pack`, manifest, save, and replay format and version checks.
- Win32, Vulkan, ENet, and other OS or third-party API result checks.

Converting a check into an `ASSERT` is an acceptable outcome for any candidate. Client-side checks on server-to-client data are valid candidates, because the client trusts server data (root `AGENTS.md` `## Key Patterns`).

### Candidates

**Client — `kConnected` implies a non-null `mpServerPeer`.** Validator evidence: `mpServerPeer` is written only at `Client.cpp:50` (`enet_host_connect`) and `:146` (nulled in the DISCONNECT event right after `kConnected` is cleared at `:144`); `kConnected` is set only in the CONNECT event at `:121`, which ENet raises only for the peer created at `:50`; `Disconnect()` (`:339`) clears the flag without nulling the peer.

1. `Engine/Source/Network/Client/Client.cpp:786, 840, 864, 916, 937` — `SendAcknowledgement`, `SendDesynchronizationReport`, `SendDebugFrameRequest`, `SendUnsubscribe`, `SendResynchronizationRequest`: `|| mpServerPeer == nullptr` re-checks what `kConnected` guarantees. Simpler form: `if (!(mStateFlags & ClientStateFlags::kConnected))`.
2. `Client.cpp:888-891` — `SendSubscribe`: `if (mpServerPeer == nullptr) { return false; }` after the `kConnected` early-out at `:880`. Simpler form: delete.
3. `Engine/Source/Network/Client/Client.h:67` — `SendSimplePacket`: same `|| mpServerPeer == nullptr`. Simpler form: as candidate 1.
4. `Client.cpp:55, 334` — `~Client` and `Disconnect`: `mpServerPeer != nullptr &&` before the `kConnected` test; a failed `enet_host_connect` leaves the flag unset. Simpler form: `if (mStateFlags & ClientStateFlags::kConnected)`.

**Client — other.**

5. `Engine/Source/Network/Client/ClientSessionRuntime.cpp:497-501` — `SetDesiredCoordinates`: `if (!desiredCoordinates.empty())` around `assign`, after `clear()`. `assign` of an empty range leaves the vector empty. Simpler form: `mDesiredCoordinates.assign(desiredCoordinates.begin(), desiredCoordinates.end());`, dropping the `clear()` too.
6. `Engine/Source/Network/Client/ReconcileReplayTick.cpp:379` — `ReconcileFastPathCatchUp`: `std::min(..., kiNetworkBufferSize)` on the new snapshot count. Evidence: the loop at `:359-371` runs at most `iBudget = kiNetworkBufferSize - iStartCount` steps, and `iReplayWriteCount` rises only in `ReconcileRunTickCoord` (`:177`), one per step, from 0 (`:356`). Client-only reconcile bookkeeping; the replayed state that feeds the CRC is not changed. Simpler form: `rCell.iSnapshotCount = iStartCount + rScratch.iReplayWriteCount;` with an optional `ASSERT(rCell.iSnapshotCount <= engine::kiNetworkBufferSize)`.
7. `Engine/Source/Network/NetworkSerialization.cpp:249-253` — `DeserializeStatusChangeBatch` (client side, server data): `if (iSourceSize == 0) { return 0; }` duplicates the loop exit, because `BoundedCursor::Remaining()` (`NetworkCursor.h:86`) is 0 for an empty span. Simpler form: delete.

**Server — status-change compression (candidates 8 and 9 go together).** Evidence: the only caller, `ServerBufferedFrames::BufferFrame` (`ServerBufferedFrames.cpp:62`, through the sandbox `NetworkSessionContract::CompressStatusChanges`), drops over-cap batches first (`:49-58`) and passes `mCompressionBuffer`, sized to at least `kiMaxCompressedStatusChangeBatchBytes` (`ServerBufferedFrames.cpp:27`; `NetworkSerialization.h:33` = prefix + `LZ4_COMPRESSBOUND` of the worst-case serialized batch) and only ever grown (`:161-163`). `SerializeGroup` skips empty groups (`NetworkSerialization.cpp:127`), so the input stays within that bound and LZ4 cannot fail. The input is the server's own simulation output. The compressed bytes on the wire do not change.

8. `NetworkSerialization.cpp:353-359` — `CompressStatusChangeBatch`: the LZ4-failure branch. Simpler form: `ASSERT(iCompressedSize > 0);`.
9. `Engine/Source/Network/Server/ServerBufferedFrames.cpp:63-73` — `BufferFrame`: the `else` that drops the payload when compression returned 0. Simpler form: `ASSERT(iCompressedSize > 0);` then an unconditional `buffered.compressedData.assign(...)`.

**Server — handler re-checks after the `Server::Receive` gates.** Evidence: each handler's only caller is the `Receive` switch, reached only after gate 1 found the client (`Server.cpp:225-229`), and, for rows whose contract leaves `bRequiresHandshake` at its default `true` (`NetworkProtocol.h:159`, rows `:170-176`), after gate 4 (`Server.cpp:285-289`) dropped non-handshaken clients in the same call. Gates 3-5 return right after any `RecordContractViolation`; `NetworkMessages::Read` only throws; nothing between the gates and the handler removes a client or clears its handshake. The gates themselves stay. The executor re-proves, for each handler, that no path inside it before the check can remove the client.

10. `Engine/Source/Network/Server/Server.cpp:632-637` — `ClientHello`: null check on `FindClient` after the Hello is parsed. Every path in `ClientHello` that removes the client (`RejectHello`, `:684-698`) returns first. Simpler form: `ASSERT(pClient != nullptr);`.
11. `Server.cpp:786-790` — `ClientUnsubscribe`: null check on `FindClient`. Simpler form: `ASSERT(pClient != nullptr);`.
12. `Server.cpp:497-501` — `ClientAcknowledgementStream`: `FindHandshakenClient` plus a null return.
13. `Server.cpp:525-529` — `ClientDesynchronizationReport`: same.
14. `Server.cpp:558-562` — `ClientDebugFrameRequest`: same.
15. `Server.cpp:708-712` — `ClientSubscribe`: same.
16. `Server.cpp:813-817` — `ClientResynchronizationRequest`: same.

   Simpler form for 12-16: `ClientConnection* pClient = FindClient(iClientId); ASSERT(pClient != nullptr && pClient->bHandshakeComplete);` and delete the null branch.

**Server — other.**

17. `Engine/Source/Network/Server/ServerBroadcaster.cpp:263-266` — `ProcessUpdatePlayerRequests`: `mCells.contains(updateCoord)` guard. Evidence: called only from `BuildFrameInputs` (`:52`), which fills `mFrameInputs` only from `mActiveCoordinates` (`:23-29`); `:257` already checked the key; `PrepareActiveSet` (`GameBase.cpp:961-965`) creates a cell for every active coordinate (`SyncActiveCells`, `ServerSessionRuntime.cpp:306-314`; `EnsureNextFrames`, `Game.cpp:285-291`); `:267` already calls `mCells.at` unguarded. Simpler form: delete.
18. `ServerBroadcaster.cpp:236-239` — same function: `if (ownedPlayers.empty()) continue;`. With an empty span the search loop (`:243-251`) does not run and `if (!bFound) continue;` (`:252-255`) gives the same result. Simpler form: delete.
19. `Engine/Source/Network/Server/ServerSessionRuntime.cpp:207` — `SendNewSubscriptionFullStates`: `rSubscription.iSlot < std::ssize(pClient->slots)`. Evidence: the only producer of `PendingNewSubscription` is `Server::ClientSubscribe` (`Server.cpp:774`), which pushes only after `AllocateSlot` returned `iSlot >= 0` bounded by `std::ssize(slots)` (`Server.cpp:744-750`, `Server.h:93`); `slots` is sized once at connect (`Server.cpp:179`) and never resized; `.at()` still bounds-checks. Simpler form: drop only that term.
20. `Engine/Source/Network/Server/ServerTransferManager.cpp:192-197` — `ApplyPreparedTransfers`, verbose log only: the `kiReserve` buffer-room `break`. The ID count is capped at 8 (`:188-191`) and a `%lld` is at most 20 characters, so `iPosition` stays at most 152 and `152 + 24 <= 192`. Simpler form: delete the constant and the `if`.
21. `ServerTransferManager.cpp:204-208` — same log: `if (iWritten <= 0) { break; }` after `snprintf("%lld", int64)` with at least 40 free bytes. Simpler form: delete.
22. `Engine/Source/Server/ServerDisplay.cpp:111-114` — `ServerUpdateDisplayStatistics`: inner `if constexpr (kbProfiling)` repeats the early return at `:90-93`; this is not a template, and `GetCpuCounter` and `kCpuCounterExplosions` are declared unconditionally (`ProfileManagerBase.h:389`, `:98`). Simpler form: the single statement `gpProfileManager->GetCpuCounter(kCpuCounterExplosions).iCount = iTotalExplosions;`.
23. `ServerDisplay.cpp:34-35, 190, 195-196` — `ServerDisplayContentChanged`: `sbHasLastContentHash` only guards a first content hash of exactly 0. `uiHash` starts at the FNV offset basis (`:139`) and is mixed at least once (`:146`); the only cost of that 2^-64 case is one skipped repaint of a developer window. Simpler form: delete the flag and its write, and test `if (uiHash == suiLastContentHash)`.

## Critical files

- `Engine/Source/Network/Client/Client.cpp`, `Client.h`
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp`
- `Engine/Source/Network/Client/ReconcileReplayTick.cpp`
- `Engine/Source/Network/NetworkSerialization.cpp`
- `Engine/Source/Network/Server/Server.cpp`
- `Engine/Source/Network/Server/ServerBroadcaster.cpp`
- `Engine/Source/Network/Server/ServerBufferedFrames.cpp`
- `Engine/Source/Network/Server/ServerSessionRuntime.cpp`
- `Engine/Source/Network/Server/ServerTransferManager.cpp`
- `Engine/Source/Server/ServerDisplay.cpp`

## In scope

Only the exact checks, branches, and statements named in candidates 1-23, in the functions named there, and only the candidates the executor proves. Comment edits that a removed check makes false are in scope.

## Out of scope

- Every boundary listed under `### Boundaries that must never be removed`, including the `Server::Receive` gates, `FindHandshakenClient` itself, and its other callers at `Server.cpp:331` and in `RejectHello` (`:691`).
- Any check, branch, or file not listed as a candidate, including similar-looking sites the executor notices; report those as residuals instead.
- `PaintGridMap` in `ServerDisplay.cpp`, which `Documents/Plans/Engine/ServerDisplayGridBoundsInt64.md` owns.
- The sandbox wrapper `NetworkSessionContract::CompressStatusChanges` under `Projects/`, beyond what candidates 8-9 make dead.
- Wire format, packet layout, and protocol contract rows (`NetworkProtocol.h`).

## Acceptance criteria

- The completion summary lists every candidate 1-23 as applied, with the executor's own proof (invariant or every caller cited against current code), or dropped, with the reason.
- No boundary from `### Boundaries that must never be removed` is weakened; every removed server-side check was dominated by a gate that still runs.
- Client and server build clean in Debug and Release through `/compile`.
- A live `/agent-harness` run connects a client, subscribes, runs ticks across a cell transfer, and disconnects with no `ASSERT` and no CRC mismatch.

## Notes

- Risk tier: Tier 3 (`.agents/references/risk-tiers.md`). Trigger: trust boundary — candidates 10-16 edit the server's client-record handlers, where a wrong invariant would let a client-sent record reach a server `ASSERT`; candidates 8-9 sit on the wire payload producer (bytes unchanged). No determinism/CRC, save, or replay format change is intended.
- `ServerDisplay.cpp` is server-only; `Client*.cpp` and `ReconcileReplayTick.cpp` are client-only; `NetworkSerialization.cpp` is shared. No project-membership change.
- `Documents/Plans/Game/FrameEditCellPins.md` cites `ServerSessionRuntime.cpp` and `ServerTransferManager.cpp`; it does not touch the candidate lines, so there is no ordering constraint.
