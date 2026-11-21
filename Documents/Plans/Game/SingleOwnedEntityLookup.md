<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T23:51:29.463Z","dependsOn":[]} -->
# One Owned-Entity Lookup in ApplyOwnedEntityEvent

## Context

`ClientSession::ApplyOwnedEntityEvent`
(`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:141`)
searches `gpGame->mClientOwnedEntities` with `std::ranges::contains` (`:151`)
and, when the entity is absent, logs and calls `AddClientOwnedEntity` with
`rEvent.coordinate` (`:153-154`), which already stores the coordinate
(`Projects/BrokenEngineSandbox/Source/Game.cpp:68-73`). After the `kAssigned`
`UpdateDesiredCoordinates` call (`:156`) it calls the private
`UpdateOwnedEntityCoordinate` (`:158`; defined `:172-179`, declared
`ClientSession.h:62`, no other caller), which searches the vector a second time
to write the coordinate.

Writing the coordinate before the `kAssigned` `UpdateDesiredCoordinates` call
instead of after it is unobservable: no engine code reads
`mClientOwnedEntities` (only the game's `FleetSelection.cpp`, `HudScreen.cpp`,
and `Game.cpp` do), and the closing `UpdateDesiredCoordinates` call follows
either way. The `rEvent.globalId.iValue != 0 &&` prefix of the present test
changes the outcome only for global id 0, which a trusted server never sends
for an owned entity (`Engine/Source/Network/AGENTS.md`
`## Corrupt Input Policy`; fleet spawns require a nonzero global id).

## Design

In `ApplyOwnedEntityEvent`, replace the `contains` test, the add, and the later
`UpdateOwnedEntityCoordinate` call with one `std::ranges::find` on
`&engine::OwnedEntity::globalId`: when absent, keep the existing log line and
call `AddClientOwnedEntity(rEvent.globalId, rEvent.coordinate)`; otherwise
write `it->coord = rEvent.coordinate`. Keep the `kAssigned`
`UpdateDesiredCoordinates` call and everything after it unchanged. Delete
`UpdateOwnedEntityCoordinate` and its declaration.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.h`

## In scope

- `ClientSession::ApplyOwnedEntityEvent`: the assigned-entity lookup, add, and
  coordinate write.
- `ClientSession::UpdateOwnedEntityCoordinate`: delete the definition and
  declaration.

## Out of scope

- The `kDied` branch, both `UpdateDesiredCoordinates` calls, and the pending
  focus and client grid coordinate handling.
- `Game::AddClientOwnedEntity`.
- `Documents/Architecture/GameReconciliation.md`, whose "first add ... when it
  is absent, then update its coordinate" stays true in outcome.

## Acceptance criteria

- Clean `/compile` build of the BrokenEngineSandbox client.

## Notes

- Change Workflow tier: Tier 1. Trigger: local behavior-preserving change
  inside `ClientSession.{h,cpp}`; the removed member is private with one
  caller.
- Client-only bookkeeping; no CRC, wire, save, or replay exposure.
