<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T18:45:27.173Z","dependsOn":["Documents/Plans/Engine/ChangeListTransportContract.md","Documents/Plans/Engine/GridSaveFramingToEngine.md","Documents/Plans/Frame/FrameInputSourceSplit.md"]} -->
# Replay lifecycle and metadata in Engine

## Context

This is the second independently landable slice of the D4 save/replay game contract. The current server-only implementation in `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:19-1155` owns the replay manifest, digest and inventory protocol, metadata, writer/reader lifecycle, transfer capture, and per-tick checksum path. Engine code already drives that game-owned object during the server tick and shutdown paths (`Engine/Source/GameBase.cpp:143-220,282,339-345,729-769`; `Engine/Source/Main.cpp:457`).

The goal is to move the replay mechanism while keeping game-specific state behind required direct `game::` functions. The grid framing and staged-adoption seam is a separate prerequisite. This Plan also hoists `mFrameInputs` to `engine::GameBase`: engine dispatch already reads it (`Engine/Source/GameBase.cpp:306,352`), and replay uses it for writer updates, delayed reader activation, terminal playback, and normal playback (`Save/GameSaveLoad.cpp:297-302,350-361,884-888,1011-1012,1079-1103`). An accessor would preserve the current inversion on a hot shared-state path.

The candidate is E5 / I5 / R3. It is the largest D4 candidate (roughly 1,100 lines) and includes security-shaped digest code that is easy to rewrite almost correctly. The claiming session must re-check all cited callers and current versions before editing.

The user explicitly authorized this scoped option-bearing exception on 2026-08-18: this Plan preserves only bounded research choices around exact direct-function signatures and extraction boundary. Implementation cannot begin until the claiming Tier-3 session resolves and explicitly approves those choices. The required dispatch shape is fixed: required domain-neutral `game::` free functions, with no virtual `GameBase` hooks, registered callbacks, startup registration phase, CRTP, type erasure, or pointer-indirection seam.

Each client/server target links one game-logic implementation, and the repository already uses this compile/link-time static contract: `RunFrameTick` is engine-owned while its phases call required game functions directly (`Engine/Source/Frame/FrameBase.cpp:285,338-359`), and process lifecycle calls required game functions directly (`Engine/Source/Main.cpp:212-276,462-465`). Direct functions therefore need no runtime owner or registration. Pure virtuals would add runtime dispatch on a live `GameBase` instance (the existing `GameBase` hooks at `Engine/Source/GameBase.h:150-161` are behavior precedent, not a persistence requirement); registered `std::function` callbacks would add indirect calls, storage, registration ordering, and a missing-registration state; CRTP would add template plumbing without a needed polymorphic owner.

This Plan depends on the landed `Documents/Plans/Engine/ChangeListTransportContract.md`, the grid framing Plan, and the currently live `Documents/Plans/Frame/FrameInputSourceSplit.md`, which owns the deterministic `FrameInput` split and preserves its replay serialization. `ActiveSetSkeletonToEngine` is completed landing history, not a live scheduler dependency. ChangeListTransportContract owns and settles the transfer classification, materialization, ordering, and capture contract consumed by Replay. `ReconcileReplayChainToEngine.md` is a separate client rollback consumer/reference, not a Replay prerequisite or joint transfer owner.

## Design

### Direct game contract and bounded implementation choices

Retain the six proposed domain-neutral game functions as the bounded contract surface:

- `WriteReplayMeta` writes the game-owned replay metadata after the writer files are complete.
- `ReadReplayMeta` reads metadata into staged replay state before any live replacement.
- `AdoptReplayMeta` applies staged metadata only after manifest, reader, and grid correlation succeeds.
- `OnReplayStreamsInvalidated` clears replay-only fixtures while leaving the live game intact.
- `OnReplayAborted` additionally clears staged transfers and unpublished status changes while preserving the current frames/inputs/active set for live simulation.
- `OnStateReplaced` relinks and resynchronizes connected clients after a successful save load, fresh reset, or replay adoption.

The names are domain-neutral; engine-side names must not say or require `Fleet`. The exact signatures, final engine placement, and extraction boundary remain bounded choices for the claiming Tier-3 session. That session must resolve and approve them before implementation. `ReplayMeta` and any staged replay values remain opaque to Engine except for the sequencing required by the current format. Do not replace the required direct contract with virtuals, registered callbacks, `std::function` storage, CRTP, type erasure, or pointer indirection.

### Manifest, digest, inventory, and exact format identity

Preserve the current replay format verbatim:

- manifest version `3` and invalidation version `0` (`Save/GameSaveLoad.cpp:19-21`);
- the exact domain-separation string `broken-engine/replay-manifest-generation/v3` (length 43) and artifact names at `:104-116`: `F7.replay.manifest`, `F7.replay.grid`, `F7.replay.meta`, `F7.replay.<coordKey>`, `F7.replay.<coordKey>.frames`, `F7.replay.<coordKey>.checksums`, and optional `F7.replay.<coordKey>.fullframes`;
- `ReplayManifestRecord` ordering and the payload field order at `:68-77,122-194`: manifest version, initial tick, sorted activation records (tick/x/y), full-frames flag, sorted inventory entries (kind/coordinate key/byte count/SHA-256), followed by the generation digest;
- expected inventory construction (the two global artifacts plus three or four per-coordinate artifacts), sorted unique record identity, bounded canonical records/inventory, and exact per-artifact SHA-256 checks at `:196-233,532-666`;
- generation digest preimage: the exact versioned payload plus the exact domain-separation prefix and length encoding, then the current SHA-256 result (`:173-194`);
- atomic invalidation and final publication of manifest payload followed by its 32-byte generation digest (`:243-262`);
- publication order: coordinate writers, metadata, expected inventory/digests, and final manifest (`:837-864,898-980`).

Manifest readers must retain the current trust-boundary rules: bound counts before reserve, reject empty or non-canonical records and inventory, validate tags/widths and trailing data, rebuild expected inventory and compare kind/coordinate identity, compare the generation digest, and hash every listed artifact before reading a component. Version mismatch remains an expected refusal distinct from corrupt data (`:691-705`); do not turn it into a generic corruption path.

### Writer/reader lifecycle

Preserve the current lifecycle and failure cleanup:

- Invalidation writes the invalid manifest marker and `OnReplayStreamsInvalidated` clears readers, pending readers, and replay-only transfer fixtures while leaving the live game intact (`:272-278,833-895`).
- Recording creates writers for active coordinates, tracks activation ticks, updates each non-terminal writer with the current frame input, and retains terminal writers/end frames so a coordinate that retires still writes a complete terminal stream (`:297-303,364-417,868-895,996-1028`).
- Stopping recording sorts manifest records, saves every coordinate writer using a retained terminal frame or current frame, cleans partial siblings on a writer failure, writes `ReplayMeta`, builds the expected inventory and digest, and publishes the final manifest in the existing order (`:898-993`).
- Reader construction stages saved start frames, initial inputs, and readers before activation. Activation is delayed until the recorded tick; then it creates/replaces the coordinate frame, transfers the saved start to the next frame, installs the staged input, and only then publishes the reader (`:350-362,1045-1062`).
- Terminal readers load and validate their terminal input/checksum, require terminal data to be consumed, remove the coordinate and its input, and retire before normal readers are advanced (`:1064-1096`).
- Each normal tick loads the difference, rejects transfer status changes in the ordinary input channel, loads an optional post-dispatch transfer batch, validates its transfer-only shape, prepares it for publication, and validates the per-tick checksum (`:1098-1142`).
- When all readers retire, emit the existing end marker, clear replay-only state, arm the replay reload flag, and stop that fixed-tick dispatch (`:1144-1150`).
- `OnReplayAborted` performs the additional staged-transfer and unpublished-status-change cleanup while retaining current frames, inputs, and active set (`:280-286,1038-1043`).

### Staged replay adoption and metadata ordering

Keep metadata read before validating each reader and the grid, but keep its adoption separate. The current load path reads metadata into a staged `ReplayMeta`, constructs and validates all staged readers, reads the grid into isolated state, correlates initial tick/coordinate/frame/time/CRC membership, and only then adopts the grid, sets the replay clock, applies metadata, resets clients, and recomputes active state (`:669-779`). A combined read-and-restore hook would break this atomicity. `ReadReplayMeta` and `AdoptReplayMeta` must therefore remain distinct, just as grid read/adopt remains distinct in the prerequisite Plan.

### `mFrameInputs` ownership

Hoist `mFrameInputs` from `game::Game` to `engine::GameBase` before or as part of the replay extraction, preserving its map type, coordinate identity, initialization/clear/erase behavior, and all writer, reader, server-broadcast, and frame-dispatch uses. Replay must not add an accessor for this hot shared state. The game direct functions may still own game-specific values, but Engine owns this generic deterministic input map after the hoist.

### Transfer and change-list boundary

Replay consumes the landed `Documents/Plans/Engine/ChangeListTransportContract.md` and does not create another transfer seam. The current implementation's `IsTransferType` participates in server transfer validation and client rollback (`Frame/StatusChange.h:29-34`; `Network/Server/ServerTransferManager.cpp:385`; `Network/Client/ReconcileReplayTick.cpp:139,175`), while the client rollback path consumes the settled change-list transfer rules. Transfer capture is part of that same contract. The current recording path reaches `CaptureHarvestedTransfers` (`Network/Server/ServerTransferManager.cpp:320-325`; `Save/GameSaveLoad.cpp:305-348`) as historical context; this Plan does not add or relocate a replay-recording hook.

Transfer classification, materialization, ordering, and capture remain outside this Plan and are owned and settled by `Documents/Plans/Engine/ChangeListTransportContract.md`.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.h` — current replay manager, writer/reader state, failure points, and public transfer-capture entry.
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:19-1155` — authoritative manifest/digest/inventory, metadata, writer/reader, terminal, checksum, invalidation/abort, and replay tick implementation.
- `Projects/BrokenEngineSandbox/Source/Game.h:27-33,86-96,155` and `Projects/BrokenEngineSandbox/Source/Game.cpp:864-873` — `ReplayMeta`, its current restore behavior, and current `mFrameInputs` owner.
- `Engine/Source/GameBase.h` and `Engine/Source/GameBase.cpp:143-220,282,306,339-360,723-769` — lifecycle, dispatch, and replay active-set call sites; target owner for `mFrameInputs`.
- `Engine/Source/Main.cpp:457` — shutdown autosave/lifecycle caller.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerTransferManager.cpp:320-325,385` and `Projects/BrokenEngineSandbox/Source/Network/Client/ReconcileReplayTick.cpp:139,175` — transfer capture/classification/reconciliation boundary.
- `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:29-34` — transfer status classification used at the replay trust boundary.
- `Documents/Plans/Frame/FrameInputSourceSplit.md` — live prerequisite for shared deterministic `FrameInput` ownership and stream serialization.
- `Documents/Plans/Engine/ChangeListTransportContract.md` — live change-list and transfer owner; `Documents/Plans/Engine/ReconcileReplayChainToEngine.md` — separate client rollback consumer/reference; no redesign is authorized here.
- The eventual Engine replay source/header and matching client/server project/filter entries selected by the claiming Tier-3 session.

## In scope

- Extract the replay manifest/digest/inventory, writer/reader lifecycle, delayed activation, terminal writer/end-frame handling, metadata staging/adoption, invalidation/abort, and per-tick checksum path behind the six required direct `game::` functions.
- Hoist the generic deterministic `mFrameInputs` map to `engine::GameBase` and update all correctness-dependent producers/consumers without adding an accessor seam.
- Preserve the current format bytes, artifact names, domain-separated digest preimage, publication order, reader/grid correlation, version-mismatch distinction, trust-boundary checks, transfer-channel rules, terminal behavior, and end marker.
- Consume the landed ChangeListTransportContract classification, materialization, ordering, and capture contract; ReconcileReplayChain remains a separate client rollback reference, and this Plan does not create another transfer seam.
- Update all affected callers, project membership, and owning documentation required by the chosen extraction.
- Run applicable client and server builds and the existing save/replay harness scenarios, including record/stop/playback, delayed activation, terminal retirement, matching per-tick checksums, end marker, and clean abort, without adding test code.

## Out of scope

- Grid save framing, sorted grid serialization, staged grid adoption, save/reset/autosave/autoload orchestration, and the four grid hooks; those belong to `GridSaveFramingToEngine`.
- Change-list transport or reconciliation redesign, transfer classification/materialization/capture, or protocol/wire changes. Replay consumes the landed ChangeListTransportContract; ReconcileReplayChain is only a separate client rollback reference, and this Plan does not create another transfer seam.
- Any replay or save format change, compatibility layer, version bump, artifact rename, changed digest algorithm, changed publication order, or changed `FrameInput` stream layout.
- Virtual dispatch, callback registration, `std::function` storage, CRTP, type erasure, or speculative abstractions.
- Player/Fleet-named engine interfaces; game-specific metadata/state remains behind domain-neutral direct functions.
- Unit tests, test framework work, generated data, or unrelated cleanup.

## Risk tier and invariants

Future implementation is Tier 3 because it crosses the engine/game boundary and changes deterministic replay serialization, digest/trust-boundary code, staged state replacement, frame-input ownership, and transfer/reconciliation coordination. The user-authorized bounded choices do not lower that tier.

Preserve these invariants:

1. The engine calls required domain-neutral `game::` free functions directly; no virtual, registered callback, registration phase, CRTP, type-erased, or pointer-indirection dispatch is introduced.
2. The six replay/state hooks remain separate where atomicity requires it: metadata read versus adopt, stream invalidation versus abort, and state replacement after successful adoption.
3. Manifest version `3`, invalidation version `0`, exact artifact filenames, exact domain-separation string, record/inventory ordering, digest preimage, per-artifact SHA-256 inventory, and final 32-byte generation digest remain byte-identical.
4. Counts and records are bounded and canonical before allocation/use; expected inventory shape and identity, digest, artifact byte counts, and trailing data are checked at the file trust boundary.
5. Version mismatch is refused and reported distinctly from corrupt/damaged data.
6. Metadata, all readers, saved starts, and the grid are staged and correlated before live reset/adoption; initial tick, coordinate membership, frame tick/time bit pattern, and frame CRC correlation remain enforced.
7. Publication order remains invalidation → coordinate writers → metadata → inventory/digest → final manifest, with atomic file writes and partial-sibling cleanup on failure.
8. Writer updates use the current coordinate `FrameInput`; terminal writers retain their end frame and terminal readers consume/validate terminal data before removal.
9. Delayed readers activate exactly at their recorded tick; ordinary input cannot carry transfers, and post-dispatch records contain only valid transfer batches.
10. Per-tick difference checksums are validated before publication/retirement; the end marker and fixed-tick stop/reload behavior remain unchanged.
11. Ordinary invalidation preserves the live game; abort additionally clears staged transfers and unpublished status changes while leaving current frames, inputs, and active set suitable for live continuation.
12. `mFrameInputs` becomes an engine-owned generic map with unchanged coordinate, clear, insert, erase, and serialization-use behavior; no hot-path accessor is added.
13. Transfer classification, materialization, ordering, and capture remain owned by `Documents/Plans/Engine/ChangeListTransportContract.md`; ReconcileReplayChain is a separate client rollback consumer/reference, and Replay creates no transfer seam.

## Coordination

This Plan depends directionally on `Documents/Plans/Engine/ChangeListTransportContract.md`, `Documents/Plans/Engine/GridSaveFramingToEngine.md`, and the currently live `Documents/Plans/Frame/FrameInputSourceSplit.md`. The first owns change-list transport, the second supplies the staged grid/adoption contract, and the third supplies the shared deterministic `FrameInput` ownership and stream implementation. If `FrameInputSourceSplit.md` is no longer live when this Plan is validated, omit that dependency only after confirming its landed completion and record that completion in the Plan's coordination evidence.

`ActiveSetSkeletonToEngine` is completed landing history, not a dependency to recreate. `ReconcileReplayChainToEngine.md` is a separate client rollback consumer/reference, not a Replay prerequisite or joint transfer owner. Replay consumes only the landed ChangeListTransportContract classification, materialization, ordering, and capture contract without creating another transfer seam. Do not run this Plan concurrently with any directional prerequisite when they touch the same callers.

The exact signatures and placement are deliberately bounded options, explicitly authorized by the user for resolution by the claiming Tier-3 session. No implementation work starts until that session records the decision and approval in its execution card.

## Acceptance criteria

- Source/format review shows manifest version `3`, invalidation version `0`, exact domain string and artifact names, canonical record/inventory ordering, exact digest preimage, per-artifact SHA-256 inventory, atomic publication, and the current publication order unchanged.
- Writer/reader review and harness evidence show terminal writers/end frames are retained, delayed readers activate on the recorded tick, terminal readers validate and retire correctly, partial failures clean up, and replay-only invalidation/abort cleanup remains distinct.
- Load review shows metadata is read into staged state, all readers and the grid are validated/correlated before adoption, `AdoptReplayMeta` remains separate, version mismatch is distinct from corrupt data, and connected-client/state replacement ordering is preserved.
- Per-tick replay review shows ordinary inputs reject transfers, post-dispatch records accept only transfers, every difference checksum is validated, and the existing end marker/fixed-tick reload behavior remains.
- `mFrameInputs` is owned by `engine::GameBase` with unchanged map semantics and all engine/game producers and consumers updated; no accessor or duplicate map remains.
- The six direct domain-neutral hooks use the resolved signatures; no additional transfer seam is introduced, ChangeListTransportContract remains the sole transfer owner, and ReconcileReplayChain is only a separate client rollback reference.
- Applicable standard client and server builds succeed, including chosen Engine source/project membership; no unit tests are added.
- The existing save/replay harness scenario passes: record multiple ticks, stop recording, verify manifest/inventory publication, start playback, observe delayed activation and terminal retirement, require a newly appended `End replay <tick>, looping` marker, compare ordered writer/reader checksum sequences, require no new replay/CRC/checksum/desync errors, and cancel/abort playback cleanly. Existing save-load/reset coverage remains free of new errors as the grid prerequisite signal.
- `plan validate` reports `status: valid`, `code: ok`, and exactly the `ChangeListTransportContract`, `GridSaveFramingToEngine`, and `FrameInputSourceSplit` dependencies with no dependency errors.

## Execution card

### What does this Plan do?

It moves the replay manifest and lifecycle mechanism into Engine behind six required direct game functions, hoists deterministic `mFrameInputs` to `GameBase`, and preserves the exact format, trust-boundary, checksum, terminal, and transfer-channel behavior. It consumes grid framing, change-list transport, and FrameInput prerequisites without creating another transfer seam; ReconcileReplayChain is a separate client rollback reference, not a Replay prerequisite or transfer owner.

### Why is this good for the codebase?

The replay path is the largest remaining game-owned persistence mechanism and contains a security-shaped digest protocol. One engine implementation avoids a second game rewriting it while direct game hooks keep game metadata and state ownership explicit. Hoisting `mFrameInputs` removes a hot shared-state inversion already traversed by Engine.

- Goal: centralize replay lifecycle/format validation in Engine and leave game-specific metadata/state behind six direct functions.
- Boundary: manifest/digest/inventory, writer/reader, metadata sequencing, terminal/checksum behavior, and input-map ownership; change-list transport, including transfer classification/materialization/ordering/capture, remains owned by its landed Plan, while ReconcileReplayChain remains a separate client rollback reference.
- Tier trigger: Tier 3 for engine/game integration, deterministic replay/CRC behavior, trust-boundary and digest code, frame-state ownership, and change-list/reconciliation coordination.
- Bounded decision gate: the user explicitly authorized this option-bearing Plan; the claiming Tier-3 session must resolve/approve exact signatures and placement before implementation.
- Interfaces and invariants: direct domain-neutral game hooks; opaque staged metadata; exact format/digest/publication order; staged correlation; terminal and delayed activation; checksum and transfer-channel validation; `GameBase::mFrameInputs` ownership.
- Acceptance checks:
  - source/format/digest and trust-boundary review;
  - existing record/stop/playback/abort harness scenario with checksum and end-marker evidence;
  - save-load/reset no-regression signal;
  - applicable client/server builds;
  - plan validation with exact dependencies.
- Roles:
  - preparation/implementation/propagation/docs: implementer;
  - Tier-3 plan audit and simplicity review: fresh reviewers;
  - external decision grill and external-claim checks if needed: Tier-3 reviewer/locator;
  - builds: builder through `/compile`;
  - save/replay scenarios: `/agent-harness` operator;
  - C++ correctness, scope, and adversarial review: fresh reviewers;
  - project/style checks: mechanic through `/update-vcxproj` and `/code-style-review`;
  - final acceptance and landing gate: fresh `/verify-changes` reviewer followed by `/finalize-changes` only after user confirmation.
- No unit-test role or unit tests.

## Notes

This document converts the researched replay evidence into a scheduler-tracked future Plan; it does not itself authorize implementation before the bounded Tier-3 decisions are resolved.
