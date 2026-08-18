<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T17:51:50.693Z","dependsOn":[]} -->
# Split `game::FrameInput` from display input

## Context

Baseline: `12a682dad9ad483fe9b6098a0dd6576d62ba0af2`.

The game input files currently combine two unrelated responsibilities:

- `Projects/BrokenEngineSandbox/Source/Input/Input.h:9-52` declares display-rate menu and camera input.
- `Projects/BrokenEngineSandbox/Source/Input/Input.h:54-65` declares the shared deterministic `game::FrameInput`.
- `Projects/BrokenEngineSandbox/Source/Input/Input.cpp:12-153` polls client hardware.
- `Projects/BrokenEngineSandbox/Source/Input/Input.cpp:155-204` implements `FrameInput` CRC and stream operators.

`FrameInput` is consumed by shared frame dispatch, reconciliation, server broadcasting, save/replay, and the replay difference stream. The current source is authoritative: `FrameInput::kiVersion` is exactly `15`. Earlier investigation text mentioning version `14` is stale and is not followed.

The mixed file currently appears in both client and server projects because its polling method bodies are client-gated while its `FrameInput` declarations and serialization are shared. This Plan separates the deterministic type without changing display-input behavior. The later display-input ownership Plan depends on this split.

## Design

Create `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h` and `FrameInput.cpp`.

`FrameInput.h` contains the existing `game::FrameInput` declaration unchanged:

- `static constexpr int64_t kiVersion = 15`;
- `std::vector<StatusChange> statusChanges`;
- `Crc()`;
- the existing stream operator friends.

It includes `Frame/StatusChange.h`. It does not include `Input/RawInputManager.h`, menu input declarations, camera input declarations, or the display `Input` class.

`FrameInput.cpp` contains the existing `FrameInput::Crc()`, `operator<<`, and `operator>>` bodies moved without semantic changes. Preserve:

- status-change iteration order;
- CRC initialization and multiplication order;
- type-tag CRC before payload CRC;
- stream count as `int64_t`;
- count validation through `common::ValidateDeserializedCount`;
- type-tag validation through `IsKnownStatusChangeType`;
- variant seating through `DefaultDataForType` before `std::visit` and `common::Read`;
- status type followed by payload serialization order.

The new files remain shared game files with no whole-file `BT_CLIENT` or `BT_SERVER` guard. Add both files to both Visual Studio projects and their `Game\\Frame` filters.

Replace `Frame/Frame.h`'s `Input/Input.h` include with `Frame/FrameInput.h`. Because that include was also the transitive source of display-input declarations, add narrow direct `#include "Input/Input.h"` directives to:

- `Engine/Source/Main.cpp`, which constructs `game::Input` and assigns `game::gpInput`;
- `Projects/BrokenEngineSandbox/Source/Game.cpp`, which consumes complete `MenuInput` values;
- `Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp` only when it still dereferences `gpInput->mCameraInput`. If `CameraOwnershipToEngine` has already landed, its reduced game camera no longer consumes display input and this include must remain absent.

Retain the existing direct include at `Engine/Source/GameBase.cpp:12`. Inventory every `MenuInput`, `CameraInput`, `game::Input`, and `gpInput` consumer; no other complete display-input consumer currently requires a direct include. `Game.h` uses the existing forward declaration from `GameBase.h`, and `Input.cpp` includes its local header.

After moving `FrameInput`, the remaining game `Input.h/.cpp` contain display input only and become entirely client-only. Keep `#pragma once` outside the header's whole-file `BT_CLIENT` guard. Wrap the implementation as a whole-file `BT_CLIENT` translation unit. Remove the old game `Input.h/.cpp` from the server project and server filters immediately; retain them in the client project and client filters until `Documents/Plans/Input/DisplayInputToEngine.md` moves display input to Engine.

Update the input and frame ownership documentation so it says deterministic `FrameInput` lives under `Source/Frame`, display input remains under `Source/Input`, and game display input is client-only. No frame phase, replay algorithm, CRC algorithm, or display-input behavior changes.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Input/Input.h:3-5,54-65` — remove the `FrameInput` declaration and its now-unused status-change include; add the whole-file client guard while retaining display declarations for the client.
- `Projects/BrokenEngineSandbox/Source/Input/Input.cpp:155-204` — move the CRC and stream bodies; retain the client-only display implementation in a whole-file client translation unit.
- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h` — new shared declaration file.
- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.cpp` — new shared implementation file.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.h:3-8` — include `Frame/FrameInput.h` and no longer provide display-input declarations transitively.
- `Engine/Source/Main.cpp:1-10,265-270` — add the direct display-input include; behavior remains unchanged.
- `Engine/Source/GameBase.cpp:1-13,34-39` — retain its direct display-input include.
- `Projects/BrokenEngineSandbox/Source/Game.cpp:1-8,570-852` — add the direct display-input include; menu behavior remains unchanged.
- `Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp:1-8,111,280` — add the direct display-input include only in the pre-D8 tree; in a post-D8 tree verify the obsolete reader and include are absent.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj` — add both new shared Frame files; retain game `Input.h/.cpp`.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj` — add both new shared Frame files; remove game `Input.h/.cpp`.
- The matching `.vcxproj.filters` files — place new files under `Game\\Frame`, retain game Input only in the client filter, and remove it from the server filter.
- `Projects/BrokenEngineSandbox/Source/Input/AGENTS.md` — document the split, client-only display input, and shared FrameInput location.
- `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` — document `FrameInput` serialization ownership.
- `Engine/Source/Input/AGENTS.md` — remove the obsolete explanation that shared `FrameInput` forces the game Input header into the server build.

Read-only compatibility consumers to inventory and compile unchanged:

- `Engine/Source/Frame/FrameBase.h/.cpp`;
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.h/.cpp`;
- `Projects/BrokenEngineSandbox/Source/Network/Client/ReconcileReplayTick.cpp`;
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerBroadcaster.cpp`;
- `Documents/Architecture/Network.md`.

## In scope

- Move the `FrameInput` declaration from `Source/Input/Input.h` to new `Source/Frame/FrameInput.h`.
- Move `FrameInput::Crc()` and both stream operators from `Source/Input/Input.cpp` to new `Source/Frame/FrameInput.cpp`.
- Preserve `kiVersion == 15` and exact CRC, stream, byte-order, count-validation, and variant-validation behavior.
- Replace the `Frame/Frame.h` include.
- Add direct display-input includes to `Main.cpp` and `Game.cpp`; retain the existing `GameBase.cpp` include. Add the camera include only when the current tree still contains its `gpInput` reader; never reintroduce it after D8. Verify no other complete consumers are omitted.
- Add the new shared header and source to both client/server projects and filters.
- Make the remaining game `Input.h/.cpp` whole-file client-only.
- Remove the old game `Input.h/.cpp` from the server project and server filters while retaining them in the client project and client filters.
- Update the named input/frame ownership documentation.

## Out of scope

- Any menu flag, camera input, raw-input polling, edge-detection, wheel, gamepad, ImGui, cursor, or other display-input behavior.
- Any `StatusChange` declaration, payload, enumerator, variant ordering, or serialization helper.
- Any change to `FrameInput::kiVersion`; it must remain `15`.
- Any change to `Frame::kiVersion`, frame CRCs, reconciliation, replay algorithms, network protocol, or difference-stream layout.
- Moving display input to Engine, introducing `InputPoll`, or changing camera ownership; those belong to D9B and D8.
- Removing game `Input.h/.cpp` from the client project; D9B owns that deletion.
- New compatibility aliases, abstractions, or unit tests.
- Changes to architecture diagrams, because phase order and lifecycle do not change.

## Risk tier and invariants

Tier 3: the change touches shared client/server deterministic serialization and changes client/server source affinity for the remaining display-input files.

Preserve these invariants:

- `game::FrameInput` remains the same type with the same public members and namespace.
- `kiVersion` remains exactly `15`.
- Serialized bytes remain ordered as count, then each status type, then its payload.
- CRC bytes remain mixed in status order, type first and payload second.
- Unknown status tags are rejected before variant seating.
- `DefaultDataForType` still selects the variant alternative before payload reads.
- `FrameInput.h/.cpp` compile in both client and server builds.
- The display `Input.h/.cpp` compile only in the client build after the split.
- No duplicate declaration or operator definition remains.
- No client-only guard surrounds the shared `FrameInput` implementation.
- The moved display code retains all current bindings and guards; this Plan changes only file ownership and direct includes.

## Coordination

This Plan has no prerequisite.

It is the prerequisite for `Documents/Plans/Input/DisplayInputToEngine.md`. D9B must wait for this Plan to land, consume the client-retained game display-input files, and not assume those files remain in the server project.

Do not run D9B concurrently with this Plan because both touch the game Input ownership and project membership.

## Acceptance criteria

- Source inventory finds exactly one `FrameInput` declaration, one `Crc()` definition, and one definition for each stream operator; none remains in `Source/Input/Input.*`.
- The moved declaration reports `kiVersion == 15`.
- Review of old and new implementation regions confirms unchanged CRC loop, stream field order, count validation, unknown-tag rejection, and `DefaultDataForType` seating.
- The `Frame/Frame.h` include is replaced; direct display-input includes exist in `Main.cpp`, `Game.cpp`, and the already-direct `GameBase.cpp`. `Graphics/Camera.cpp` has the include exactly when its current implementation still dereferences `gpInput`; a post-D8 tree has neither. The complete consumer inventory finds no omission.
- The game Input header and source are whole-file client-only, the client project retains them, and the server project/filter contain neither.
- Both client and server standard `/compile` builds succeed.
- The existing Debug single-coordinate replay acceptance sequence is run without adding test code: capture relevant-log baselines, record multiple ticks, stop recording, start playback, require a newly appended `End replay <tick>, looping` marker, compare the ordered `DifferenceStreamWriter` and `DifferenceStreamReader` checksum sequences, require no new replay/CRC/checksum/desync errors, and cancel playback cleanly. This is the minimal replay signal that the moved stream operators remain compatible.
- `plan validate` reports valid metadata and no dependency errors when this body is wrapped with its plan metadata.
- No unit tests are added.

## Execution card

### What does this plan do?

It separates the shared deterministic `game::FrameInput` type and replay serialization from display-rate game input. It also makes the remaining display-input files explicitly client-only and removes them from the server project, while retaining the client implementation until D9B.

### Why this is good for the codebase

The deterministic frame-input contract becomes visibly owned by the Frame subsystem instead of being hidden inside hardware-input code. The client/server affinity then matches actual use, and D9B can move display input without carrying shared replay code along.

- Goal: Relocate `FrameInput` to `Source/Frame`, preserve all bytes/CRC/version behavior, and make residual display input client-only.
- Out of scope: Display-input semantics, D8 camera ownership, D9B engine Input, replay algorithm changes, protocol changes, and tests.
- Tier trigger: Tier 3 because shared deterministic serialization and client/server project affinity are exposed.
- Interfaces and invariants: `game::FrameInput`, `kiVersion == 15`, exact CRC/stream order, trust-boundary validation, direct include ownership, shared Frame membership, client-only residual Input membership.
- Acceptance checks:
  - Source identity review → one declaration and one implementation set, with exact algorithm preserved.
  - Include inventory → all complete display-input consumers have direct includes.
  - Client build → exit `0`.
  - Server build → exit `0`, with no game display-input source.
  - Minimal replay acceptance → completed single-coordinate playback loop, matching checksums, no new replay/CRC/desync errors.
  - Plan validation → `status: valid`, `code: ok`.
- Roles:
  - Preparation/implementation: implementer.
  - Plan review: fresh `/plan-audit` and `/plan-simplicity-review` reviewers.
  - Build: builder through `/compile`, client and server.
  - Replay acceptance: `/agent-harness` operator.
  - C++ correctness: fresh `/repo-code-review` reviewer.
  - Scope: fresh `/scope-review` reviewer.
  - Tier-3 fresh-eyes: `/adversarial-review` reviewer.
  - Propagation/docs: implementer through `/update-affected-code` and `/update-claude-docs`.
  - Project/style: mechanic through `/update-vcxproj` and `/code-style-review`.
  - Landing gate: fresh `/verify-changes` reviewer.
  - No unit-test role.
