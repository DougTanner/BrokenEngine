<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:39:03.362Z","dependsOn":[]} -->
# Give the navigation-delay range one code owner

## Context

Paths are under `Projects/BrokenEngineSandbox/Source/` unless they start at
the repository root; `FC/` stands for `Frame/Collections/`. Line numbers are
from the commit that added this Plan; locate each site by symbol.

Originating gap: the architecture phase of a deep analysis of `Ui` (recursive),
verified by a second reader. It is not an unmet acceptance criterion of any
change. The false condition, from root `AGENTS.md` `## Directives` (DRY): the
admitted navigation-delay range has one code owner that every reader uses.

Verified root cause: the range bounds are written as literals in two places
tied only by comments.

- `IsNavigationDelayInRange` (`FC/Ships/ShipTypes.h:182-187`) tests the delay
  against two float literals; its comment says the range matches the HUD
  slider.
- The HUD Nav Delay slider (`ImGui::SliderFloat("Nav Delay", ...)`,
  `Ui/Screens/HudScreen.cpp:394`) passes the same two literals as its bounds;
  the comment above it (`:392-393`) refers to the server's rejection.

Every other check of the range calls the predicate
(`Network/Server/ServerSession.cpp:36`,
`Network/Server/ServerFleetSerialization.cpp:132`, `FC/Fighters/Fighters.h:217`,
`FC/Omnis/Omnis.h:240`, `FC/Battleships/Battleships.h:210`,
`Agent/Commands/ServerSimulationHarnessRigs.cpp:284` and `:417`), so the slider
is the only second code copy.

What drift would cause: a slider zone whose release silently reverts, or an
admitted range the slider cannot reach. It cannot make the slider send a value
the server rejects: the release handler (`HudScreen.cpp:398-401`) reverts any
value failing the predicate before the send.

## Design

The author's recommendation: declare the two bounds as named constants beside
`IsNavigationDelayInRange` in `FC/Ships/ShipTypes.h`, have the predicate test
against them, drop the number from its comment, and pass the same constants to
the slider. `ShipTypes.h` already reaches `HudScreen.cpp` through `Game.h` and
`Fleet.h`, so no include changes. The predicate keeps its name, signature, and
the single range test that also fails NaN and both infinities.

The five navigation-delay defaults written as a bare literal equal to the
maximum (`Fleet.h:120`, `Frame/StatusChange.h:107`,
`FC/Fighters/Fighters.h:319`, `FC/Omnis/Omnis.h:376`,
`FC/Battleships/Battleships.h:307`) and the comment at
`Network/Server/ServerSession.cpp:32`, which repeats the range, stay as they
are. A default is a different concept from the admitted range, and the minimum
sufficient change names only the two bounds the predicate and the slider use.

Invariants: the admitted range is value-identical before and after, so server
admission, grid-save validation, each `PostRead`, and the harness rig accept
and reject exactly the same values; no CRC, wire, or serialized byte changes
and no version is bumped.

Risk tier: Tier 2 — scoped behavior. Trigger: the edit is inside the predicate
the server's client-record and grid-save validation and the ship `PostRead`
checks share, with new shared constants in a simulation header; it stays Tier 2
only because the check and what it admits are unchanged.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipTypes.h`
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp`

## In scope

- `IsNavigationDelayInRange`, its comment, and two new bound constants beside
  it in `FC/Ships/ShipTypes.h`.
- The two bound arguments of the Nav Delay `ImGui::SliderFloat` call in
  `HudScreen.cpp`.
- Any `AGENTS.md` sentence `/update-claude-docs` finds the change makes false.

## Out of scope

- The range's values, and the predicate's name, signature, and callers.
- The slider's release handling, its function-static edit state, and the
  navigation request.
- The five navigation-delay default initialisers and the `ServerSession.cpp`
  comment `## Design` names: a default is not the admitted range.
- `pNavigationDelays`, `SpawnInfo::navigationDelay`, and every other part of
  `Documents/Plans/Game/RemoveShipNavigationDelayColumn.md`.

## Acceptance criteria

1. Code read: the two bounds appear as literals once each, at the named
   constants; `IsNavigationDelayInRange` and the Nav Delay slider both read
   those constants; the predicate still rejects NaN and both infinities.
2. `/compile` builds the BrokenEngineSandbox client and server, and the
   repository static checks pass.

## Notes

- `FC/Ships/AGENTS.md` `## Ship Invariants` and `Ui/Screens/AGENTS.md`
  `## Shared Contracts` describe the range in prose; `/update-claude-docs` in
  the executing session decides whether either changes.
- Other Plans cite `HudScreen.cpp` by line number, which this edit can shift:
  `Documents/Plans/Game/FleetOwnedSquadronState.md`,
  `IncludeShipGlobalIdsInCrc.md`, `RemoveClientStateFromReplayMeta.md`, and
  `RemoveShipNavigationDelayColumn.md`. Each is located by symbol.

## Coordination

`Documents/Plans/Game/RemoveShipNavigationDelayColumn.md` keeps
`IsNavigationDelayInRange` by name, removes its three `PostRead` callers and
the spawn-payload check in `ServerSimulationHarnessRigs.cpp`, and deletes
`SpawnInfo::navigationDelay`, which holds three of the five defaults this Plan
leaves alone. Neither Plan depends on the other. This Plan keeps the
predicate's name and signature. Whichever lands second re-derives its sites
from the landed tree: here, the predicate's remaining callers.
