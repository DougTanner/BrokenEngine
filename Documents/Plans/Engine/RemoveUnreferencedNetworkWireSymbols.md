<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:39:33.006Z","dependsOn":[]} -->
# Remove unreferenced Network wire symbols

## Context

Paths are under `Engine/Source/Network/`. Verified against commit `10d84ca6`
by whole-word searches of `Engine`, `Projects`, `Tools`, `DataPacker`, and
`Common`, repeated over the whole tree including Markdown; the lines cited
are those at commit `c675b6b2`.

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

Originating gap: the architecture phase (module cohesion and AI-generation
residue lenses) of an analysis of `Engine/Source/Network` at that commit.
Unmet criterion: "every listed name has at least one reader or is removed".

## Design

Recommended: delete each listed definition.

- `:880` and `:881` go together with `:908`, `:909`, `:922`, and `:923`;
  `:825` goes with its only user `:826`.
- Removing the `GridCoord` arm leaves only the arithmetic case, so the
  `static_assert` text at `NetworkCursor.h:165`, which names `GridCoord` as
  supported, is corrected in the same edit.
- `MessageReader::AtEnd` is deleted with the rest.
  `Documents/Plans/Engine/RemoveIslandPlacementsFromStaticDataWire.md` keeps
  both `AtEnd` and a trailing-byte check out of its scope, so it gives
  `AtEnd` no reader.

No wire layout or contract row changes, so `kiProtocolVersion` stays.

## Critical files

- `Engine/Source/Network/NetworkMessages.h`
- `Engine/Source/Network/NetworkProtocol.h`
- `Engine/Source/Network/NetworkCursor.h`

## In scope

- The definitions listed in `## Context`, each at the line given there.
- The `static_assert` message in `PushSimplePacketArgument`.

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
- Every `Visit`, every contract row, and every constant that has a reader.

## Risk tier

Tier 2 — scoped behavior. Trigger: it removes public constants and a public
member function from engine headers, which Tier 1 excludes. No runtime
behavior, determinism/CRC, wire, serialization, version, or threading
exposure: every removed name is unread.

## Acceptance criteria

1. None of the listed names remains.
2. The BrokenEngineSandbox client and server and ModelViewer build through
   `/compile`.

## Coordination

`Documents/Plans/Engine/RemoveCoordUpdateTickReader.md` and
`Documents/Plans/Engine/RemoveIslandPlacementsFromStaticDataWire.md` edit
`NetworkMessages.h` or `NetworkProtocol.h` too. None of the three depends on
another. Whichever lands later re-derives the lines of its references from the
landed headers.

## Notes

- The executing session confirms no private-game use of the removed names
  where that source is available, and reports it as unconfirmed otherwise.
