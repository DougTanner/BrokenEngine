<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T21:45:13.849Z","dependsOn":[]} -->
# Cleanup: remove the Game and FleetSelection accessor and pass-through methods that rule 49 forbids

## Context
`Documents/C++StyleGuide.txt` rule 49 forbids getter, setter, `Can*`, and
other accessor methods for independently readable or mutable state, and any
function whose implementation is only one state access, one assignment, or
one pass-through call. The `Projects/` hand-read style sweep found these in `Projects/BrokenEngineSandbox/Source/Game.h` and routed
them here instead of fixing them, because the fix removes public methods and
changes callers outside the batch, including one in `Engine/`. That is
outside the sweep's meaning-preserving auto-fix bound.

Line numbers below are as of the commit that adds this Plan (the landing
commit of the `Projects/` hand-read style sweep).

Sites:
- `Game.h:104` `PreviousClientArmor()` and `Game.h:188`
  `SetPreviousClientArmor()` — getter and setter over the private
  `mfPreviousClientArmor` (`Game.h:181`).
- `Game.h:187` `PlayerAlignment()` — getter over the private
  `mPlayerAlignment` (`Game.h:182`).
- `Game.h:189` `Alignments()` — getter over the private `mAlignments`
  (`Game.h:184`); it has no callers.
- `Game.h:108` / `Game.cpp:118-121` `PlayerCount()` — returns
  `std::ssize(mClientPlayerIds)` over the already-public `mClientPlayerIds`.
- `Game.h:114-123` — ten one-line pass-throughs to the already-public
  `mFleetSelection` (`FleetCount` through `SyncFleets`).

Removing the `Game` pass-throughs sends callers to `mFleetSelection`, which
has three accessors of its own over private state
(`Projects/BrokenEngineSandbox/Source/FleetSelection.cpp`):
`FleetCount()` (:28, returns `std::ssize(mClientFleets)`),
`FocusedFleetIndex()` (:33, returns `miFocusedFleetIndex`), and
`FocusedMemberGlobalId()` (:115, returns `mFocusedMemberGlobalId`). They have
the same root cause and the same callers, so this Plan includes them. The other
`FleetSelection` methods compute a result or change state with side effects
(`FocusNextFleet` calls `AutoSelectFirstAliveMember` and
`gpGame->CaptureClientStateIfChanged`; `CanFocus*` compare; `FocusedFleet`
bounds-checks), so rule 49 does not apply to them and they stay.

Callers at that commit (`git grep` of each method name):
- `Agent/AgentScene.cpp:95-97` (`FleetCount`, `FocusedFleetIndex`,
  `FocusedFleet`)
- `Agent/Commands/ServerSimulationFixtures.cpp:401,806` (`PlayerAlignment`)
- `Game.cpp:80,680` (`mFleetSelection.FocusedMemberGlobalId()`)
- `Network/Client/ClientReconciler.cpp:44,121` (`PreviousClientArmor`,
  `SetPreviousClientArmor`)
- `Network/Client/ClientSession.cpp:92` (`SyncFleets`), `:142`
  (`FocusedMemberGlobalId`), `:153` (`PlayerCount`), `:248`
  (`SetPreviousClientArmor`)
- `Save/GameSaveLoad.cpp:26` (`PreviousClientArmor`)
- `Ui/Screens/HudScreen.cpp:51,198,204,207,209,216,224,227,229,246,274,294`
  (fleet navigation)
- `Engine/Source/Network/Server/ServerTransferManager.cpp:151`
  (`game::gpGame->PlayerAlignment()`)

## Design
The author's recommendation, following rule 49's own remedy ("Put the member
or composed component in `public:` and perform the operation at the caller"):
1. `Game.h`: move `mfPreviousClientArmor`, `mPlayerAlignment`, and
   `mAlignments` from the `private:` block to `public:`, keeping their
   declaration order. `mEnemyAlignment` has no accessor and stays private.
   Delete `PreviousClientArmor`, `SetPreviousClientArmor`, `PlayerAlignment`,
   `Alignments`, and `PlayerCount` (declaration and the `Game.cpp`
   definition), and the ten fleet pass-throughs along with the
   `// Fleet navigation — delegated to mFleetSelection` comment and its now-empty
   `#if defined(BT_CLIENT)` block.
2. `FleetSelection.h/.cpp`: move `mClientFleets`, `miFocusedFleetIndex`, and
   `mFocusedMemberGlobalId` to `public:`; delete `FleetCount`,
   `FocusedFleetIndex`, and `FocusedMemberGlobalId`. Update the class comment
   that says Game "forwards its public fleet-navigation API".
3. Callers: read or assign the member directly (`gpGame->mPlayerAlignment`,
   `gpGame->mfPreviousClientArmor = ...`, `std::ssize(gpGame->mClientPlayerIds)`,
   `std::ssize(gpGame->mFleetSelection.mClientFleets)`,
   `gpGame->mFleetSelection.miFocusedFleetIndex`,
   `gpGame->mFleetSelection.mFocusedMemberGlobalId`), and call the remaining
   `FleetSelection` methods through `gpGame->mFleetSelection`. Inside
   `FleetSelection.cpp` and `Game.cpp`, replace calls to the deleted accessors
   with the member.

Rationale: every replacement reads or writes the same value the method did,
so the change keeps behavior identical. `Game` is not serialized, so the
access change moves no persisted layout.

## Critical files
- `Projects/BrokenEngineSandbox/Source/Game.h`
- `Projects/BrokenEngineSandbox/Source/Game.cpp`
- `Projects/BrokenEngineSandbox/Source/FleetSelection.h`
- `Projects/BrokenEngineSandbox/Source/FleetSelection.cpp`
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp`
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/AgentScene.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp`
- `Engine/Source/Network/Server/ServerTransferManager.cpp`

## In scope
- `game::Game`: the methods `PreviousClientArmor`, `SetPreviousClientArmor`,
  `PlayerAlignment`, `Alignments`, `PlayerCount`, `FleetCount`,
  `FocusedFleetIndex`, `FocusNextFleet`, `FocusPrevFleet`,
  `CanFocusNextFleet`, `CanFocusPrevFleet`, `FocusedFleet`,
  `SelectPlayerInFleet`, `FocusedMemberGlobalId`, `SyncFleets`, and the access
  section of `mfPreviousClientArmor`, `mPlayerAlignment`, `mAlignments`
- `game::FleetSelection`: the methods `FleetCount`, `FocusedFleetIndex`,
  `FocusedMemberGlobalId`, the access section of `mClientFleets`,
  `miFocusedFleetIndex`, `mFocusedMemberGlobalId`, and the class comment
- Every caller of those methods listed in `## Context`, in `Projects/` and at
  `ServerTransferManager.cpp:151`

## Out of scope
- Every other `Game` or `FleetSelection` method, including `ClientPlayerId`,
  `IsClientPlayer`, `ClientPlayerIndex`, `SetClientGridCoord`, and the
  `FleetSelection` methods named in `## Context` as not rule 49 sites
- `mEnemyAlignment`'s access
- Any other rule, any other file's accessors, and any behavior change
- `ReplayMeta`, `ConfirmedClientState`, and other serialized types' fields
  (their `fPreviousClientArmor` fields keep their names and layout)

## Risk tier and invariants
Tier 3 (invariant/integration): the trigger is a change spanning
independently owned subsystems (`.agents/references/risk-tiers.md`). The
public-method removal updates callers in the game's UI, network, save, and
agent subsystems, plus one `Engine/` server file. Runtime behavior stays unchanged: no
serialized, CRC-covered, wire, save, replay, or `.pack` layout or value
changes, and no simulation arithmetic changes.

## Acceptance criteria
- `git grep -nE "(PreviousClientArmor|PlayerAlignment|Alignments|PlayerCount|FleetCount|FocusedFleetIndex|FocusedMemberGlobalId)\(" -- "Projects/*.h" "Projects/*.cpp" "Engine/*.h" "Engine/*.cpp"`
  matches no declaration, definition, or call of the deleted methods; its
  only expected hit is the `"Alignments("` string literal in the
  `std::formatter<engine::Alignments>` specialization in
  `Engine/Source/Engine.h`
- `Game.h` has no one-line forwarding method to `mFleetSelection`
- `/compile` Client and Server Debug and Release builds pass

## Notes
Routed from the `Projects/` hand-read style sweep, batch B5 (`Source/` root
files), `Routed Findings`.
