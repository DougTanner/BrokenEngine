<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T18:45:20.614Z","dependsOn":[]} -->
# Grid save framing and orchestration in Engine

## Context

This is the first independently landable slice of the D4 save/replay game contract. The current server-only implementation in `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:1157-1315` owns the grid save header, deterministic grid payload order, staged reads, adoption, and the save/load/reset/autoload/autosave callers. Engine code already drives the game-owned save/load object during server ticks and process shutdown (`Engine/Source/GameBase.cpp:143-220,282,339-345`; `Engine/Source/Main.cpp:457`). Moving only the grid framing seam reduces that engine-to-game inversion without deciding replay policy.

In plain language, a save writes one versioned snapshot of the world grid and game-owned state; a load must read the complete snapshot into isolated state, validate it, and only then replace the live state. The current implementation is the evidence baseline (session baseline `2f90423b5cf14ce4bfdc1687818126dc6da47d82`); the claiming session must re-check line locations and current callers before editing.

The user explicitly authorized this scoped option-bearing exception on 2026-08-18: this Plan preserves the bounded research choices around the exact direct-function signatures and extraction boundary. Implementation cannot begin until the claiming Tier-3 session resolves and explicitly approves those choices. The required dispatch shape is fixed: required domain-neutral `game::` free functions, with no virtual `GameBase` hooks, registered callbacks, startup registration phase, CRTP, type erasure, or pointer-indirection seam.

Each client/server target links one game-logic implementation, and the repository already uses this compile/link-time static contract: `RunFrameTick` is engine-owned while its phases call required game functions directly (`Engine/Source/Frame/FrameBase.cpp:285,338-359`), and process lifecycle calls required game functions directly (`Engine/Source/Main.cpp:212-276,462-465`). Direct functions therefore need no runtime owner or registration. Pure virtuals would add runtime dispatch on a live `GameBase` instance (the existing `GameBase` hooks at `Engine/Source/GameBase.h:150-161` are behavior precedent, not a persistence requirement); registered `std::function` callbacks would add indirect calls, storage, registration ordering, and a missing-registration state; CRTP would add template plumbing without a needed polymorphic owner.

The candidate is E3 / I4 / R3. It establishes the direct staged-state game-function contract that the replay slice consumes. It must preserve the established save format and trust-boundary behavior rather than invent persistence semantics.

## Design

### Direct game contract and bounded implementation choices

Retain the four proposed domain-neutral game functions as the bounded contract surface:

- `WriteSaveState` supplies game-owned state while the engine writes the versioned grid snapshot.
- `ReadSaveState` reads game-owned state into an opaque, required movable `game::SaveStagedState` value.
- `AdoptSaveState` consumes that value only after the complete grid has passed validation.
- `ResetSaveState` performs the game-owned fresh-state reset required by the reset/load orchestration.

The names are domain-neutral; an engine-side function must not say or require `Fleet`. The exact parameter and return signatures, the final engine file placement, and the precise extraction boundary remain the bounded choices for the claiming Tier-3 session. That session must record the resolution before implementation. `SaveStagedState` remains a value that engine code stores and passes back without inspecting its fields. Do not replace it with type-erased ownership, a virtual interface, a registered callback, CRTP plumbing, or a pointer indirection merely to hide game fields.

### Versioned, sorted grid framing

Preserve the current `WriteGrid` byte and ordering contract (`Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:1157-1192`): write the `game::Frame` version header, frame count, followed client grid coordinate, next global ID, game-owned save state, then each coordinate frame in ascending `GridCoord::ToKey()` order. For each key, reconstruct the coordinate, write it, write static data without NavData, and write the current frame. Keep the existing atomic file write and the current version identity. Sorting is part of deterministic save output, not a presentation detail.

### Staged read, validation, and adoption

Keep the two read stages separate (`:1195-1212` and `:1214-1303`). The public load path first creates `SaveStagedState`/grid staging, then adopts only after the whole stream succeeds. The staged reader must preserve all current trust-boundary checks:

- validate the `game::Frame` version header before setting the header-validated failure flag;
- bound the frame count against the stream before reserve/allocation;
- read and validate the client coordinate and positive next-global-ID value;
- let the direct `ReadSaveState` function fill game-owned staged state;
- read every coordinate's static data without NavData, current frame, and replacement next frame;
- reject invalid first-frame clocks (non-negative tick, accumulator headroom, finite time) and reject any later frame whose tick or float bit pattern differs from the first;
- require the file-derived client coordinate to name a frame that was actually staged;
- reject stream failure after the payload, and turn corrupt, truncated, or otherwise invalid file data into the existing `false` result.

`AdoptSaveState` and the grid adoption remain after all correlation checks. Preserve the current adoption order (`:1305-1315`): replace coordinate frames, move game-owned fleets/GUID mapping/random state through the opaque value, then apply next-global-ID, tick, and current-time values. Engine code owns sequencing and does not inspect game fields.

Preserve the established post-header failure contract (`:1195-1206`): if a valid header was followed by corrupt or truncated payload, clear live coordinate frames and reset live game save state so callers can rebuild a fresh game. A version mismatch or invalid header remains a distinct refusal and does not acquire that post-header reset state.

### Save/load/reset/autoload/autosave orchestration

Move or re-home the orchestration only as far as needed to make the grid framing seam engine-owned, keeping the current order and meanings:

- `ServerSave` writes the quicksave using the current client grid coordinate.
- `ServerLoad` stages and adopts the file, captures the loaded tick/time, resets game state, restores the saved clock, restores the followed client coordinate, resets connected clients, and recomputes the active set (`:431-457`; `Network/Server/ServerSession.cpp:411-496`).
- `ServerReset` creates a fresh game frame, resets the global-ID base and game-owned state, clears the game-owned manager state required for a fresh game, resets connected clients, and recomputes the active set (`:460-473`).
- `Autosave` writes the backup file atomically; `TickAutosave` keeps the one-hour timer and skips while recording, replaying, or loading a replay (`:475-494`).
- `Autoload` stages/adopts the autosave and restores the followed client coordinate (`:496-508`).

The exact call ownership is one of the bounded choices to resolve in the Tier-3 session. Whichever ownership is selected must retain these sequencing and failure contracts. Replay lifecycle and transfer policy are not moved by this Plan.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.h` — current server-only save/replay owner and the seam whose grid responsibilities are split.
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:1157-1315` — authoritative grid write, staged read, failure reset, and adoption implementation; `:420-508` — save/load/reset/autosave/autoload orchestration.
- `Projects/BrokenEngineSandbox/Source/Game.h` and `Projects/BrokenEngineSandbox/Source/Game.cpp` — current game state, reset, followed-coordinate, and game-owned save state behavior.
- `Engine/Source/GameBase.h` and `Engine/Source/GameBase.cpp` — engine lifecycle and save/load/replay call sites that must use the direct game contract.
- `Engine/Source/Main.cpp:457` — process-shutdown autosave caller.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:411-496` — connected-client reset/resynchronization after replacement.
- The eventual Engine save/framing source/header and the matching client/server project/filter entries selected by the claiming Tier-3 session.

## In scope

- Extract the versioned grid framing, sorted write, staged read, validation, adoption, and established post-header failure behavior from the current game save owner into the engine-facing seam.
- Implement the four required direct `game::` functions and the movable opaque `game::SaveStagedState` contract after the claiming Tier-3 session resolves their exact signatures and extraction boundary.
- Preserve the current save bytes, version identity, field ordering, coordinate ordering, clock/client-coordinate validation, atomic writes, and state replacement ordering.
- Preserve save, load, reset, autosave, and autoload orchestration and connected-client resynchronization ordering.
- Update every correctness-dependent caller, project membership, and owning documentation required by the chosen extraction, without changing unrelated game behavior.
- Run the applicable client and server builds and the existing save/load/reset/autoload/autosave and save/replay harness scenarios without adding test code.

## Out of scope

- Replay manifest, digest, inventory, writer/reader lifecycle, delayed activation, terminal-writer retention, replay metadata, per-tick checksum, or replay-only reset policy; those belong to `ReplayLifecycleToEngine`.
- Transfer classification, materialization, ordering, and replay transfer capture are owned solely by the live `Documents/Plans/Engine/ChangeListTransportContract.md`; `Documents/Plans/Engine/ReconcileReplayChainToEngine.md` is only a separate client rollback consumer/reference, not a joint owner or Grid prerequisite. Do not move `IsTransferType`, `ApplyTransferStatusChanges`, or `CaptureHarvestedTransfers` as part of this Plan.
- Any change-list transport or reconciliation redesign, network or wire protocol change, replay/save compatibility layer, format version change, or new persistence format.
- Virtual dispatch, callback registration, `std::function` storage, CRTP, type erasure, or a new abstraction not required by the resolved direct-function contract.
- Player/Fleet-named engine interfaces; game fields may remain inside the opaque value, but Engine must not inspect or require them.
- Unit tests, test framework work, generated data, or unrelated code/documentation cleanup.

## Risk tier and invariants

Future implementation is Tier 3 because it crosses the engine/game boundary and changes deterministic save serialization, staged state replacement, trust-boundary handling, and lifecycle coordination. The user-authorized bounded choices do not lower that tier.

Preserve these invariants:

1. The engine calls required domain-neutral `game::` free functions directly; no virtual, registered callback, registration phase, CRTP, type-erased, or pointer-indirection dispatch is introduced.
2. `game::SaveStagedState` is movable and opaque to Engine; read, validation, and adopt remain distinct operations.
3. The version header, frame count, client coordinate, next-global-ID, game state, ascending coordinate-key order, no-NavData static data, and current-frame order remain byte-identical.
4. Grid output remains atomically published and deterministic.
5. Deserialized counts are bounded before allocation; malformed, truncated, or invalid stream data fails gracefully at the trust boundary.
6. The positive next-global-ID rule, first-frame finite clock/range checks, exact later-clock comparison, and file-derived client-coordinate membership check remain enforced.
7. No live grid, game save state, clock, or connected-client state is replaced until the complete staged snapshot and game-state correlation succeeds.
8. After a valid header followed by a corrupt/truncated payload, the established reset clears live frames and game save state so callers can rebuild fresh; header/version refusal remains distinct.
9. Adoption moves all staged game/grid state and then applies next-global-ID, tick, and current-time values before client resynchronization and active-set recomputation.
10. Save/load/reset/autoload/autosave continue to skip or sequence around active replay/recording exactly as today; replay and transfer policy remain outside this slice.

## Coordination

This Plan has no directional dependency and is independently landable. It is the prerequisite for `Documents/Plans/Engine/ReplayLifecycleToEngine.md`; the replay Plan must consume the landed direct contract and staged grid behavior rather than duplicate it. Do not implement both slices concurrently when they would edit the same save owner or engine lifecycle call sites.

Change-list transport, including transfer classification, materialization, ordering, and capture, remains solely owned by the live `Documents/Plans/Engine/ChangeListTransportContract.md`; `Documents/Plans/Engine/ReconcileReplayChainToEngine.md` is only a separate client rollback consumer/reference, not a joint owner or Grid prerequisite. This Plan does not duplicate the transfer seam or reserve transfer symbols.

The exact signatures and extraction boundary are deliberately bounded options, explicitly authorized by the user for resolution by the claiming Tier-3 session. No implementation work starts until that session records the decision and approval in its execution card.

## Acceptance criteria

- Source and byte-order review shows one grid writer and one staged-reader/adopter path, with the version header, sorted coordinate keys, no-NavData static data, current frames, and atomic publication unchanged.
- A malformed or truncated file after a valid header triggers the established live-frame/game-state reset; an invalid header/version remains a distinct refusal. Count, next-global-ID, clock, and client-coordinate checks are independently visible in the new owner.
- A successful load stages all grid and game state before adoption, preserves the exact adoption order, restores clock/client coordinate, resets connected clients, and recomputes the active set.
- Save, quickload/load, reset, autosave, and autoload paths retain their existing filename/flag behavior, timer/replay guards, and caller sequencing.
- The direct `game::` free-function contract uses the resolved domain-neutral surface and movable opaque `SaveStagedState`; no virtuals, callback registration, CRTP, type erasure, or Fleet-named engine interface appears.
- Applicable standard client and server builds succeed, including any chosen Engine source/project membership; no unit tests are added.
- The existing save/load/reset/autoload/autosave harness scenarios pass, and the existing save/replay harness scenario remains free of new save, replay, CRC, checksum, or desync errors when run as the required no-regression signal.
- `plan validate` reports `status: valid`, `code: ok`, and no dependency errors for this Plan and the live scheduler tree.

## Execution card

### What does this Plan do?

It moves the deterministic grid save framing and its save/load/reset/autosave/autoload orchestration behind a required direct `game::` function contract in Engine, while preserving staged adoption and the established post-header failure reset. It does not move replay or transfer policy.

### Why is this good for the codebase?

The engine already owns the lifecycle that drives saves, while the game currently owns a large mixed persistence class. A narrow grid seam lets a second game provide its own state without copying engine framing and keeps the security-shaped validation and deterministic ordering in one implementation.

- Goal: centralize current grid framing/orchestration in Engine and leave game-specific state behind four direct functions.
- Boundary: only the grid save/read/adopt/reset and listed save lifecycle callers; replay and transfer remain separate.
- Tier trigger: Tier 3 for engine/game integration, deterministic serialization, trust-boundary validation, and live-state replacement.
- Bounded decision gate: the user explicitly authorized this option-bearing Plan; the claiming Tier-3 session must resolve/approve exact signatures, `SaveStagedState` placement, and extraction boundary before implementation.
- Interfaces and invariants: direct domain-neutral `game::` functions; movable opaque staged state; exact current save bytes/order; staged-before-adopt; post-header reset; clock, ID, coordinate, count, and stream validation.
- Acceptance checks:
  - source/format identity and trust-boundary review;
  - save/load/reset/autosave/autoload harness scenarios;
  - existing save/replay harness no-regression scenario;
  - applicable client/server builds;
  - plan validation with no scheduler errors.
- Roles:
  - preparation/implementation/propagation/docs: implementer;
  - Tier-3 plan audit and simplicity review: fresh reviewers;
  - external decision grill: Tier-3 reviewer before implementation;
  - builds: builder through `/compile`;
  - save/replay scenarios: `/agent-harness` operator;
  - C++ correctness, scope, and adversarial review: fresh reviewers;
  - project/style checks: mechanic through `/update-vcxproj` and `/code-style-review`;
  - final acceptance and landing gate: fresh `/verify-changes` reviewer followed by `/finalize-changes` only after user confirmation.
- No unit-test role or unit tests.

## Notes

This document converts the researched grid evidence into a scheduler-tracked future Plan; it does not itself authorize implementation before the bounded Tier-3 decisions are resolved.
