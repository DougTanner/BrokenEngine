<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:43:56.633Z","dependsOn":[]} -->
# Make the static-data drain a private client runtime member

## Context

Found by the architecture review phase of a deep analysis of
`Engine/Source/Network/Client` at commit `d87ad840`. Line numbers are from
that commit; locate by symbol if they moved.

`ClientSessionBase::ApplyReceivedStaticData`
(`Engine/Source/Network/Client/ClientSessionBase.cpp:49-56`) loops
`mpRuntime->mpClient->mReceivedStaticData` into
`mpRuntime->StageStaticData` and clears the buffer. It touches no base state
and no game state.

Its only caller in the repository is
`ClientSessionRuntime::PollAndDrain`
(`Engine/Source/Network/Client/ClientSessionRuntime.cpp:491`), which reaches
it through `game::gpClientSession->` and then calls its own private
`ApplyReceivedFullStates()` (492) and `ApplyReceivedUpdates()` (493), declared
private at `ClientSessionRuntime.h:150-151`. So the runtime leaves itself
through the game global to run a loop over its own state, and one of three
parallel drains lives in another class.

The documents disagree about the owner:

- `Engine/Source/Network/Client/AGENTS.md` `## Overview` says
  `ClientSessionBase` "owns static-data application"; `## Receive Paths` of
  the same file says "`ClientSessionRuntime` drains static data, full states,
  then deltas".
- `Documents/Architecture/Network.md:529-534` repeats both halves in one
  bullet: the runtime owns the "static→full-state→delta drain order" and the
  base owns "static-data application".
- The class comment at `ClientSessionBase.h:12` lists "static-data
  application".

Two comments name the function for something it does not do. Texture-slot
acquisition for received cells is `gpIslandTerrainResidency->AcquireTextureSlot`
in `ClientSessionRuntime::StartTerrainPreparation`
(`ClientSessionRuntime.cpp:208-211`), yet:

- `Engine/Source/Graphics/Managers/TextureManager.cpp:208` says "Island
  textures load dynamically per ClientSessionBase::ApplyReceivedStaticData".
- `Projects/BrokenEngineSandbox/Source/Game.cpp:269` says "mirrors
  ClientSessionBase::ApplyReceivedStaticData".

Unmet condition: the static-data drain has one owner, and the documents and
comments agree with the code.

Originating gap: that review's module-cohesion lens. It is not an unmet
acceptance criterion of any change. It is pre-existing: the analysis session
changed no source.

## Design

Recommended (the author's recommendation):

- Move the loop and the `clear()` into a private `ClientSessionRuntime` member
  named `ApplyReceivedStaticData`, declared beside `ApplyReceivedFullStates`,
  and call it directly where line 491 calls the base today. Rationale: the
  three drains then sit together and read alike.
- Delete the declaration at `ClientSessionBase.h:23` and the definition at
  `ClientSessionBase.cpp:49-56`.
- Keep `ClientSessionRuntime::StageStaticData` (`ClientSessionRuntime.h:87`)
  as it is, public. It then has only an in-class caller; inlining it or
  narrowing it is not part of this Plan, because keeping it is the smaller
  edit.
- Fix the documents and comments listed in `## In scope`.

`Projects/Coretide` is a private game project that is absent from a session
worktree. This Plan removes a public `ClientSessionBase` member, so the
executing session searches `Projects/Coretide` in the primary checkout for
`ApplyReceivedStaticData` and reports what it finds before it removes the
member.

Risk tier: Tier 2 (the author's classification). Trigger: a public member of
an engine class that game sessions inherit is removed. No packet, CRC, save,
`.pack`, `kiVersion`, or replay change: the same loop runs at the same
position in `PollAndDrain` on the main thread.

## Critical files

- `Engine/Source/Network/Client/ClientSessionBase.h` and
  `ClientSessionBase.cpp`
- `Engine/Source/Network/Client/ClientSessionRuntime.h` and
  `ClientSessionRuntime.cpp`
- `Engine/Source/Network/Client/AGENTS.md`
- `Documents/Architecture/Network.md`
- `Engine/Source/Graphics/Managers/TextureManager.cpp` (comment only)
- `Projects/BrokenEngineSandbox/Source/Game.cpp` (comment only)

## In scope

- `ClientSessionBase::ApplyReceivedStaticData`: its declaration and
  definition, removed.
- A new private `ClientSessionRuntime::ApplyReceivedStaticData` and the one
  call in `ClientSessionRuntime::PollAndDrain`.
- `Engine/Source/Network/Client/AGENTS.md` `## Overview`: the words that give
  `ClientSessionBase` static-data application.
- The class comment above `ClientSessionBase` in `ClientSessionBase.h`: the
  words "static-data application".
- `Documents/Architecture/Network.md`: the words "static-data application" in
  the `ClientSessionBase` sentence of the session-ownership bullet.
- The two comments quoted in `## Context`, reworded to name the code that
  acquires the texture slot, or shortened so they name no function.

## Out of scope

- `ClientSessionRuntime::StageStaticData`, its `IsCurrentIdentity` gate, and
  terrain preparation.
- `ApplyReceivedFullStates`, `ApplyReceivedUpdates`, and
  `ClientSessionBase::ApplyOwnedEntityEvents`.
- `Client::ServerCoordinateStaticData` and the inline admission checks it
  applies.
- Any other `ClientSessionBase` member.

## Invariants

- Drain order stays static data, then full states, then deltas.
- The drain stays after `ApplyOwnedEntityEvents` and
  `ProcessReceivedGamePackets`, and before `PublishPreparedCells`.

## Acceptance criteria

1. A search of `Engine`, `Projects/BrokenEngineSandbox`,
   `Projects/ModelViewer` and `Documents/Architecture` finds no
   `ClientSessionBase::ApplyReceivedStaticData`.
2. `Engine/Source/Network/Client/AGENTS.md` `## Overview` and
   `## Receive Paths`, and `Network.md`, name the runtime alone as the owner
   of the static-data drain.
3. `/compile` builds the BrokenEngineSandbox client and server.
4. Through `/agent-harness`, with a local server: the client connects and
   publishes an authoritative cell, which needs received navigation and so
   proves static data was drained. The supplied evidence records no harness
   setup; the executing session sets it up from the harness documentation.

## Coordination

No Plan below depends on this one, and it depends on none.

- `Documents/Plans/Engine/ClientConnectFrameTransitionInEngine.md` adds
  members to `ClientSessionBase` and edits
  `Engine/Source/Network/Client/AGENTS.md` where it says who owns session
  functions. Whichever lands second applies its edit on top of the landed
  class and `## Overview` text.
- `Documents/Plans/Engine/RemoveIslandPlacementsFromStaticDataWire.md` states
  in its `## Context` that the only static-data drain is `ClientSessionBase`
  into `ClientSessionRuntime::StageStaticData`. After this Plan lands, that
  drain is the private runtime member; the other Plan's scope is unchanged.
- `Documents/Plans/Engine/ReduceClientSessionRuntimeCpp.md` may move
  `PollAndDrain` and the drains to another file. Whichever lands second
  re-derives its regions by symbol from the landed file.
- `Documents/Plans/Engine/ReduceTextureManagerCpp.md` may move the comment
  at `Engine/Source/Graphics/Managers/TextureManager.cpp:208` to another
  file. Whichever lands second finds the comment by its text in the landed
  files.
