<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:39:51.200Z","dependsOn":[]} -->
# Remove dead Network symbols

## Context

Paths are under `Engine/Source/Network/` unless they start elsewhere. Verified
against commit `10d84ca6`; the lines cited are those at commit `c675b6b2`,
except the `NetworkSimulation.h` and `EngineSwitches.h` lines, which
`## Coordination` places. The unreferenced-name findings were verified by
whole-word searches of `Engine`, `Projects`, `Tools`, `DataPacker`, and
`Common`, repeated over the whole tree including Markdown.

Originating gap: the architecture phase (module cohesion, AI-generation
residue, and client/server and data-shape lenses) and the in-function
refactoring phase of an analysis of `Engine/Source/Network` at that commit.

### Unreferenced wire symbols

Each name below appears only at its own definition, or only inside another
name on this list:

- `MessageReader::AtEnd` (`NetworkMessages.h:215`). It was added for a
  game-side trailing-byte check that no longer exists.
- `ServerConnectionResponseMessage::kiAcceptedGuidSize`
  (`NetworkMessages.h:583`).
- Fourteen server-to-client size constants in `NetworkMessages.h`:
  `ServerSubscribeAcceptMessage::kiFixedSize` (`:672`),
  `ServerUnsubscribeAckMessage::kiFixedSize` (`:693`),
  `ServerLoadNotificationMessage::kiFixedSize` (`:708`),
  `ServerTimespeedUpdateMessage::kiFixedSize` (`:725`),
  `ServerCoordFullStateMessage::kiEnvelopeSize` and `kiFixedSize` (`:825`,
  `:826`), `CoordUpdateFields::kiEnvelopeSize` and `kiFixedSize` (`:880`,
  `:881`), their re-exports in `ServerCoordUpdateMessage` (`:908`, `:909`)
  and `ServerCoordResendMessage` (`:922`, `:923`), and
  `ServerDebugFrameMessage::kiEnvelopeSize` and `kiFixedSize` (`:936`,
  `:937`). Each is a hand-summed second description of a layout its `Visit`
  already owns, with no reader because the client adds no structural checks
  on server data (`AGENTS.md` `## Corrupt Input Policy`).
- `kiMaximumPacketSize` (`NetworkProtocol.h:108`). It reads as a packet
  ceiling, but nothing enforces it; the enforced limit is
  `kiMaximumClientInboundBytesPerTick` (`NetworkProtocol.h:172`).
- The `GridCoord` arm of `PushSimplePacketArgument`
  (`NetworkCursor.h:159-162`). All eight call sites of the simple-packet
  writer pass no argument or one `uint8_t`.

Unmet criterion: "every listed name has at least one reader or is removed".

### CoordUpdateTickReader and PacketTypeName

`NetworkMessages::CoordUpdateTickReader` (`NetworkMessages.h:323-429`) is a
third implementer of the message visitor interface, beside `MessageWriter`
and `MessageReader`. It exists for one caller: `GetCoordUpdateTickOrZero`
(`NetworkMessages.h:956-970`), called only at `NetworkSimulation.h:156`, whose
result feeds only the `kVerbose` drop log at `NetworkSimulation.h:158`. The
class repeats `Type` (`:337-343` against `:168-174`) and lacks the
`Field(float&)` and `BoundedCount` members both other visitors carry.

`PacketTypeName` (`NetworkProtocol.h:46-81`) is a hand-kept list parallel to
the `PacketType` enumerators, which every new engine packet type must extend.
Its one caller is the same log statement. Seven of its strings do not match
their enumerators: `:50`, `:51`, `:52`, `:53` (`Coord` for `Coordinate`),
`:55` (`Desync`), `:56` (`Ack`), and `:62` (`Resync`).

The whole path compiles only when `keNetworkSimulation`
(`Engine/Source/EngineSwitches.h:100`) is not `kDisabled`, which is its
committed value; the callers sit inside `if constexpr`
(`Server/Server.cpp:144-147`, `Client/Client.cpp:182`).

Unmet criterion: "the visitor interface has two implementers, and the drop
log still prints the tick of a well-formed update or resend".

### Uncompressed status-change batch codec

`SerializeStatusChangeBatch` and `DeserializeStatusChangeBatch` are written
as public entry points, but each is the helper of one function in its own
file. Three things follow, and a fourth observation was decided as a
non-change.

- Exported without an outside caller. They are declared at
  `NetworkSerialization.h:46` and `:49`; the only calls are
  `NetworkSerialization.cpp:146` and `:173`. Outside callers use only
  `CompressStatusChangeBatch` (`Server/ServerBufferedFrames.cpp:65`) and
  `DecompressStatusChangeBatch` (`Client/Client.cpp:608`).
  `Documents/C++StyleGuide.txt` rule 66 gives file-local functions `static`.
- The serialized-size bound is derived twice.
  `NetworkSerialization.cpp:139-140` uses
  `kiTypeCount * 3 + iCount * kiMaxStatusChangeBytesPerItem`;
  `MaxSerializedStatusChangeBatchBytes` (`NetworkSerialization.h:27-30`) uses
  `iCount * (kiMaxStatusChangeBytesPerItem + 3)`. Both are valid upper bounds
  today, because a group header is written only for a non-empty group
  (`NetworkSerialization.cpp:18-21`).
  `kiMaxCompressStatusChangeWorkbufferBytes` (`NetworkSerialization.h:34`)
  bounds the Workbuffer high-water (`Server/ServerBroadcaster.cpp:120`) with
  the header formula while the reservation it bounds uses the other one. No
  overflow is reachable; this is drift risk.
- A guard for a state the caller already handled. The zero-count return at
  `NetworkSerialization.cpp:66-69` cannot run: the only caller returned at
  `:132-135` for an empty batch. Without the guard an empty span still
  returns 0, since no group is emitted (`:82-87`).
- No build guards, which stays. `NetworkSerialization.cpp` is compiled by
  both executables with no `BT_CLIENT` or `BT_SERVER` guard, though encode
  has only a server caller and decode only a client caller; the source says
  "Only the client decodes a batch" (`:94-95`). Root `AGENTS.md`
  `## Client/Server Targets` says to guard client/server-only code at the
  narrowest practical scope. The user chose in the authoring session to add
  no guard: `AGENTS.md` `## Ownership` names `NetworkSerialization` as owner
  of the whole codec, and `NetworkMessages.h` keeps its paired writer and
  reader unguarded in both builds, so guards would split a mirrored pair.

Unmet criteria: the two uncompressed functions have internal linkage and no
header declaration; the serialized reservation and the published bound come
from one function; no check exists for a state no caller produces.

## Design

The lines cited are pre-edit lines; after an earlier step shifts a header,
locate each later reference by its name. No wire layout or contract row
changes, so `kiProtocolVersion` stays.

1. Unreferenced wire symbols: delete each listed definition.
   - `:880` and `:881` go together with `:908`, `:909`, `:922`, and `:923`;
     `:825` goes with its only user `:826`.
   - Removing the `GridCoord` arm leaves only the arithmetic case, so the
     `static_assert` text at `NetworkCursor.h:165`, which names `GridCoord` as
     supported, is corrected in the same edit.
   - `MessageReader::AtEnd` is deleted with the rest.
     `Documents/Plans/Engine/RemoveIslandPlacementsFromStaticDataWire.md`
     keeps both `AtEnd` and a trailing-byte check out of its scope, so it
     gives `AtEnd` no reader.
2. `CoordUpdateTickReader` and `PacketTypeName`:
   - Delete `CoordUpdateTickReader`.
   - Rewrite the body of `GetCoordUpdateTickOrZero` to run a `MessageReader`
     over `ServerCoordUpdateMessage::Visit` or `ServerCoordResendMessage::Visit`
     and return `iTick` when the reader stays valid, otherwise zero. The
     reader fills `iTick` (`NetworkMessages.h:897`) and leaves `mbValid` false
     on a type mismatch (`:170-173`), after which its reads consume nothing
     (`:308-312`). Use the reader directly, not `NetworkMessages::Read`, which
     throws (`:453-456`); a dropped simulated packet is not corruption.
   - Delete `PacketTypeName` and print the numeric `iPacketType` at
     `NetworkSimulation.h:158`. `Server/Server.cpp:275` already logs the raw
     type byte, so this matches an existing log and removes the parallel
     list.

   The alternative for `PacketTypeName` is to keep it and correct the seven
   strings. Deleting is recommended because the list has one developer-log
   caller and must otherwise be extended with every packet type.

   One accepted behavior difference: a packet cut short after its tick field
   now logs tick 0, because the reader's `Payload` has to fit (`:232-236`). It
   affects only this log.
3. Uncompressed status-change batch codec:
   - Give both functions `static` (`NetworkSerialization.cpp:62`, `:90`) and
     remove their declarations and comments (`NetworkSerialization.h:44-49`).
   - Replace `NetworkSerialization.cpp:139-140` with a call to
     `MaxSerializedStatusChangeBatchBytes(iCount)` and drop
     `kiMaxGroupHeaders`. This only changes the size of a scratch reservation
     that the high-water constant already assumes.
   - Delete the zero-count guard at `NetworkSerialization.cpp:66-69`.

   The encode-only helpers and constant (`SerializeGroup` at `:14`,
   `kiTypeCount` at `:37`, `GroupIndicesByType` at `:39`) are not touched;
   the decode half (`:90-126`) uses none of them. The wire format does not
   change.

## Critical files

- `Engine/Source/Network/NetworkMessages.h`
- `Engine/Source/Network/NetworkProtocol.h`
- `Engine/Source/Network/NetworkCursor.h`
- `Engine/Source/Network/NetworkSimulation.h`
- `Engine/Source/Network/NetworkSerialization.cpp`
- `Engine/Source/Network/NetworkSerialization.h`

## In scope

- The unreferenced wire symbols listed in `## Context`, each at the line
  given there, and the `static_assert` message in `PushSimplePacketArgument`.
- `NetworkMessages::CoordUpdateTickReader`: the whole class.
- `NetworkMessages::GetCoordUpdateTickOrZero`: its body.
- `PacketTypeName`: the whole function.
- The drop log statement at `NetworkSimulation.h:155-158`: the type argument
  only.
- `SerializeStatusChangeBatch`: linkage, declaration, and the zero-count
  guard.
- `DeserializeStatusChangeBatch`: linkage and declaration.
- `CompressStatusChangeBatch`: the `iMaxSerializedSize` computation only.

## Out of scope

- `WriteUint64` (`NetworkCursor.h:127`): it is the write mirror of
  `ReadUint64`, which is used, and the cursor helpers stay a parallel set.
- `ServerCoordStaticDataMessage::kiEnvelopeSize` (`NetworkMessages.h:854`):
  it feeds `kiFixedSize` (`:855`), which
  `Server/ServerBufferedFrames.cpp:213` reads.
- `ServerOwnedEntityStateMessage::kiFixedSize` (`NetworkMessages.h:775`): it
  has readers.
- The `WriteGridCoord(common::Workbuffer&, ...)` overload
  (`NetworkCursor.h:150-154`): `NetworkMessages.h:112` uses it.
- `MessageWriter`, `MessageReader` apart from `AtEnd`, every `Visit`, every
  message layout, every contract row, and every constant that has a reader.
- The rest of `NetworkSimulation.h`, and the committed value of
  `keNetworkSimulation`.
- `PacketType` and its enumerator names.
- `BT_CLIENT` and `BT_SERVER` guards around the encode half, the decode
  half, or their declarations: decided against in the authoring session, so
  the pair stays unguarded together.
- The batch wire format, `GroupIndicesByType`, `SerializeGroup`, and the LZ4
  framing.
- The values of `kiMaxStatusChangeBytesPerItem`,
  `kiMaxCompressStatusChangeWorkbufferBytes`, and the compressed bound.
- Both outside callers of the compressed codec, and project membership of
  `NetworkSerialization.cpp` and `NetworkSerialization.h`.
- The `Max` and `Maximum` spellings of these names.
- The island-placement wire change of
  `Documents/Plans/Engine/RemoveIslandPlacementsFromStaticDataWire.md`.

## Risk tier

Tier 2 — scoped behavior. Triggers: it removes public constants, a public
member function, a public class, a public function, and two public
declarations from engine headers, changes the text of one developer log, and
changes the size of one server scratch reservation, which Tier 1 excludes.
No determinism/CRC, wire, serialization-format, version, or threading
exposure: every removed name is unread, and `kiProtocolVersion` stays.

## Acceptance criteria

1. None of the listed unreferenced wire symbols remains.
2. `CoordUpdateTickReader` and `PacketTypeName` no longer exist, and the
   visitor interface has two implementers.
3. The two uncompressed functions are `static` and absent from the header;
   `CompressStatusChangeBatch` sizes its reservation with
   `MaxSerializedStatusChangeBatchBytes`; the zero-count guard is gone.
4. With a temporary, uncommitted non-disabled `keNetworkSimulation`, the
   BrokenEngineSandbox client and server build through `/compile`.
5. The landed tree keeps `keNetworkSimulation` at `kDisabled`, and the
   BrokenEngineSandbox client and server and ModelViewer (which includes the
   headers through `Engine.h`) build through `/compile`.
6. `/agent-harness`, with a client connected to a local server, in two runs:
   on the temporary non-disabled `keNetworkSimulation` build of criterion 4,
   a dropped coordinate update logs its real tick; on the landed default
   build of criterion 5, the client keeps applying status-change batches with
   no CRC mismatch. The originating evidence records no harness setup; the
   executing session sets it up from the harness documentation.

## Coordination

`Documents/Plans/Engine/RemoveIslandPlacementsFromStaticDataWire.md` edits
`NetworkMessages.h` and `NetworkProtocol.h` too and lands after this Plan.

`Documents/Plans/Engine/ConsolidatedSmallDeduplications.md` (its item 2)
edits `NetworkSimulation.h` too and needs the same temporary non-disabled
build. Neither depends on the other. Whichever lands later re-derives its
regions from the landed tree. `keNetworkSimulation` is defined in
`Engine/Source/EngineSwitches.h`, moved there after commit `c675b6b2`; the
`NetworkSimulation.h` and `EngineSwitches.h` lines cited here are those after
that move.

## Notes

- The executing session confirms no private-game use of the removed names
  where that source is available, and reports it as unconfirmed otherwise.
