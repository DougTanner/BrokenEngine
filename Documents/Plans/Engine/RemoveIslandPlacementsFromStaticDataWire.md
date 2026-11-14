<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T14:19:47.916Z","dependsOn":[]} -->
# Stop sending island placements in the static-data packet

## Context

`CellStaticData::Write` (`Engine/Source/Frame/CellStaticData.cpp:44-50`)
starts the `kServerCoordinateStaticData` body with an `int32_t` placement
count and, per placement, `islandCrc`, `f2WorldPosition`, and `fRotation`.
`CellStaticData::Read` (lines 92-100) resizes `islands` to that count and
reads them back.

No receiver uses them:

- `Write` has one caller, `Engine/Source/Network/Server/ServerBufferedFrames.cpp`
  (line 211). `Read` has one caller, `Engine/Source/Network/Client/Client.cpp`
  (line 535), which pushes the record to `mReceivedStaticData`.
- The only drain is `ClientSessionBase` into
  `ClientSessionRuntime::StageStaticData`
  (`Engine/Source/Network/Client/ClientSessionRuntime.cpp:153-163`), which
  moves out `navigationData` alone.
- Client placements come from `GenerateIslandChain`
  (`ClientSessionRuntime.cpp:126`), as
  `Engine/Source/Network/Client/AGENTS.md` `## Live Terrain Preparation`
  states: "Received static data contributes only its navigation."
- Saves regenerate placements (`Engine/Source/File/GridSave.cpp`), and the
  `cell_coordinate_probe` command
  (`Engine/Source/Agent/Commands/CellCoordinateProbe.cpp`) hashes generated
  and live-cell placements, never received ones.

So every static-data packet carries a block the client allocates for
(`islands.resize(iCount)`) and then discards. The unmet condition: the
static-data body carries only what a receiver consumes.

Origin: an architecture review of `Engine/Source/Frame` (architecture phase,
client/server and data-shape lens), validated against commit `ae681f03`.

## Design

Recommended: remove the placement block from both functions, so the body
starts at the navigation graph count, and bump `engine::kiProtocolVersion`
(`Engine/Source/Network/NetworkProtocol.h`), because the layout of an engine
packet changes (`Engine/Source/Network/AGENTS.md` `## Transport Contracts`).

Removal is recommended over making the client compare received placements
with generated ones: that would add a client check on server data, which
`Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy` rules out, and it
would be a new feature rather than debt removal.

Documentation the change makes wrong, to fix in the same change:

- `Engine/Source/Frame/AGENTS.md` `## Terrain and Navigation`: the sentence
  "Static-data deserialization trusts the count under the corrupt-input
  policy" (about `kiMaximumIslandsPerCell`) and the word "placement" in
  "Preserve placement and vertex bits" both describe the removed block.
- `Documents/Architecture/Network.md`: add the new version to the
  version-history paragraph that follows the client-to-server contract table.
- The comment above `PlacementsCrc` in `CellCoordinateProbe.cpp` says the fold
  matches `CellStaticData::Write`'s member order; after the change `Write`
  has no placement members.

## Critical files

- `Engine/Source/Frame/CellStaticData.cpp`
- `Engine/Source/Network/NetworkProtocol.h`
- `Engine/Source/Frame/AGENTS.md`
- `Documents/Architecture/Network.md`
- `Engine/Source/Agent/Commands/CellCoordinateProbe.cpp` (comment only)

## In scope

- In `CellStaticData::Write`: the placement count and the placement loop.
- In `CellStaticData::Read`: the count read, `islands.resize`, and the
  placement loop.
- `kiProtocolVersion`: one increment of the then-current literal.
- The documentation and comment sites listed in `## Design`.

## Out of scope

- The navigation part of `Write`/`Read`: graph count, vertices, gap-encoded
  polygon offsets, visibility edges, and `BuildNavigationAcceleration`.
- `CellStaticData::islands` itself and every reader of it.
- `ServerBufferedFrames`' cached encoding and LZ4 framing, and
  `NetworkMessages::ServerCoordStaticDataMessage`.
- `ClientSessionRuntime::StageStaticData` and terrain preparation.
- Any client comparison of received and generated placements.
- `kiNavDataVersion`, `Frame::kiVersion`, and the save format: `NavData` and
  saved state do not change.
- `PlacementsCrc`'s computation.

## Risk tier and invariants

Tier 3 under `.agents/references/risk-tiers.md`: a wire/protocol change. The
bytes an engine packet carries change, so a client and server of different
builds must be rejected at Hello by the bumped version.

Invariants to hold:

- No backward compatibility: one current layout, older builds rejected.
- The client still marks `kNavigationDataBuilt` after `Read`, including for an
  empty graph.
- Client and server placements for a coordinate stay equal, since both still
  come from `GenerateIslandChain`.

## Acceptance criteria

1. `CellStaticData::Write` and `Read` neither write nor read a placement
   count or placement fields.
2. `kiProtocolVersion` is one higher than at the start of the change, and
   `Network.md` records the new version.
3. The BrokenEngineSandbox client and server build through `/compile`.
4. Through `/agent-harness`, with a local server: the client connects and
   publishes an authoritative cell that has navigation data, and
   `cell_coordinate_probe` for that cell's coordinate reports the same
   `placements.crc` on client and server.
5. The replay determinism recipe in
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md` shows no
   CRC mismatch.

The originating evidence records no harness setup for criteria 4 and 5; the
executing session sets them up from the harness documentation.

## Coordination

`Documents/Plans/Engine/StaticDataDrainInClientRuntime.md` moves the
static-data drain that `## Context` names from `ClientSessionBase` into a
private `ClientSessionRuntime` member. Neither Plan depends on the other, and
this Plan's scope is unchanged: after that Plan lands, the only drain is that
runtime member into `ClientSessionRuntime::StageStaticData`.

## Notes

- `BuildPlaneNavigationData` (`Engine/Source/Frame/NavCellData.cpp`) and the
  navigation build around it compile only under `BT_SERVER`. Its comment above
  the `vertices.empty()` test cites `CellStaticData::Read` on the client.
  Re-read that comment against the changed `Read`; this Plan does not edit it,
  so if it no longer holds, surface it as a leftover under the Change Workflow.

- The byte saving is small: at most `kiMaximumIslandsPerCell` twenty-byte
  entries per packet, sent from a cached encoding. The gain is removing dead
  wire content and the discarded allocation, not bandwidth.
