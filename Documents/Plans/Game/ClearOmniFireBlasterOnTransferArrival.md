<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:49:40.971Z","dependsOn":[]} -->
# Clear an Omni's kFireBlaster flag on transfer arrival

## Context

Line numbers are from commit `629ced2e`; locate by symbol if they moved. Paths
without a root are under
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`. Nothing was built or
run for this Plan: every statement is from a code read.

An Omni that crosses a cell edge on a tick it was told to fire carries the
fire request into the destination cell, where it can fire one bolt at
nothing. The trace:

- PostRender Update sets the request. `AcquireTarget` sets
  `ShipFlags::kFireBlaster` when an enemy is in range
  (`Ships/ShipQueries.cpp:231-234`) and never clears it; the Omni Update is
  its only caller (`Omnis/Omnis.cpp:708`).
- The Spawn phase consumes it. `OmnisPostRender::SpawnBlasters` is the only
  simulation reader and the only clear (`Omnis/OmnisCombat.cpp:222-226`).
  The one other reader is the agent scene query, which only names the set
  bit in its output
  (`Projects/BrokenEngineSandbox/Source/Agent/AgentScene.cpp:81`).
- Transfer runs before Destroy and Spawn
  (`Engine/Source/Frame/FrameBase.cpp:334-340`). A row Update flagged to
  fire and PostCollision then marked for transfer leaves the cell before
  `SpawnBlasters` sees it, and the Transfer literal copies the flags
  unmasked: `.flags = rCurrentPostRender.pFlags[i]`
  (`Omnis/OmnisNavigation.cpp:38`).
- The arrival keeps the bit. `OmnisPostRender::Spawn(Frame&, const
  SpawnInfo&)` copies `rInfo.flags` and changes only the navigation bits
  and `kNoAiDirection` (`Omnis/Omnis.cpp:317-321`).
- The first tick in the destination is transfer-locked, so Update skips
  `AcquireTarget` (`Omnis/Omnis.cpp:629-632`) and copies the flags through
  (`:589`, `:771`). `SpawnBlasters` then consumes the carried bit: it steps
  `pNextBlasterFireTimes` once (`Omnis/OmnisCombat.cpp:243`) and, when the
  timer crosses zero (`:245`), spawns one bolt flat along the carried hull
  heading (`:239`, `:257-265`).

That bolt aims at nothing in the destination cell: its target test was made
one tick earlier in the source cell. `BlastersPostRender::Spawn` refuses a
muzzle outside the cell (`Blasters/Blasters.cpp:92-97`), the client muzzle
sound plays only when the bolt spawned (`Omnis/OmnisCombat.cpp:273-278`),
and the timer advances either way (`:280`). A dying or healing row fires
nothing (`:229-237`). The bit is cleared after that one tick. Client and
server carry the same bit, so this is not a desync.

Scope evidence only: the bit exists on Omnis alone. `AcquireTarget` has no
other caller, and the Fighters and Battleships Transfer literals
(`Fighters/Fighters.cpp:373`, `Battleships/Battleships.cpp:252`) name no
`.flags` member.

The documents disagreed on intent, and neither was trusted over the other
by the analysis:

- `AGENTS.md` (the one in that directory) `## Game-Specific Rules` says a
  transfer arrival "restores carried gameplay state verbatim" except a
  closed list that does not name this bit. That supports the code.
- `Omnis/AGENTS.md` `## Behavior Invariants` says Omnis fire only at a
  visible target in range and in line of sight. That reads against a shot
  with no target test in the destination cell.

The authoring agent's transcript of the exchange in the authoring session
(2026-11-14), for an execution card or dispatch brief to quote. The question
asked of the user: "An Omni that decided to fire and then crossed into a
neighbouring cell in the same tick carries its 'fire a blaster' flag, and can
fire one untargeted bolt straight ahead on its first tick in the new cell (no
desync; harmless). How should I record this?" Three options were offered.
"Option 1: Document it (Recommended)" was presented as adding one sentence to
the Omni rules saying the fire flag carries across a transfer, with no code
change. "Option 2: Clear on arrival" was presented as clearing the flag when
an Omni arrives in a cell, so no untargeted bolt is fired, and as changing
simulation results, being the highest risk tier, and taking a version bump
that invalidates existing saves and replays. The third was "Option 3: Record
nothing". The user's reply, verbatim: "Option 2: Clear on arrival". When this
binds is in `.agents/references/authority-order.md`.

Originating gap: the architecture phase (simulation and threading lens) of
a findings-only deep analysis of that directory (recursive), confirmed by
the in-function phase and an independent verification read. Pre-existing
debt, not an unmet acceptance criterion of any change. Unmet condition: an
Omni fires only on a target test made in the cell it is in.

## Design

Clear `ShipFlags::kFireBlaster` for an Omni that arrives by transfer, so the
arrival's first `SpawnBlasters` call skips the row without stepping its fire
timer.

Where to clear is left to the executing session, between two sites:

- the Transfer literal in `OmnisNavigation.cpp` (`:38`), masking the bit
  out of the payload; or
- `OmnisPostRender::Spawn(Frame&, const SpawnInfo&)`, beside the other flag
  resets (`Omnis/Omnis.cpp:317-321`).

`AGENTS.md` `## Game-Specific Rules` requires a choice between transferred
and fresh values to select on the explicit transfer marker
(`rInfo.bTransfer`), never on a carried value. The author's recommendation
is the arrival site, because the other arrival flag resets already live
there and Fighters clear `kVeerOff` on arrival the same way
(`Fighters/AGENTS.md` `## Behavior Invariants`). A genuine spawn passes no
set bit, so clearing it there for every row and clearing it only under the
marker give the same result; the executing session picks the simpler form
that satisfies the rule.

Bump the base of `Frame::kiVersion`
(`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:116`).
`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` `## Invariants`
requires it when CRC semantics change without a contributing version bump,
and no collection layout changes here. Older saves and replays are then
rejected; no compatibility path is added. `FrameInput::kiVersion` and
`engine::kiProtocolVersion` do not change: no byte written or sent changes
shape.

Update the two documents so they match the code:

- `AGENTS.md` `## Game-Specific Rules`: add the Omni fire request to the
  exception list of the transfer-arrival bullet.
- `Omnis/AGENTS.md`: one sentence that the fire request does not carry
  across a transfer, in the section the executing session judges it
  belongs to.

Rewrap only the edited paragraphs.

Risk tier: Tier 3 (the author's classification). Trigger: determinism/CRC
and save/replay compatibility. The change alters CRC'd simulation state
(`pFlags`, `pNextBlasterFireTimes`, the Blasters rows, and the random
stream, since a bolt draws its angle jitter from
`rFrame.postRender.randomEngine`). No wire, data-layout, threading,
allocation, shader, or build exposure.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/OmnisNavigation.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/AGENTS.md`

## In scope

- One of: the `.flags` member of the Transfer `SpawnInfo` literal in
  `OmnisNavigation.cpp`, or the `spawnFlags` block of
  `OmnisPostRender::Spawn(Frame&, const SpawnInfo&)`. Not both.
- The base literal of `Frame::kiVersion` in `Frame.cpp`.
- The transfer-arrival bullet of `AGENTS.md` `## Game-Specific Rules`: one
  added exception.
- One sentence in `Omnis/AGENTS.md`.
- A comment at the changed site only where rule 64 of
  `Documents/C++StyleGuide.txt` calls for one.

## Out of scope

- `AcquireTarget`, `SpawnBlasters`, and the phase order in `FrameBase.cpp`.
- Any other carried flag, including `kHealing` and `kBlasterSpawnLeft`,
  and the carried `pNextBlasterFireTimes` value.
- Holding fire for the rest of the transfer lock or under arrival grace:
  after the first tick no request is set, so nothing more is needed.
- Fighters and Battleships, which carry no such bit.
- Any compatibility path for older saves or replays.
- Other clauses of the exception list.

## Acceptance criteria

1. An Omni row added by a transfer arrival has `ShipFlags::kFireBlaster`
   clear; the diff settles this.
2. A genuine Omni spawn is unchanged; the diff settles this.
3. `Frame::kiVersion`'s base is higher than at the baseline; the diff
   settles this.
4. Both documents state that the fire request does not carry across a
   transfer.
5. The client and server build, and a `/agent-harness` replay determinism
   check over a run in which Omnis transfer between cells while firing
   reports no CRC mismatch. The supplied evidence records no harness setup
   that reproduces the state: nobody ran the game for the finding.

## Notes

- The harness cannot easily show the absence of one untargeted bolt, so
  criterion 1 rests on the diff and criterion 5 on determinism.

## Coordination

No directional dependencies or mandatory nondirectional coordination. The
exception list of the transfer-arrival bullet in
`Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md`
`## Game-Specific Rules` already names the Omni transfer lock and every ship
type's cell-change timer; this Plan adds its one clause and keeps those.
