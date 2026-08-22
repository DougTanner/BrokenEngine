# Teleport Movement Override

## Context

User-requested capability: a movement override that relocates an **existing** entity (the
player at minimum) to a chosen point, without that displacement being interpreted as
ordinary swept travel. Two motivating uses: future ship teleport abilities, and AgentHarness
tests that need an entity placed at a chosen point instead of being flown there.

The engine has no such override today. Chosen-position *spawning* already exists via
`SpawnPlayerData`'s offsets (added by `Documents/Plans/Agents/HarnessPlacementAndPusherOverflowObservability.md`,
landed separately); this feature is only about moving an entity that already exists.

### The collision-sweep hazard a naive teleport hits (verified at 774a1de)

Writing a new position straight into the player's position array is not enough:

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:700-715` builds the
  player collision layer with `pVecStartPositions` = the **previous** frame's positions and
  `pVecEndPositions` = the current ones.
- `Engine/Source/Frame/Collision.cpp:156-161` builds swept AABBs from that start/end pair, and
  `:588-594` runs `SweptSphereTest` across the whole segment.
- Blasters (`Collections/Blasters/BlastersUpdate.cpp:276`) and missiles
  (`Collections/Missiles/MissilesUpdate.cpp:356`) are the only layers setting `bSweptTest = true`.

So a player moved a long distance inside a tick is swept against every blaster and missile
lying along the jump path and collects hits it never flew through. The previous-frame start
array is const shared history owned by the previous frame and cannot be rewritten to hide the
jump.

### The consumption slot that neutralises it (verified at 774a1de)

Phase 5, `game::FramePostRender::Spawn` (`Engine/Source/Frame/FrameBase.cpp:348-361`), runs
**after** Collision and **before** the shared CRC stamp at `:361`, and receives the same
published `FrameInput` on client and server. `ProcessSpawnStatusChanges`
(`Collections/Players/Players.cpp:241`) already performs uuid to index lookups in that phase.
Applying the relocation there means the tick's collision work has already completed against
the pre-teleport position, and the new position is still inside the CRC.

## Design (author's recommendation, not a settled decision)

Recommended shape: a new append-only `StatusChangeType` (for example `kTeleportPlayer`) with a
payload of `{ int64_t iPlayerUuid; }` plus the target position, applied in the Phase 5
Spawn handler next to `ProcessSpawnStatusChanges`. Rationale: it reuses the existing
uuid to index lookup and the already-proven post-Collision/pre-CRC window, and it keeps the
override out of the Update and Collision phases entirely.

Deliberately left open for the implementing session to decide with the user:

- Whether the payload carries an absolute world position or a cell-relative offset, and how it
  is expressed (the existing spawn path uses offsets).
- Whether the override applies to players only, or is generalised to other collections. Players
  only is the smaller change and satisfies both stated uses.
- What happens to velocity and orientation across the jump (preserved, zeroed, or specified in
  the payload).
- Whether a future ship *ability* drives this same status change or a separate simulation-side
  path; only the override mechanism is in scope here.

### Contract sites any new status-change type must touch

- `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h` — append the enumerator (never
  reorder or insert; the underlying values are wire/save bytes), add the payload struct, extend
  `StatusChangeTypeName`, and add the explicit `DefaultDataForType` case. `DefaultDataForType`
  has a `default:` arm returning `TransferData{}`, so a forgotten case fails silently rather
  than failing to compile — this must be checked by hand.
- `Engine/Source/Network/NetworkSerialization.cpp` — per-item wire size and both codec switches
  (the file's own comment at `:147` names the set of sites that move together).
- `FrameInput::kiVersion` bump — required for any `StatusChange` payload change
  (`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`, `FrameInput` serialization rule).
- `engine::kuiProtocolVersion` bump — required for any incompatible `StatusChange` layout
  change (`Projects/BrokenEngineSandbox/Source/Network/AGENTS.md`).

### Out-of-cell targets

A target point outside the requesting entity's own cell is **refused**, not clamped, matching
the cell-ownership invariant in `Engine/Source/Frame/AGENTS.md`: an entity's position must lie
inside its own cell's bounds, which is why an out-of-bounds entity transfers instead of being
simulated locally. Cross-cell teleport, if ever wanted, is separate work built on the existing
transfer path.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h`
- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h` / `FrameInput.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp`
  (`ProcessSpawnStatusChanges` and its Phase 5 caller)
- `Engine/Source/Network/NetworkSerialization.cpp`
- `Engine/Source/Network/NetworkProtocol.h` (`kuiProtocolVersion`)
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp` (harness command, if the
  harness entry point is included)

## In scope

- One new append-only `StatusChangeType` and payload for relocating an existing player, with
  its serialization, wire-size, and codec sites.
- Applying the relocation in the Phase 5 Spawn handler in `Players.cpp`, after Collision and
  before the shared CRC stamp.
- Refusing an out-of-cell target.
- The `FrameInput::kiVersion` and `engine::kuiProtocolVersion` bumps the payload change forces.
- Optionally, an AgentHarness command that issues the status change.

## Out of scope

- Chosen-position *spawning* (already delivered by the separate harness placement work).
- Cross-cell teleport and any change to the transfer path.
- Ship teleport ability gameplay: cooldowns, costs, targeting UI, VFX.
- Generalising the override to non-player collections.
- Backward compatibility with the pre-bump protocol or replay versions.

## Risk tier and invariants

Expect Change Workflow **Tier 3**: the change touches wire/protocol layout, serialization
versioning, and simulation state inside the shared CRC — all Tier-2 exclusions in the root
`AGENTS.md` risk triggers.

Invariants to hold: append-only status-change enumerators; client and server apply the same
published `FrameInput` and must produce identical PostRender bytes; the relocation must land
after Collision and before the CRC stamp; entity positions stay inside their own cell.

## Acceptance criteria

- A teleport request relocates the target player to the requested in-cell point on the tick it
  is consumed, and the player takes no blaster or missile hit that lies only along the jump
  path.
- Client and server CRCs match across the teleport tick, and a replay of that tick reproduces
  bit-identically.
- A target outside the entity's own cell is refused and logged, with the entity unmoved.
- Both version constants are bumped in the same change as the payload.

## Revisit When

A ship gains a teleport ability, or an AgentHarness scenario needs an already-spawned entity
repositioned rather than spawned at a chosen point.

## Notes

Provenance: explicitly requested by the user during the Tier-3 preparation of
`Documents/Plans/Agents/HarnessPlacementAndPusherOverflowObservability.md`. All code evidence
above was verified against baseline `774a1def513887aae849cc1c7d4a380f9c983711`.
