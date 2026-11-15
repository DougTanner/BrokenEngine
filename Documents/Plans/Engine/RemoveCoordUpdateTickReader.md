<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:39:09.614Z","dependsOn":[]} -->
# Remove CoordUpdateTickReader and PacketTypeName

## Context

Paths are under `Engine/Source/Network/` unless they start elsewhere. Verified
against commit `10d84ca6`; the lines cited are those at commit `c675b6b2`,
except the `NetworkSimulation.h` and `EngineSwitches.h` lines, which
`## Coordination` places.

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

Originating gap: the architecture phase and the in-function refactoring phase
of an analysis of `Engine/Source/Network` at that commit. Unmet criterion:
"the visitor interface has two implementers, and the drop log still prints
the tick of a well-formed update or resend".

## Design

Recommended:

- Delete `CoordUpdateTickReader`.
- Rewrite the body of `GetCoordUpdateTickOrZero` to run a `MessageReader`
  over `ServerCoordUpdateMessage::Visit` or `ServerCoordResendMessage::Visit`
  and return `iTick` when the reader stays valid, otherwise zero. The reader
  fills `iTick` (`NetworkMessages.h:897`) and leaves `mbValid` false on a
  type mismatch (`:170-173`), after which its reads consume nothing
  (`:308-312`). Use the reader directly, not `NetworkMessages::Read`, which
  throws (`:453-456`); a dropped simulated packet is not corruption.
- Delete `PacketTypeName` and print the numeric `iPacketType` at
  `NetworkSimulation.h:158`. `Server/Server.cpp:275` already logs the raw
  type byte, so this matches an existing log and removes the parallel list.

The alternative for `PacketTypeName` is to keep it and correct the seven
strings. Deleting is recommended because the list has one developer-log
caller and must otherwise be extended with every packet type.

One accepted behavior difference: a packet cut short after its tick field now
logs tick 0, because the reader's `Payload` has to fit (`:232-236`). It
affects only this log.

No wire layout or contract row changes, so `kiProtocolVersion` stays.

## Critical files

- `Engine/Source/Network/NetworkMessages.h`
- `Engine/Source/Network/NetworkProtocol.h`
- `Engine/Source/Network/NetworkSimulation.h`

## In scope

- `NetworkMessages::CoordUpdateTickReader`: the whole class.
- `NetworkMessages::GetCoordUpdateTickOrZero`: its body.
- `PacketTypeName`: the whole function.
- The drop log statement at `NetworkSimulation.h:155-158`: the type argument
  only.

## Out of scope

- `MessageWriter`, `MessageReader`, and every message layout.
- The rest of `NetworkSimulation.h`, and the committed value of
  `keNetworkSimulation`.
- `PacketType` and its enumerator names.

## Risk tier

Tier 2 — scoped behavior. Trigger: it removes a public class and a public
function from engine headers and changes the text of one developer log,
which Tier 1 excludes. No determinism/CRC, wire, serialization, version, or
threading exposure.

## Acceptance criteria

1. `CoordUpdateTickReader` and `PacketTypeName` no longer exist, and the
   visitor interface has two implementers.
2. With a temporary, uncommitted non-disabled `keNetworkSimulation`, the
   BrokenEngineSandbox client and server build through `/compile`, and in an
   `/agent-harness` session at that level a dropped coordinate update logs
   its real tick. The originating evidence records no harness setup; the
   executing session sets it up from the harness documentation.
3. The landed tree keeps `keNetworkSimulation` at `kDisabled` and builds the
   client, the server, and ModelViewer.

## Coordination

`Documents/Plans/Engine/RemoveUnreferencedNetworkWireSymbols.md` and
`Documents/Plans/Engine/RemoveIslandPlacementsFromStaticDataWire.md` edit
`NetworkMessages.h` or `NetworkProtocol.h` too. None of the three depends on
another. Whichever lands later re-derives the lines of its references from the
landed headers.

`Documents/Plans/Engine/ConsolidatedSmallDeduplications.md` (its item 2)
edits `NetworkSimulation.h` too and needs the same temporary non-disabled
build. Neither depends on the other. Whichever lands later re-derives its
regions from the landed tree. `keNetworkSimulation` is defined in
`Engine/Source/EngineSwitches.h`, moved there after commit `c675b6b2`; the
`NetworkSimulation.h` and `EngineSwitches.h` lines cited here are those after
that move.

## Notes

- The executing session confirms no private-game use of the two removed
  names where that source is available, and reports it as unconfirmed
  otherwise.
