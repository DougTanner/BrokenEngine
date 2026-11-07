# Squadron Orders for Fighters and Omnis

Fighters and Omnis have no player order, and the transport to send one to a whole squadron does not exist. This document designs that transport and lists candidate orders. This is an Investigation, not a Plan, because the user has not chosen which order or orders to add. It earns a Plan once the user decides (`## Design` item 1).

## Context

Paths are under `Projects/BrokenEngineSandbox/Source/` unless they start at the repository root.
Line numbers were read from the tree carrying per-fleet squadrons; locate by symbol if they moved.

This is a capability addition, not debt. The user asked for it in these words (the authoring
agent's record): "Orders ... which are buttons on the right UI panel will be applied at the group
level. The battleship group's orders will be the Cell navigation slider for now." and, on building
order transport with squadrons, "could also make a followup plan to implement with actual fleet
orders to add". "Squadron" is the established term; the user's "group" means it here. The user has
not named the orders, and when asked to choose deferred the choice: "I'll decide later". `## Design`
item 1 holds that open decision.

State today:

- A squadron is the pair `(eType, FleetMember::uiSquadron)` within one fleet (`Fleet.h`). It exists
  exactly while a member names it. No squadron record and no Frame column exist.
- The only player order is the Battleship squadron's Nav Delay slider, shown when the focused ship
  is the flagship (`Ui/Screens/HudScreen.cpp:373`). Its value lives in
  `Fleet::navigationDelaySeconds`, which already is that one-member squadron's order storage.
- Fighters and Omnis have no player order, and the right panel is empty for them
  (`HudScreen::RenderFocusedOwnedEntityPanel`). The left panel's `[x]` is a member action.
- `ShipOrders` holds `kIsFlagship`, `kTargetFlagship` (random-spawn rows only, `Frame/Frame.cpp:424`),
  and the target plane; bits 2-7 are free (`Frame/Collections/Ships/ShipTypes.h:54-60`).

The slider path is not the order path this work needs. It reaches no status change: the packet
writes `Fleet::navigationDelaySeconds` in `UpdateFleetNavigationDelay`
(`Network/Server/ServerFleetManager.cpp:406`), and the value is read only when the flagship's timer
next fires (`Network/Server/FleetNavigationController.cpp:83`).

The existing fan-out to members is that timer's `kUpdateFleet` loop
(`Network/Server/FleetNavigationController.cpp:92-124`), which pushes one status change into each
live member's own cell input. `kUpdateShip` is not reusable: its only producer is the harness
(`Agent/Commands/ServerSimulationHarnessRigs.cpp:409`), and every apply resets the row's
cell-change timer (`Frame/Collections/Ships/ShipStatusChanges.cpp:60-61`).

## Design

No option is chosen. Items below use option A's names and behaviour (`SquadronOrder` values,
`kHoldEscort`, button labels) only because A is the author's recommendation, so the mechanism reads
concretely; every name is the author's recommendation too. The mechanism in items 2-7 and 9 holds
for any single order apart from its names, with one exception: option D gives Omnis nothing, so the
Omni-specific parts of those items do not hold for it. Under D, item 2's Omni entries stay at the
default as the Battleship's do, item 3 refuses Omni as well, item 6 has no Omni stored order to read,
and item 9's branch draws for a Fighter only. Item 1 and the behaviour in item 8 depend on which
order or orders are chosen.

### 1. Open decision: which order or orders to add

The user decides, and has deferred: "I'll decide later". Two questions are open: which options to
take, and whether to take one order or several. Neither has an answer here, and the author's
recommendation below names one option without answering the second. The design is written for one
order: item 2's single `eOrder` cannot carry two independent orders, so choosing several also changes
what items 3-6 and 9 carry.

When the user chooses:

- Replace items 1 and 8, the `ShipOrders` bit, and the matching lines of `## Critical files`,
  `## In scope`, `## Out of scope`, `## Risk triggers and invariants`, `## Acceptance criteria`, and
  `## Notes`. The `## Out of scope` line naming `ChooseFleetTactic`, `FleetBoard`, `FleetStance`, and
  weapon fire fits only option A: option B gates weapon fire, option C changes `FleetBoard`, and
  option D adds a fleet branch that line may also exclude.
- Unless the choice is A, replace A's names in items 2-7 and 9. Under D, also make the changes to
  items 2, 3, 6, and 9 that the `## Design` preamble lists, and drop Omnis from the `# Title`.
- If the choice is several orders, change items 2-6 and 9 to carry them.
- Remove the text that says nothing is chosen: the opening paragraph's candidate-orders and
  Investigation sentences, the `## Context` sentences on the deferred choice, the `## Design`
  preamble, and each "Written for option A" line.
- Re-run plan review on what changed. The document then moves to `Documents/Plans/Game/` with its
  metadata line (`Documents/Investigations/AGENTS.md`).

| Option | Effect | Cost |
| --- | --- | --- |
| A. Engage / Hold Escort, both types | A held squadron ignores weapons free: Fighters stay on their escort slots, Omnis keep patrolling. Both still fire | One row bit; one stance override per type |
| B. Weapons Free / Hold Fire, both types | Rows steer as today and do not shoot | One row bit; separate fire gates in the Fighter beam and Omni blaster paths |
| C. Normal / Aggressive engage radius | A squadron joins a fight farther from the flagship | One row bit; `FleetBoard` computes one stance per flagship, so it needs a second stance per view |
| D. Fighter Attack Flagship | Fighters chase the enemy flagship | New fleet branch; `kTargetFlagship` drives random-spawn rows only. Gives Omnis nothing |

Author's recommendation, not a decision: A. It has an exact precedent: a healing Omni overrides its
copy of the view to `FleetStance::kEscort` before `ChooseFleetTactic`
(`Frame/Collections/Omnis/Omnis.cpp:633-637`). Row fire does not read the tactic, so a held squadron
still defends itself. With at most two squadrons per type, one can stay home while the other fights.
One order serves both types, so the right panel needs one button pair.

Under option A, `SquadronOrder::kEngage` is the default and leaves behaviour as it is today.

The user also offered an idea, recorded here unevaluated and not as a decision: "one idea is for the
fighters to use formations when on the top layer". It has no table row because no one has assessed
its effect, its cost, or whether it is an order at all. Facts found in code:

- If "top layer" means the top flight plane, that is the last `kFlightPlanes` entry
  (`Frame/FlightPlaneTable.h`), which is `kiFighterHighestFlightPlane`
  (`Frame/Collections/Fighters/Fighters.h:39-40`). `DefaultFlightPlane` returns it for a Fighter
  (`Frame/Collections/Ships/ShipBehaviours.cpp:51-54`), so an escort tick already targets it
  (`Frame/Collections/Ships/FleetCoordination.cpp:212-217`).
- Fleet Fighters have one placement today, the escort slots: `EscortSlotPoint`
  (`Frame/Collections/Ships/FleetCoordination.cpp:94-124`) spaces the fleet's non-dying Fighters
  evenly on one circle around the flagship, reading neither squadron nor plane.
- The word "formation" appears nowhere in the code or Markdown under `Projects/BrokenEngineSandbox/`.
  `## Out of scope` here excludes "squadron-aware escort or formation layout", so taking up the idea
  changes that line too.

### 2. Stored order

The first order needs storage, because a respawn must inherit it and the HUD must show it. The
author recommends a fixed `std::array<FleetSquadron, N> Fleet::squadrons`, with N being
`kiMaximumFleetSquadronsPerType` times the `ShipType` count, indexed by the pair through one inline
function in `Fleet.h`. `FleetSquadron` holds `SquadronOrder eOrder`, declared in `ShipTypes.h` as a
`uint8_t` enum because the status change also carries it. A fixed array keeps a squadron defined by
its members, needs no create or find step, and gives FleetSync and the save a fixed size. A growing
list would add both steps.

The Battleship's entries stay `kEngage`: the request refuses that type, and the slider stays its
order. When `ProcessDestroyFleetMemberRequests` erases a squadron's last member, the entry resets to
`kEngage`, so a later squadron with that number starts at the default.

`WriteFleet` and `ReadFleet` carry the array, one byte per entry. FleetSync carries it in
`VisitFleetHeader`, with `kiFleetHeaderSize`, its `static_assert`, and the `BoundedCount` trailing
size updated. Replay fleet records and the fleet checksum inherit it through `WriteFleetData`.

### 3. Request

One new `GamePacketType`, appended: `kClientSquadronOrderRequest`, with a
`GameMessages::SquadronOrderRequestMessage` carrying fleet GUID, ship type, squadron, and order
through one `Visit` and a `static_assert`ed `kiSize`, and a contract row sized from that `kiSize`
with `iMaximumPerTick` 8, like its neighbours (`Network/GamePacketType.h`).

`ServerSession::HandleGamePacket` throws through `ThrowCorruptStream` on a ship type outside
Fighter and Omni, a squadron at or above `kiMaximumFleetSquadronsPerType`, or an order at or above
`SquadronOrder::kCount`. It queues the rest in a new pending list beside
`mPendingDestroyFleetMemberRequests`, with the same clears at `ServerFleetManager.cpp:260`, `:316`,
and `:432`. Playback already refuses the packet at the top of that function.

The queue drains in `ServerSession::BuildAdvancingFrameInputs`, after destroy requests, because a
frame-affecting order needs that tick's inputs. Paused updates retain it, as they retain destroys.
Draining after destroys makes a same-tick order for a just-emptied squadron meet the refusal below.

### 4. Fan-out

`ProcessSquadronOrderRequests` looks the fleet up under the sender's own client GUID, as the destroy
path does, and ignores an unknown fleet, a squadron with no members, or an order equal to the stored
one. It stores the order, walks the fleet's members with that pair, and copies the
`kUpdateFleet` loop: skip a dead member, find its `mFrameInputs` entry, read its uuid with
`ShipUuidByGlobalId`, and push the status change. It then sends the fleet sync. Each member's own
cell input takes its entry, so cells need no coordination, and client and replay inherit it through
`FrameInput`.

### 5. Row payload

One new appended `StatusChangeType::kUpdateSquadronOrder` with `UpdateSquadronOrderData`
(`eType`, `iUuid`, `eOrder`) and its `Fields()`, `DefaultDataForType` arm, name, and variant arm.
`ApplyUpdateStatusChanges` sets or clears `ShipOrders::kHoldEscort` (bit 2) through
`VisitShipByUuid`, ignoring a row it cannot find as `kUpdateFleet` does. `IsAdoptableStatusChange`
also checks the order range. The bit lives in the existing `pOrders` column, so no column changes,
and `SetTargetFlightPlane` touches only bits 8-15.

`ServerSimulationHarnessRigs.cpp`'s status-change switch ends in a `default` arm that assumes
`UpdateFleetData`, so the new type needs its own `case`, or `inject_payload` would reach
`std::get` on the wrong alternative.

### 6. Dead and respawning members

The fan-out skips dead members. `SpawnFleetMember` sets `kHoldEscort` on `spawnOrders`
(`Network/Server/ServerClientManager.cpp:183-188`) from the squadron's stored order, which reaches
every ship type through `QueueFleetSpawn`, so no `SpawnInfo` field is added. Requests drain before
the fleet step and a queued spawn becomes a row in that same tick's post-tick batch, so a member
spawning on the order's own tick reads the new value, and a later order finds its row.

### 7. Members in transit

No special case. Updates apply in Update before the Transfer phase, the transfer payload copies
`pOrders` afterward (`Frame/Collections/Fighters/Fighters.cpp:379`), and the member's coordinate
moves during that harvest, so a row is in exactly one cell at each `PrepareTick`.

### 8. Behaviour

This item is option A's behaviour. In a fleet row's Update, when the previous frame's orders hold
`kHoldEscort`, override the view's stance to `FleetStance::kEscort` before the tactic call. Omnis
extend the existing `kHealing` test. Fighters stop passing `fleetBoard.Find(...)` inline and hold a
local view the same way (`Frame/Collections/Fighters/Fighters.cpp:621`). `ChooseFleetTactic` itself
does not change.

Escort slots stay as they are: `EscortSlotPoint` counts all the fleet's non-dying Fighters in the
cell, so a held squadron flying beside an engaged one keeps gaps in its circle.

### 9. Right panel

`RenderFocusedOwnedEntityPanel` keeps its flagship branch unchanged. A new branch finds the focused
member by `ClientOwnedEntityIdentifier()`, and for a Fighter or Omni draws the squadron's header
text, shared with the left panel's box header, then `Engage` and `Hold Escort` buttons. The current
order's button is disabled, and both are disabled while the request is pending or
`IsServerReplaying()`. Pending state is one `engine::NetworkUiControl<SquadronOrderKey>` on
`FleetSelection`, keyed like `NavigationDelayKey` by fleet GUID, type, squadron, and stored order,
and reset beside the other controls (`FleetSelection.cpp:276-278`). A press applies to the focused
ship's whole squadron.

## Critical files

Written for option A, the author's recommendation; `## Design` item 1 says what changes with the
user's choice.

- `Fleet.h`, `Frame/Collections/Ships/ShipTypes.h`, `Frame/StatusChange.h`, `Frame/StatusChange.cpp`
- `Frame/Collections/Ships/ShipStatusChanges.cpp`
- `Frame/Collections/Fighters/Fighters.cpp`, `Frame/Collections/Omnis/Omnis.cpp`
- `Network/GamePacketType.h`, `Network/GameMessages.h`
- `Network/Client/ClientSession.h`, `Network/Client/ClientSession.cpp`
- `Network/Server/ServerSession.cpp`, `Network/Server/ServerFleetManager.h`,
  `Network/Server/ServerFleetManager.cpp`, `Network/Server/ServerFleetSerialization.cpp`,
  `Network/Server/ServerClientManager.cpp`
- `FleetSelection.h`, `FleetSelection.cpp`, `Ui/Screens/HudScreen.cpp`
- `Agent/AgentScene.cpp`, `Agent/Commands/ServerSimulationHarnessRigs.cpp`

## In scope

Written for option A, the author's recommendation; `## Design` item 1 says what changes with the
user's choice.

- `SquadronOrder` and `ShipOrders::kHoldEscort` in `ShipTypes.h`.
- `FleetSquadron`, `Fleet::squadrons`, and its index function in `Fleet.h`.
- `WriteFleet`, `ReadFleet`, and FleetSync carrying `Fleet::squadrons`.
- `ReadFleet` validation beside the existing checks after the member loop, throwing
  `std::ios_base::failure`: an order at or above `SquadronOrder::kCount`, and a non-default order on
  a pair with no member or of type Battleship.
- `kClientSquadronOrderRequest`, its message, contract row, `ClientSession` send,
  `HandleGamePacket` case, pending list, and `ProcessSquadronOrderRequests`.
- The reset in `ProcessDestroyFleetMemberRequests` and the `spawnOrders` read in `SpawnFleetMember`.
- `StatusChangeType::kUpdateSquadronOrder`, its payload, apply, adoption check, and harness rig
  `case`.
- The stance override in `FightersPostRender` and `OmnisPostRender` Update.
- The right-panel branch and `FleetSelection` control.
- `kHoldEscort` in `ShipFlagNames` (`Agent/AgentScene.cpp:41`).
- Version bumps: `engine::kiProtocolVersion`, `FrameInput::kiVersion`, the `Frame::kiVersion` base,
  and `ReplayMeta::kiVersion`.
- Docs, through `/update-claude-docs`: `Frame/AGENTS.md` status-change consumption;
  `Frame/Collections/Ships/AGENTS.md` `## Fleet Coordination Invariants`,
  `Frame/Collections/Fighters/AGENTS.md`, and `Frame/Collections/Omnis/AGENTS.md`;
  `Network/Server/AGENTS.md`; `Ui/Screens/AGENTS.md`; the contract table and version note in
  `Documents/Architecture/Network.md`; and the orders bits and `inject_payload` table in
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`.

## Out of scope

Written for option A, the author's recommendation; `## Design` item 1 says what changes with the
user's choice.

- The Battleship squadron's order: the Nav Delay slider, its packet, apply timing, flagship gate,
  and `Fleet::navigationDelaySeconds` do not change.
- Orders beyond those the user chooses from `## Design` item 1, and any per-ship order.
- User control of squadron membership, and squadron-aware escort or formation layout.
- `ChooseFleetTactic`, `FleetBoard`, `FleetStance`, and weapon fire.
- A new Frame collection or column, and any `SpawnInfo` field.
- Removing `kUpdateShip` or `pNavigationDelays`.
- Random-spawn rows and enemy waves.
- Cross-checking the save's `Fleet::squadrons` against frame `pOrders`: a disagreement is one
  behaviour bit on a fleet row until the next order, and neither reader trusts the other for
  safety.
- New harness commands, unit tests, and localization of the new labels.

## Risk triggers and invariants

Written for option A, the author's recommendation; `## Design` item 1 says what changes with the
user's choice.

- Tier 3. Triggers: wire/protocol (new packet, FleetSync header, new status change), save and replay
  format (`WriteFleet`), determinism/CRC (a new CRC-covered `pOrders` bit changes simulation), and
  the trust boundary (a client-to-server record and new grid-save fields).
- All four versions in `## In scope` move. `/add-collection-member` is not triggered, because no
  column is added.
- An order reaches a live row through `FrameInput` and a respawned row through `SpawnInfo.orders` in
  the post-tick batch, so client, server, and replay stay bit-identical. `Fleet::squadrons` is
  server-owned and client-displayed, and stays outside the Frame CRC.
- A squadron exists exactly while a member names it. An empty pair holds `kEngage`, and a Battleship
  pair always does.
- The client's request is validated by value before queuing and by ownership at the drain. A bad
  value answers under `Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy`.
- A run that issues no order simulates as it did before, apart from the version change.
- The drain's `mFrameInputs` growth and the sync run under `ScopedSuppressAllocationTracking`, as
  `ProcessDestroyFleetMemberRequests` does.

## Acceptance criteria

Written for option A, the author's recommendation; `## Design` item 1 says what changes with the
user's choice.

The supplied evidence records no harness run or setup: this document's source was a code read, so
the harness checks below are derived from code and have not been run. Recipes:
`/agent-harness`, `Projects/BrokenEngineSandbox/Documents/AgentHarness/`.

1. `/compile` builds the BrokenEngineSandbox client and server.
2. Harness: with ten Fighters and three Omnis, focus a squadron 0 Fighter and `click` `Hold Escort`.
   `query_collection` shows the `kHoldEscort` bit in `orders` on exactly that squadron's rows, and
   `describe_ui` reports `Hold Escort` disabled and `Engage` enabled. Focusing a squadron 1 Fighter
   reports the reverse. `click` `Engage` clears the bit on those rows.
3. Harness: hold one Fighter squadron and one Omni squadron, then `inject_payload` an enemy ship
   within `kfWeaponsFreeDistanceSquared` of the flagship. Held rows keep their escort or patrol
   destinations and never set `kVeerOff`, while the unheld squadrons attack.
4. Harness: `edit_frame` removes a live held Fighter row; the same global ID respawns with the bit.
5. Harness: on a fleet with three Fighters, all in squadron 0, hold that squadron, destroy all three,
   and add one Fighter. No non-empty Fighter squadron remains, so `FindOpenFleetSquadron` returns the
   empty number 0, and the new member starts at `kEngage`. A second non-empty Fighter squadron with
   room would take the add instead, which is why it must be absent.
6. Harness: `save`, change the orders, `load`; rows and `describe_ui` show the saved orders.
7. Harness: run the replay determinism recipe in
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md` over a recording that changes an
   order across a cell move; no frame CRC or fleet checksum mismatch.
8. Harness: `send_raw_packet` with `kClientSquadronOrderRequest` bytes naming a Battleship, an
   out-of-range squadron, or an out-of-range order takes the corrupt-packet path and changes
   nothing; one naming another client's fleet or an empty squadron changes nothing.
9. Harness: with the flagship focused, `describe_ui` lists `Nav Delay` and no order button; with a
   Fighter focused, it lists the order buttons and no `Nav Delay`.
10. Code read: `ReadFleet` rejects the values `## In scope` names, and `kEngage` rows take the same
    tactic path as before.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

This is one stage: the transport has no order to exercise it until the behaviour lands, so no
earlier cut can pass acceptance.

The Plan below touches nearby lines. Either order works, and the later landing refreshes against the
tree it finds and takes its own `Frame::kiVersion` bump from the value it finds:

- `Documents/Plans/Game/ShipCombatAndFleetCleanups.md` edits `HudScreen.cpp` and
  `ServerFleetManager.cpp`. Its G02 may add a message-taking send: use it if it has landed, else
  follow `SendDestroyFleetMemberRequest`.
