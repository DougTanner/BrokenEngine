<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:39:23.085Z","dependsOn":[]} -->
# Separate the responsibilities NetworkProtocol.h carries

## Context

Paths are under `Engine/Source/Network/` unless they start elsewhere. Verified
against commit `10d84ca6`; the lines cited are those at commit `c675b6b2`. No
build was run for this evidence.

`NetworkProtocol.h` carries three responsibilities: packet identifiers and
constants, the save identity type `ClientGuid`, and contract rows sized from
the message layouts. Three defects follow from that one cause.

Include cycle resolved by position:

- `NetworkProtocol.h:152` includes `Network/NetworkMessages.h` mid-file,
  between the identifier half and the contract rows.
- `NetworkMessages.h:3-4` includes only `Frame/Collections/CollectionId.h`
  and `Network/NetworkCursor.h`, yet uses `PacketType` (`:71`, `:168`,
  `:337`, every `keType` from `:482`), `ClientGuid` (`:129`, `:260`, `:564`,
  `:591`), and `kiMaximumCrashReportSize` (`:656`), all declared in
  `NetworkProtocol.h` (`:13`, `:127`, `:110`).
- The lower half of `NetworkProtocol.h` needs the layouts (`:163` and the
  rows in `:176-260`).
- So `NetworkMessages.h` does not compile when included first. Today nothing
  includes it first: its only includers are `Engine.h:14` and
  `NetworkProtocol.h:152`. Adding one include of `NetworkProtocol.h` to
  `NetworkMessages.h` does not fix it, because the nested lower half would
  then use layouts not yet declared.

Identity type in the wire-protocol header:

- `NetworkProtocol.h:114-150` defines `Guid128`, `ClientGuid`, six
  `static_assert`s about `ClientGuid.bin` and game saves, and the `std::hash`
  specialization.
- `Engine/Source/Frame/FrameRegistry.h:4` and
  `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:4` include
  `Network/NetworkProtocol.h`, each commented `// ClientGuid`, and so take
  the whole protocol closure for that one type. This is the only
  Frame-to-Network include; Network already includes Frame headers.

Slot ceiling stated twice:

- `ClientAckStreamMessage::kiMaxSlotCount` (`NetworkMessages.h:483`) and
  `NetworkManager::kiMaximumEnetCoordinateSlots` (`NetworkManager.h:21`) are
  both 64, tied only by `static_assert`s in a server-only file
  (`Server/Server.cpp:18-22`).
- The comments at `NetworkProtocol.h:161-162` and `Server/Server.cpp:18-19`
  say `NetworkProtocol.h` cannot include `NetworkManager.h`. A reading of
  `NetworkManager.h:1-76` contradicts that: it includes only
  `Network/NetworkCursor.h` and names no `NetworkProtocol.h` symbol.

Originating gap: the architecture phase (dependencies and layering, and
module cohesion lenses) of an analysis of `Engine/Source/Network` at that
commit. Unmet criteria: each of the two headers compiles when included
first; no Frame header includes `Network/NetworkProtocol.h`; the
acknowledgment-stream slot bound is derived from the transport constant, not
restated.

## Design

Three parts. Every layout and assert moves verbatim, so no wire or save
format changes and no version increments.

Part 1: move `NetworkProtocol.h:114-150` (the type, its asserts, and the
hash) into `Frame/Collections/CollectionId.h`. That header is already
included by `FrameRegistry.h:3` and `NetworkMessages.h:3`, and the comment at
`NetworkProtocol.h:114-115` names it as the sibling, so no new file is
needed. Then drop the two `// ClientGuid` includes. Two points the authoring
session settled by reading:

- `BT_OFFSETOF` is visible there. `Common/ExternalHeaders.h:471-473` defines
  it, and each program's `Pch.h` includes that file first
  (`Projects/BrokenEngineSandbox/Source/Pch.h:3`,
  `Projects/ModelViewer/Source/Pch.h:3`), before `Engine.h` reaches
  `CollectionId.h`, which has no includes of its own and already takes its
  other names from the precompiled header.
- No ownership wording rejects the type. `Engine/Source/Frame/AGENTS.md` and
  `Engine/Source/Frame/Collections/AGENTS.md` do not say where the type
  lives, and `## Frame Registry` of the first already gives Frame the
  registry's client-GUID write (`AssignRegistryClientGuid`). No sentence in
  either file changes.

Part 2, the shape the user chose in the authoring session: the identifier
half of `NetworkProtocol.h` (`:1-112`, everything above the moved type)
becomes a new header, `Network/NetworkProtocolIdentifiers.h`.
`NetworkMessages.h` includes it, and `NetworkProtocol.h` includes
`Network/NetworkMessages.h` at its top and keeps the layout-dependent tail
(from `:154` to the end). The contract rows stay where `AGENTS.md`
`## Ownership` and `Documents/Architecture/Network.md:300` say they are, and
`NetworkMessages.h`, already an oversized file, does not grow. The new
header is added to each project and filter through `/update-vcxproj`, and
the `NetworkProtocol` bullet of `## Ownership` gains one clause naming it as
the home of the packet identifiers and compatibility constants. Moving the
tail into `NetworkMessages.h` instead was rejected because it would grow
that file and move the contract rows out of the header both documents name.

Part 3, with a build-verified step: `NetworkMessages.h` includes
`Network/NetworkManager.h` and `kiMaxSlotCount` takes its value from
`NetworkManager::kiMaximumEnetCoordinateSlots`; the two tying asserts at
`Server/Server.cpp:20-21` and their comment go, the separate check at `:22`
stays, and the comment at `NetworkProtocol.h:161-162` is corrected. That the
include is legal is a read-only conclusion, so the build verifies it. If the
client, server, or ModelViewer build rejects it, keep the asserts and only
correct the two comments to say what is true.

## Critical files

- `Engine/Source/Network/NetworkProtocol.h`
- `Engine/Source/Network/NetworkMessages.h`
- `Engine/Source/Frame/Collections/CollectionId.h`
- `Engine/Source/Frame/FrameRegistry.h`
- `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h`
- `Engine/Source/Network/Server/Server.cpp`
- `Engine/Source/Network/AGENTS.md`, `Documents/Architecture/Network.md`
- `Engine/Source/Network/NetworkProtocolIdentifiers.h` (new), and each
  project and filter file listing engine headers

## In scope

- `Guid128`, `ClientGuid`, its `static_assert`s, and its `std::hash`
  specialization: their location only.
- The `// ClientGuid` includes in `FrameRegistry.h` and the game
  `StatusChange.h`.
- The mid-file include at `NetworkProtocol.h:152`, the move of
  `NetworkProtocol.h:1-112` into the new `NetworkProtocolIdentifiers.h`, the
  include of that header in `NetworkMessages.h`, and the new header's project
  and filter membership.
- `ClientAckStreamMessage::kiMaxSlotCount`, the asserts and comment at
  `Server/Server.cpp:18-21`, and the comment at `NetworkProtocol.h:161-162`.
- The `NetworkProtocol` bullet of `AGENTS.md` `## Ownership`, and any
  `Documents/Architecture/Network.md` sentence that names `NetworkProtocol.h`
  as the home of a moved constant (`:126`, `:174`, `:197`).

## Out of scope

- Any change to a layout, contract row value, constant value, or assert text.
- `NetworkSimulation.h` and the other headers that rely on the `Engine.h`
  include order: that order is the documented aggregation convention
  (`Engine/Source/AGENTS.md` `## Hub Conventions`).
- The `kiCoordinateSlots` check at `Server/Server.cpp:22`.
- Reducing the size of `NetworkMessages.h`.

## Risk tier

Tier 3 under `.agents/references/risk-tiers.md` (the author's
classification). Trigger: the change spans independently owned subsystems,
moving a type between Network and Frame headers that the game's Frame code
includes, and it adds a header to every project. No runtime behavior
changes. Invariants to hold:

- `ClientGuid` keeps its size, alignment, member offsets, and hash, so
  `ClientGuid.bin`, game saves, and the wire are unchanged.
- `kiProtocolVersion` and every save version stay.
- `AGENTS.md` `## Ownership` and `Network.md` describe the landed files.

## Acceptance criteria

1. `NetworkProtocolIdentifiers.h` holds the packet identifiers and constants,
   `NetworkMessages.h` includes it, and `GetClientPacketContract` is still in
   `NetworkProtocol.h`.
2. No Frame header includes `Network/NetworkProtocol.h`.
3. `NetworkProtocol.h` holds no mid-file include, and each of the two headers
   compiles when included first, shown by a temporary, uncommitted swap of
   their two include lines in `Engine.h` that builds.
4. Either `kiMaxSlotCount` is derived from
   `NetworkManager::kiMaximumEnetCoordinateSlots` and the two tying asserts
   are gone, or the report states which build rejected the include and the
   two comments no longer claim the include is impossible.
5. The BrokenEngineSandbox client and server and ModelViewer build through
   `/compile`.

## Coordination

`Documents/Plans/Engine/RemoveCoordUpdateTickReader.md`,
`Documents/Plans/Engine/RemoveUnreferencedNetworkWireSymbols.md`, and
`Documents/Plans/Engine/RequireClientHelloStringTerminator.md` edit
`NetworkMessages.h` or `NetworkProtocol.h` too. Each edits one symbol in the
identifier half of `NetworkProtocol.h` (`:1-112`) that Part 2 moves into
`NetworkProtocolIdentifiers.h`: `PacketTypeName` (`:46-81`),
`kiProtocolVersion` (`:83`), and `kiMaximumPacketSize` (`:108`). None of the
four depends on another. Whichever lands later re-derives the file as well
as the lines of its references from the landed headers: this Plan moves that
half as it then stands, and once this Plan has landed
`NetworkProtocolIdentifiers.h` is the critical file for those three symbols.

## Notes

- Do not justify the type move by rebuild cost: every header named here is
  in the precompiled header of each program, so any edit rebuilds it. The
  gain is that the Frame-to-Network include disappears.
- `ClientGuid` is named in 37 source files; they need no edit, because the
  type keeps its name and namespace.
- The executing session confirms the private game still compiles where that
  source is available, and reports it as unconfirmed otherwise.
