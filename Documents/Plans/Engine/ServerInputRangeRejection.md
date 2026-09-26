<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-26T22:51:30.707Z","dependsOn":[]} -->
# Reject out-of-range client packet fields and invalid save island placements on the server

## Context

The user set a direction-based trust-boundary policy this session. The parts
this Plan implements, as user decisions:

1. All incoming network records are checked for non-corruption. For
   client-to-server records the server makes certain that every record is
   uncorrupted: values within their expected range, never NaN or infinite.
2. An out-of-range client value is rejected, never clamped (user answer Q8a).
3. Saves are server input and get full validation (user answer Q10).

Line numbers are at `17c1ca52`; re-derive them at claim.

The trust-boundary survey found these gaps against that policy:

- **Navigation-delay clamp.** `ValidateNavigationDelay`
  (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:69-74`)
  clamps a finite wire value to `[0, 60]` and maps NaN/Inf to 60. Both
  `kClientUpdatePlayerRequest` (`:104`) and `kClientFleetNavigationDelay`
  (`:152`) pass it the raw float. Legitimate clients never send an
  out-of-range value: the HUD slider uses `ImGuiSliderFlags_AlwaysClamp` over
  `[0, 60]` (`Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp:346`),
  and the update-player request resends the player's current
  server-authoritative delay (`HudScreen.cpp:414-416`).
- **Boolean bytes.** `bUseMissiles` (`ServerSession.cpp:103`), the pause byte
  (`:213`), and the timespeed direction byte (`:222`) treat any nonzero byte as
  true. Every client writer sends exactly 0 or 1
  (`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:284`;
  `Projects/BrokenEngineSandbox/Source/Game.cpp:613`, `:620`, `:629`).
- **Diagnostic ticks.** `Server::ClientDesyncReport` and
  `Server::ClientDebugFrameRequest`
  (`Engine/Source/Network/Server/ServerReceive.cpp:148-174`, `:176-247`) use
  `iTick` only as a log value or a ring-buffer lookup key, and never range-check
  it. Client ticks are never negative. `GridCoord` fields and the two CRCs have
  no invalid values: the grid is unbounded and a CRC is any 64-bit value.
- **Save island placements.** `ReadGridSave`
  (`Engine/Source/File/GridSave.cpp:116`) reads each cell's
  `FrameStaticData` placements (`Engine/Source/Frame/FrameStaticData.cpp:23-37`)
  with no value check. `Engine/Source/File/AGENTS.md` `## Grid Saves` records a
  deliberate exemption: a save naming an island template the build lacks is
  adopted, and the server then terminates at that cell's first navigation
  build. `Autoload` runs at every server startup in all configs
  (`Projects/BrokenEngineSandbox/Source/Game.cpp:48-52`). So a corrupt save
  crashes a shipping server instead of falling back to a fresh game. That
  conflicts with decision 3. `gpIslandTerrain` is constructed before
  `game::Game` on the server (`Engine/Source/Main.cpp:358-365`), so the
  template map is available when `Autoload` runs.

Every other client-to-server field the survey found already has a
contract-gated size and a value check that reaches simulation state
(`Documents/Architecture/Network.md` `## Client → Server Contract`).

## Design

Author's recommendations, with rationale:

- **Navigation delay.** Replace the clamp with a rejection. A value that is
  non-finite or outside `[0.0f, 60.0f]` throws `std::ios_base::failure` inside
  the existing `try`. The game-packet catch (`ServerSession.cpp:230-238`)
  already drops the packet before any mutation and records one violation
  through `RecordGamePacketHandlerThrow`. Rename the helper to say that it
  admits or rejects rather than clamps, and keep its one shared range for both
  packet types. Decision 2 fixes reject-over-clamp. Reusing the existing catch
  adds no new failure path.
- **Boolean bytes.** Reject a byte other than 0 or 1 through the same throw, in
  the three cases named above. The protocol defines these fields as 0/1, so any
  other byte is corruption.
- **Diagnostic ticks.** In both handlers, reject `iTick < 0` by throwing
  `std::ios_base::failure` after `NetworkMessages::Read`. The `Server::Receive`
  catch (`Engine/Source/Network/Server/Server.cpp:327-336`) records the
  violation. Do not add an upper bound. Neither message carries a load
  generation, and a debug load can move the server clock backward while an
  older report is in flight, so a tick above the current server tick is stale
  protocol state, not corruption (`Engine/Source/Network/AGENTS.md`
  `## Corrupt Input Policy`). Keep the throw ahead of the cooldown update, so a
  corrupt packet does not consume the cooldown.
- **Save island placements.** Remove the exemption and validate in
  `ReadGridSave`, right after each `staticData.Read`. Every placement's
  `islandCrc` must name a template in `gpIslandTerrain->mIslands`, and
  `f2WorldPos.x`, `f2WorldPos.y`, and `fRotation` must be finite. A violation
  throws `std::ios_base::failure` inside the existing `try`, so the load returns
  false and the caller falls back to a fresh game (`GridSave.cpp:150-154`). The
  check goes in `ReadGridSave`, not `FrameStaticData::Read`, because the client
  also calls that reader for network static data, and server-to-client data is
  outside this Plan. The survey recorded only the non-finite hazard for
  placement positions and rotations. It established no valid numeric range, so
  this Plan adds no range bound beyond finiteness.

Nothing here changes a wire layout, a save layout, a CRC, or a deterministic
Frame value for valid input.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp` — `ValidateNavigationDelay`, the `kClientUpdatePlayerRequest`, `kClientFleetNavigationDelay`, `kClientPauseRequest`, and `kClientTimespeedRequest` cases, and the contract-gate comment at `:80-83`.
- `Engine/Source/Network/Server/ServerReceive.cpp` — `Server::ClientDesyncReport`, `Server::ClientDebugFrameRequest`.
- `Engine/Source/File/GridSave.cpp` — `ReadGridSave` per-cell loop.
- `Engine/Source/File/AGENTS.md` — `## Grid Saves` exemption bullet.
- `Documents/Architecture/Network.md` — contract-table rows for `kClientDesyncReport`, `kClientDebugFrameRequest`, `kClientUpdatePlayerRequest`, `kClientFleetNavigationDelay`, `kClientPauseRequest`, `kClientTimespeedRequest`.

## In scope

- `ServerSession.cpp`: the navigation-delay helper (clamp → reject) and its two
  call sites. The 0/1 byte checks in the update-player, pause, and timespeed
  cases. The helper comment and the contract-gate comment that describe the
  clamp.
- `ServerReceive.cpp`: one `iTick < 0` rejection each in
  `ClientDesyncReport` and `ClientDebugFrameRequest`.
- `GridSave.cpp` `ReadGridSave`: the per-placement template-membership and
  finiteness check after `staticData.Read`.
- `Engine/Source/File/AGENTS.md` `## Grid Saves`: replace the island-placement
  exemption bullet with the new rule (placements naming an unknown template or
  carrying a non-finite position or rotation abort the load).
- `Documents/Architecture/Network.md`: the residual-validation column of the
  six rows named above, which must state the rejection rules.

## Out of scope

- Server-to-client data, the ENet checksum, the root and hub policy text, and
  every client-side check. Other Plans own those.
- Save-read normalizations that substitute a value instead of rejecting it: the
  `ReadFleet` navigation-delay clamp and `fFrameChangeTimer` substitution
  (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:81-92`),
  `AdmitSpawnTimer`'s non-finite-to-zero rule
  (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:17-28`), and the
  Explosions trail-count clamp
  (`Engine/Source/Frame/Collections/Explosions/Explosions.h:171-184`). The user's
  reject-never-clamp answer was given for client-to-server records. Extending it
  to save reads is not decided.
- The id-map identity gap at the save read (owned by
  `Documents/Plans/Engine/CollectionIdMapIdentity.md`).
- An upper tick bound on the diagnostic requests, and any range check on
  `GridCoord` or CRC fields.
- `FrameStaticData::Read`, the client static-data path, and island placement
  generation.
- Replays and the agent command channel. Both are exempt developer tools (user
  answer Q9a). A replay's initial grid still passes through `ReadGridSave` and
  so gets the new placement check as a side effect.
- Contract sizes, caps, the violation threshold, and protocol or save versions.

## Risk tier and invariants

Change Workflow Tier 3. Trigger: the change spans independently owned
subsystems (engine Network Server receive, game Network Server session, engine
File grid save) at server trust boundaries.

Invariants:

- A rejected client packet is dropped before any mutation and costs exactly one
  contract violation, through the existing catches.
- Legitimate clients produce zero violations: every client writer already
  sends in-range values and 0/1 bytes.
- A rejected save aborts before staged adoption, and the server falls back to a
  fresh game. A valid save loads with an unchanged grid, Frame, and CRC.
- Deterministic Frame state, CRC, wire layout, save layout, and
  `kuiProtocolVersion` are unchanged.

## Acceptance criteria

- The diff shows the navigation-delay helper rejecting non-finite and
  out-of-`[0, 60]` values through `std::ios_base::failure`, with no clamp or
  substitution left in the two packet cases.
- The diff shows 0/1 enforcement in the three byte cases and `iTick < 0`
  rejection in both diagnostic handlers.
- The diff shows `ReadGridSave` rejecting an unknown `islandCrc` and non-finite
  placement fields inside its existing `try`. The `## Grid Saves` exemption
  bullet is gone, and a bullet stating the rejection replaces it.
- The `Network.md` rows state the new rules.
- A live server run loads an unmodified autosave, and a live client connects,
  changes a fleet's navigation delay, toggles weapons, and pauses with zero
  contract violations logged.
- Client and Server `Debug|x64` build clean through `/compile`.

## Notes

The survey rated the remaining client-to-server packets as already compliant.
