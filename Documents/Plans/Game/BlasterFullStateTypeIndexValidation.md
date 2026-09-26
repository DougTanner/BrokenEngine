<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-26T19:54:28.390Z","dependsOn":[]} -->
# Reject out-of-range Blaster type indices in full-state, save, and replay reads

## Context

Blaster rows name their type through the shared column
`BlastersInterpolate::puiTypeIndices`
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.h:46,57`).
Every full Frame read deserializes that column with no semantic check:
`CollectionRead` (through `AllocateAndRead`) and `SharedCollectionRead` only
run an optional `PostRead` hook after a complete read
(`Engine/Source/Frame/Collections/Collection.h:72-83,87-99,410-446,459-464`), and
`BlastersInterpolate` declares none. The readers that reach it are:

- client full state and debug frames: `DecompressAndReadFrame`
  (`Engine/Source/Network/Client/ClientReceive.cpp:15-39`);
- server grid-save load: `fileStream >> *pFrame` in `ReadGrid`
  (`Engine/Source/File/GridSave.cpp:119`);
- the replay reader's saved-start and saved-end header reads and its
  full-frame read (`Engine/Source/File/DifferenceStream.h:282,293,640`), made
  live by `engine::AdoptGridSave` (`Engine/Source/File/Replay.cpp:710`).

The first lookup of the byte is on the client, after adoption:
`ClientSession::HydrateReceivedFullState` calls
`BlastersInterpolate::ClientInitAll`
(`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSessionReceive.cpp:35-40`,
from `ClientSessionRuntime::ApplyReceivedFullStates`,
`Engine/Source/Network/Client/ClientSessionRuntime.cpp:285-309`), which calls
`ClientInit` → `GetType` (`Blasters.cpp:42-47,110-116`) → `sTypes.at`
(`Collection.h:157-160`). `SyncBlaster` makes the same lookup every client
Update (`BlastersUpdate.cpp:133-135`). Server code never looks the type up, so
a bad row loaded from a grid save or replay is adopted and then broadcast to
clients. An out-of-range byte such as `0xFF` therefore throws
`std::out_of_range` during client hydration, outside every corrupt-input catch,
instead of being rejected where the frame is read.

Originating gap: the transfer-only Blaster type-index change (network status
batch decode and replay `FrameInput` read, adding
`game::IsAdoptableStatusChange` in `Projects/BrokenEngineSandbox/Source/SpawnTransfer.h/.cpp`)
assigned "generic full-state collection type-index validation" to
`Documents/Plans/Engine/CollectionTypeIndexValidation.md`. That Plan covers
only explosion `puiTypeIndices` through `ExplosionsInterpolate::PostRead` and
explicitly keeps other collection registries out of scope, so no Plan owns the
Blaster full-state column. Evidence:
`Documents/Plans/Engine/CollectionTypeIndexValidation.md` `## Design` and
`## Out of scope`.

## Design

The author's recommendation is to add a
`static void PostRead(BlastersInterpolate& rCurrent)` hook to
`BlastersInterpolate` that checks every row's `puiTypeIndices` value against
`BlastersInterpolate::sTypes.size()` and throws `std::ios_base::failure` for
any value at or above it (which includes `kuiInvalidTypeIndex`). Rationale:
`NormalizeAfterRead` already runs the hook for both the build-local and the
shared-member read, so one check covers every reader listed above, and each of
those readers already routes `std::ios_base::failure` to its existing outcome —
the client's fatal corrupt-packet path (`Engine/Source/Network/Client/Client.cpp:359-367`,
fatal by design under `Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy`),
`ReadGrid` returning false (`GridSave.cpp:150-153`), the `SaveLoadReplay`
corrupt-replay abort (`Replay.cpp:747-749`), and the full-frame reader
disabling full-frame comparison (`DifferenceStream.h:636-650`). No reader,
catch, or policy changes. The rejection mirrors the transfer-arm test in
`game::IsAdoptableStatusChange`; re-derive that predicate and every line range
before implementing.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.h:25-80` — `BlastersInterpolate`, the new `PostRead` hook, and the `puiTypeIndices` column.
- `Engine/Source/Frame/Collections/Collection.h:72-99,117-160,443` — `NormalizeAfterRead` dispatch and the immutable type registry (read-only evidence).
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.cpp:42-47,110-116` and `BlastersUpdate.cpp:133-135` — client registry consumers (read-only evidence).
- `Engine/Source/Network/Client/ClientReceive.cpp:15-39`, `Engine/Source/File/GridSave.cpp:119,150-153`, `Engine/Source/File/DifferenceStream.h:282,293,636-650`, `Engine/Source/File/Replay.cpp:710,747-749` — readers and their existing catches (read-only evidence).

## In scope

- A new `BlastersInterpolate::PostRead` that rejects any row whose
  `puiTypeIndices` value is not a registered Blaster type by throwing
  `std::ios_base::failure`.
- A one-line trust-boundary comment on that hook.

## Out of scope

- Explosion type indices (owned by
  `Documents/Plans/Engine/CollectionTypeIndexValidation.md`), status-change
  transfer validation, and other collections' type columns.
- Any change to `Collection.h`, the readers, their catches, or the client's
  fatal corrupt-input policy.
- Collection layout, `Frame::kiVersion`, `kuiProtocolVersion`, registry
  contents or registration order, and substituting a default type for corrupt
  input.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: adding a check inside one unit
(`BlastersInterpolate`) at the existing save/replay/full-state read boundary,
with the serialized format and the trust boundary unchanged
(`.agents/references/risk-tiers.md` Tier 2).

Preserve these invariants:

- Every adopted Blaster row names a registered type; an invalid one fails at
  read time, before `ClientInitAll`, `SyncBlaster`, or server adoption.
- Valid frames read, hydrate, CRC, and replay exactly as before; the hook reads
  no client-only column and changes no row.
- The hook runs only after a complete read (`NormalizeAfterRead` gate), so it
  never inspects partial storage.

## Acceptance criteria

- Client and server `Debug|x64` builds clean through `/compile`.
- A diff read shows `BlastersInterpolate::PostRead` rejecting every
  `puiTypeIndices` value at or above `sTypes.size()` with
  `std::ios_base::failure`, and no other source change.

## Notes

`Documents/Plans/Engine/CollectionTypeIndexValidation.md` adds the same shape
of check to `ExplosionsInterpolate::PostRead`. The two Plans touch different
files and land independently in either order, so no dependency or
Coordination section is recommended; the recommendation is to keep them
separate because they sit in different subsystems (engine explosions versus
game Blasters) with different consumers.
