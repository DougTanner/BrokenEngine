<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:59:49.308Z","dependsOn":[]} -->
# Keep transfer ownership spans valid across pre-CRC allocation

## Context

Final survivor `S012-C012` is a promoted HIGH server transfer finding. `HarvestTransfers` and `ApplyReplayTransfers` form `clientTransfers` spans in a Workbuffer arena and pass them to `ApplyPreparedTransfers`; that function allocates `pPreCrcs` from the same Workbuffer before `TrackClientTransfers` consumes the spans. A grow invalidates the span while execution continues (`Engine/Source/Network/Server/ServerTransferManager.cpp:146-279,285-365`; `Common/Workbuffer.cpp`).

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-016.md` under `S012-C012 — FINAL: PROMOTE_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-012.md:234` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:227`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

Make `ApplyPreparedTransfers` receive the `common::ScopedWorkbufferArena&` that owns the transfer rows instead of a preformed `ClientTransferInfo` span. Keep pre-CRC storage in its own `ScopedWorkbufferArena`; before each destination's CRC, reacquire its `Span<common::crc_t>()` and copy the scalar to a local, then call `Frame::Crcs()` in the nested CRC scope. Re-form the pre-CRC span on every iteration because any nested grow invalidates the prior view, and let the nested CRC scope pop before forming the `ClientTransferInfo` span from the supplied arena and passing it to `TrackClientTransfers`. Apply this consume/re-form sequence to live and replay paths while preserving deterministic transfer ordering, ownership relink, destination CRC recomputation, and publication.

## Critical files

- `Engine/Source/Network/Server/ServerTransferManager.cpp:146-279,285-365` — prepared/live/replay transfer assembly.
- `Engine/Source/Network/Server/ServerTransferManager.h` — transfer arena/state contract.
- `Common/Workbuffer.cpp` and `Common/Workbuffer.h` — grow invalidation semantics.
- `Engine/Source/Network/Server/AGENTS.md` — transfer ownership/publication contract.

## In scope

- The `ApplyPreparedTransfers` arena-reference signature and consume/re-form lifetime: keep pre-CRC storage in its own `ScopedWorkbufferArena`, reacquire its `Span<common::crc_t>()` before each scalar copy, and form the `ClientTransferInfo` span only after the nested CRC scope pops.
- Copying each pre-CRC scalar to a local before its destination's nested `Frame::Crcs()` Workbuffer allocation, for both live and replay transfer paths, before `TrackClientTransfers` consumes the rows.
- Existing transfer ordering, destination materialization, ownership relink, CRC, and valid small-batch behavior.

## Out of scope

- Tick-publication Workbuffer views, transfer wire layout, destination policy, CRC algorithm, or Workbuffer global implementation changes.
- New transfer storage ownership, reordering, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: a grow-capable Workbuffer allocation can invalidate authoritative cross-cell transfer ownership metadata before CRC/publication consumption across server transfer phases.

Preserve these invariants:

- `TrackClientTransfers` consumes exactly the live/replay transfer rows produced for the current operation.
- Workbuffer growth cannot corrupt owner coordinates, client mappings, pending subscriptions, or fleet transfer state.
- Valid transfer order, destination CRC recomputation, wire layout, and replay behavior remain unchanged.

## Acceptance criteria

- A live and replay transfer batch large enough to grow the Workbuffer completes with correct owner/client/fleet state and destination CRC/publication using the consume/re-form sequence.
- `ApplyPreparedTransfers` reacquires the pre-CRC `Span<common::crc_t>()` before copying each scalar, calls each destination's `Frame::Crcs()` only after that copy, and forms `clientTransfers` only after the nested CRC scope pops.
- `TrackClientTransfers` consumes the same ordered live/replay rows after the nested allocation, even when the Workbuffer grows.
- Server `Debug|x64` builds pass through `/compile`.

## Coordination

`Documents/Plans/Engine/TickPublicationWorkbufferLifetime.md` owns publication assembly views and its later status/full-frame allocations. Keep transfer ownership spans separate from tick publication while preserving the common Workbuffer lifetime rule.

## Notes

Origin: `S012-C012`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-016.md` (`S012-C012 — FINAL: PROMOTE_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-012.md:234`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:227`. No exact existing Plan was found. No source fix or build was performed during routing.
