<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-11T00:00:15.000Z","dependsOn":[]} -->
# Promote the duplicated 128-bit identifier to engine::Guid128

## Context

`game::FleetGuid` (`Projects/BrokenEngineSandbox/Source/Fleet.h:8-15`) and `engine::ClientGuid` (`Engine/Source/Network/NetworkProtocol.h:90-101`) are the same type written twice: two `uint64_t` members named `uiHigh` and `uiLow` in the same order, both zero-initialized, both with `= default` equality. Their hash functors `game::FleetGuidHash` (`Fleet.h:17-23`) and `engine::ClientGuidHash` (`NetworkProtocol.h:103-109`) have character-identical bodies. The game header says so out loud at `Fleet.h:7`: "Mirrors engine::ClientGuid shape."

Only two things differ. `ClientGuid` carries `static constexpr int64_t kiVersion = 2` (`NetworkProtocol.h:94`), which is the on-disk header version for `ClientGuid.bin` and is consumed by `engine::ReadVersionedFile`/`WriteVersionedFile` through `STRUCT_TYPE::kiVersion` (`Engine/Source/File/FileManager.h:342-346`; the header also validates `iSize == sizeof(STRUCT_TYPE)`). And `FleetGuid` spells its emptiness test `IsValid()` while `ClientGuid` spells it `IsEmpty()`.

## Design

Replace `ClientGuid`/`ClientGuidHash` in `Engine/Source/Network/NetworkProtocol.h` with `engine::Guid128` (the two members, `IsEmpty()`, defaulted equality, no `kiVersion`) and `engine::Guid128Hash` carrying the existing hash body, at the same spot in the header. Then:

- `using ClientGuid = Guid128;` and `using ClientGuidHash = Guid128Hash;` immediately after, so every existing engine and game `ClientGuid` reference — `NetworkMessages.h`'s `OptionalGuid`/`ConnectionResponseTail`, `ClientConnection`, `ClientSpawnInfo`, fleet-owner maps, `ServerFleetSerialization.cpp:154`'s aggregate `{uiGuidHigh, uiGuidLow}` — keeps compiling unchanged.
- The `kiVersion` member cannot ride on the shared type, because it describes one specific file. The only `ReadVersionedFile`/`WriteVersionedFile` uses of `ClientGuid` are the two `ClientGuid.bin` call sites `LoadClientGuidFromDisk`/`PersistClientGuidToDisk` (`Engine/Source/Network/Client/ClientSessionRuntime.cpp:24`, `:36`). In that file's existing anonymous namespace, add `struct ClientGuidFile { static constexpr int64_t kiVersion = 2; Guid128 guid {}; };` with `static_assert(sizeof(ClientGuidFile) == sizeof(Guid128))` as the layout proof; the two functions read/write the wrapper and pass `.guid` through (the persist callback signature `void(*)(const ClientGuid&)` is unchanged). Same 16 bytes in the same order, so existing `ClientGuid.bin` files keep loading and the version stays 2.
- In `Fleet.h`, the `FleetGuid`/`FleetGuidHash` structs are replaced by `using FleetGuid = engine::Guid128;` and `using FleetGuidHash = engine::Guid128Hash;`, keeping the `:6-7` identity comment (reworded to say the alias). `engine::Guid128` is visible there without a new include because the game PCH force-includes `Engine.h`, which aggregates `NetworkProtocol.h`.
- Every existing `FleetGuid` reference then keeps compiling except one: `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.h:22` forward-declares `struct FleetGuid;`, which cannot coexist with an alias declaration for the same name. Delete that forward declaration and add `#include "Fleet.h"` (the alias owner) to `ClientSession.h`, so the four `const FleetGuid&` parameters at `ClientSession.h:73-76` see the alias.
- The four `IsValid()` call sites on a `FleetGuid` — `FleetSelection.cpp:130`, `Network/Server/ServerBroadcaster.cpp:36`, `:59`, `Network/Server/ServerFleetManager.cpp:295` — become `IsEmpty()` with the sense inverted at each site (`FleetSelection.cpp:155` and `:245` are `global_id_t`, not touched).

No file is added or deleted, so `/update-vcxproj` is not triggered.

## Critical files

- `Engine/Source/Network/NetworkProtocol.h` — `Guid128`, `Guid128Hash`, the two `ClientGuid` aliases
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp` — the `ClientGuidFile` wrapper and the two persistence call sites (`:24`, `:36`)
- `Projects/BrokenEngineSandbox/Source/Fleet.h` — structs replaced by aliases
- `Projects/BrokenEngineSandbox/Source/FleetSelection.cpp`, `Network/Server/ServerBroadcaster.cpp`, `Network/Server/ServerFleetManager.cpp` — the four `IsValid()` sites
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.h` — the `struct FleetGuid;` forward declaration at `:22`, replaced by the `Fleet.h` include

## In scope

- Adding `Guid128`/`Guid128Hash` and aliasing `ClientGuid`, `ClientGuidHash`, `FleetGuid`, `FleetGuidHash` onto them
- The `ClientGuidFile` version wrapper, its `static_assert`, and the two call sites
- Rewriting the four `IsValid()` call sites as negated `IsEmpty()`
- Removing the `struct FleetGuid;` forward declaration at `ClientSession.h:22` and adding the include that makes the alias visible to the four declarations at `ClientSession.h:73-76`
- Any `Engine/Source/Network/AGENTS.md` or game `Network/AGENTS.md` sentence that names the old owner

## Out of scope

- Any change to how a GUID is minted, to fleet identity semantics, or to the two 64-bit draws the fleet RNG consumes per identifier
- Any change to a wire layout: the GUID is written and read as two `uint64_t` fields in existing messages, and those stay as they are
- Any change to the `ClientGuid.bin` on-disk version number or byte layout
- The engine message visitors — owned by `Documents/Plans/Network/EngineMessageVisitorGaps.md` (that plan does not touch the GUID fields; the two plans are order-independent)

## Risk tier and invariants

Tier 3 — both identifiers are persisted and both appear on the wire. Invariants that must be proven before aliasing, not assumed: `sizeof`, member order, and alignment are identical for the old and new types (the wrapper `static_assert` plus the acceptance loads below are the proof). The persistence sites that pin the layout are `ServerFleetSerialization.cpp:33-34` and `:55-56` (fleet GUID written and read as `uiHigh` then `uiLow`), `ServerFleetSerialization.cpp:154` (the fleet-owner map key rebuilt from two `uint64_t`), and the `ClientStateSettings` POD (`ClientSettings.cpp:337-345`, `kiVersion = 3`), where `FleetGuid` is the leading member of a versioned `ClientState.bin` record whose layout and `sizeof` must not shift. `ClientGuid.bin` keeps version 2 and its 16-byte body.

## Acceptance criteria

- Client and server compile; one 128-bit identifier type remains.
- A `ClientGuid.bin` and a `ClientState.bin` written before the change load after it and yield the same values.
- A server save written before the change loads after it with identical fleet ownership.
- A reconnect relinks to the same fleet, and a harness fleet create/delete round trip behaves as before.

## Scores

Effort 1 / Impact 2 / Risk 1
