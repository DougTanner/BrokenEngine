<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:59:45.925Z","dependsOn":[]} -->
# Keep tick publication views valid across Workbuffer allocations

## Context

Final survivor `S012-C011` is a retained HIGH server publication finding. `BuildTickPublication` forms `publicationCoords` from the main Workbuffer, then allocates grid updates, nested status changes, and optional full-frame rows from the same buffer before `PublishTick` consumes the earlier spans. `Workbuffer::Grow` resizes backing storage, invalidates every live view/pointer, and continues execution (`Engine/Source/Network/Server/ServerBroadcaster.cpp:133-221`; `Common/Workbuffer.cpp`; `ServerSessionRuntime.cpp:216-224`).

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-015.md` under `S012-C011 — FINAL: RETAIN_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-012.md:220` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:226`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

Determine the final publication-coordinate count and total status-row count first, counting every source row even when `BufferFrame` later drops an over-cap per-cell payload. Then make one combined Workbuffer pre-reservation before forming `publicationCoords`, carving the coordinate, grid-update, status-change, and optional full-frame storage in the existing allocation order. Keep the current `BuildTickPublication` and `PublishTick` APIs, publication ordering, per-cell limits, CRC behavior, and wire representation; do not replace the reservation with per-view refreshes or heap storage.

## Critical files

- `Engine/Source/Network/Server/ServerBroadcaster.cpp:133-221` — publication assembly.
- `Engine/Source/Network/Server/ServerBroadcaster.h` — publication storage contract.
- `Common/Workbuffer.cpp` and `Common/Workbuffer.h` — grow invalidation semantics.
- `Engine/Source/Network/Server/ServerSessionRuntime.cpp:216-224` — arena owner.
- `Engine/Source/Network/Server/AGENTS.md` — publication/CRC contract.

## In scope

- One combined Workbuffer pre-reservation for coordinate, grid-update, status-change, and optional full-frame publication data, made after the final coordinate and total status-row counts and before `publicationCoords` is formed.
- Counting every status row in that reservation, including rows that `BufferFrame` later drops when a cell exceeds the per-cell cap; retain the existing allocation order, publication counts, and `PublishTick` API.
- Existing publication ordering, per-cell validation, CRC, and valid small/large payload behavior.

## Out of scope

- Transfer-manager spans, Workbuffer implementation policy, wire layout, status-count protocol limits, or tick scheduling.
- Changing CRC algorithms, publication ordering, adding a heap container, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: Workbuffer growth can invalidate pointers in deterministic server tick publication and CRC/delta output across broadcaster and runtime owners; memory lifetime and network publication are invariant surfaces.

Preserve these invariants:

- Every publication view/pointer remains valid until its synchronous consumer finishes.
- Large valid active/status/full-frame sets cannot corrupt publication storage or CRC/delta data when Workbuffer grows.
- Existing ordering, limits, wire representation, and valid small publications remain unchanged.

## Acceptance criteria

- A large valid publication requiring Workbuffer growth is served by one combined pre-reservation made before `publicationCoords` is formed, with no publication view or pointer exposed to a later grow before `PublishTick` consumes it.
- The reservation includes every coordinate and status row before publication, including rows `BufferFrame` later drops for a per-cell cap; existing publication counts, row order, CRC/delta behavior, and optional full-frame rows remain correct.
- Ordinary small ticks retain current output and `PublishTick` receives the existing API shape.
- Server `Debug|x64` builds pass through `/compile`.

## Coordination

`Documents/Plans/Engine/TransferWorkbufferSpanLifetime.md` owns a separate transfer-manager span crossing a pre-CRC allocation. Keep tick-publication views and transfer ownership metadata independently valid while preserving the shared Workbuffer grow contract.

## Notes

Origin: `S012-C011`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-015.md` (`S012-C011 — FINAL: RETAIN_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-012.md:220`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:226`. No exact existing Plan was found. No source fix or build was performed during routing.
