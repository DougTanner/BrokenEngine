<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:26.150Z","dependsOn":[]} -->
# Validate serialized frame areas before grid-state adoption

## Context

The accepted finding `CAI/shard-0014/004` identifies a derived-geometry trust
boundary gap. `FrameStaticData::Read` accepts serialized `vecArea` after only
checking placement count (`Engine/Source/Frame/FrameStaticData.cpp:24-37`), and
`ReadGridSave` adopts it without checking the separately serialized coordinate
(`Engine/Source/File/GridSave.cpp:104-113,160-166`). Collision and transfer use
the lane order directly through `ComputeFrameBounds`/`IsOutOfBounds`
(`Engine/Source/Frame/FrameUtils.h:56-65,132-139`). A finite reversed area can
therefore make every entity appear outside its own cell and transfer the whole
cell after a successful load.

The source shard's frozen/live target hashes match audit baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; current status contains only the
six pre-existing `Documents/Plans/Agents/Cpp*Audit.md` edits. The missing area
validation is unresolved, pre-existing, and outside the approved audit work.

## Design

The author's recommendation is to validate the four `vecArea` lanes at the
staging/adoption boundary against the frame coordinate: require finite values,
`minX < maxX`, `minY < maxY`, and the canonical cell extents derived from that
coordinate. Use the existing corrupt-save/static-data failure path rather than
publishing inverted geometry. Apply the same rule to save and network static
data while preserving the serialized representation and valid writer output.

## Critical files

- `Engine/Source/Frame/FrameStaticData.cpp:8-65` — area read and derived-data reset.
- `Engine/Source/File/GridSave.cpp:104-166` — save staging/adoption.
- `Engine/Source/Network/Client/ClientReceive.cpp:324-339` and
  `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSessionReceive.cpp:18-31` — network static-data staging/application.
- `Engine/Source/Frame/FrameUtils.h:56-65,121-139` — area convention and bounds consumers.
- `Engine/Source/Frame/AGENTS.md` and `Engine/Source/File/AGENTS.md` — geometry/save trust-boundary rules.

## In scope

- Finite/order/extent validation of `FrameStaticData::vecArea` after its owning
  coordinate is known and before save or network state is published.
- Routing an invalid area through the existing staged-save or static-data
  rejection path without mutating live frames.
- The read/staging/adoption and bounds-consumer regions named above.

## Out of scope

- Changing cell dimensions, coordinate-to-area arithmetic, collision/transfer
  policies, navigation, placement CRC validation, duplicate save coordinates,
  or the serialized frame version/layout.
- Repairing an invalid area by moving entities or silently accepting reversed
  bounds.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Serialized
save/network geometry controls deterministic collision and transfer state and
crosses staged-adoption trust boundaries.

Tier rationale: the Design fully specifies the predicate (finite lanes,
`minX < maxX`, `minY < maxY`, canonical extents for the owning coordinate) and
routes failures through the existing rejection path at the same read/adoption
boundary. It adds only corrupt-input rejection; the serialized representation,
area convention, and valid-data collision and transfer results are unchanged.

Preserve these invariants:

- Each adopted frame area is finite, ordered as `x=minX, y=maxY, z=maxX,
  w=minY`, and canonical for its `GridCoord`.
- Invalid static data cannot reach `ComputeFrameBounds`/transfer as live state.
- Valid save/network payloads, strict boundary tests, frame version, and CRC
  behavior remain unchanged.

## Acceptance criteria

- A save or static-data payload with finite but reversed area lanes is rejected
  before adoption and leaves no live cell whose entities are all marked for
  transfer.
- A valid non-origin coordinate's canonical area passes and yields the existing
  collision/transfer results.
- Client and server `Debug|x64` builds clean through `/compile`; save-load and
  static-data acceptance exercises cover both boundaries.

## Coordination

`Documents/Plans/Engine/GridSaveIslandReference.md` and
`Documents/Plans/Engine/GridSaveDuplicateCoord.md` share the staged save
boundary but own island membership and coordinate-key uniqueness. Keep area
validation independent, preserve the common failure path, and re-derive line
citations before implementation. No dependency is required.

`Documents/Plans/Engine/ClientStaticIslandReference.md` also validates client
static-data receive before queueing. Keep area geometry separate from island
membership at that shared boundary, preserve whole-packet rejection and slot
immutability, and re-derive the receive line ranges before implementation. No
dependency is required.

## Notes

The consolidated index records no duplicate-family hint or external claim for
this candidate.
