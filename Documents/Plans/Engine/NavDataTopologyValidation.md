<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:20.044Z","dependsOn":[]} -->
# Reject malformed navigation topology before client queries

## Context

The accepted finding `CAI/shard-0014/003` identifies a semantic navigation
trust-boundary gap. `NavData::Read` validates counts and remaining bytes but
accepts raw polygon offsets and visibility-edge indices
(`Engine/Source/Frame/NavCellData.cpp:468-507`). `BuildNavAcceleration` then
derives ranges without requiring a valid ascending partition of the vertex
array (`NavCellData.cpp:413-441`). For a count-valid offset equal to the vertex
count, the polygon gets an extreme empty AABB and no edges; client queries then
walk directly through an obstacle (`Engine/Source/Frame/NavQuery.cpp:162-182,677-707`).
The malformed static-data packet survives receive and changes client movement
against the server graph, violating the shared navigation and reconciliation
contract.

The source shard's frozen/live target hashes match audit baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; current status contains only the
six pre-existing `Documents/Plans/Agents/Cpp*Audit.md` edits. The topology gap
is unresolved, pre-existing, and outside the approved audit work.

## Design

The author's recommendation is to validate navigation topology immediately
after deserialization and before `BuildNavAcceleration`: require offsets to be
an ascending partition within `vertices`, require each polygon's range to meet
the existing minimum-vertex contract, and require every visibility-edge
endpoint to name an in-range paired vertex. Throw the existing corrupt-stream
exception for invalid topology so the receive boundary drops the payload;
preserve the established count/byte checks and shared build/query predicates.

## Critical files

- `Engine/Source/Frame/NavCellData.cpp:376-507` — topology read and acceleration build.
- `Engine/Source/Frame/NavQuery.cpp:162-182,582-731` — blocked/LOS consumers.
- `Engine/Source/Network/Client/ClientReceive.cpp` and `ClientSessionRuntime.cpp` — static-data receive/application (read-only boundary references).
- `Engine/Source/Frame/AGENTS.md` — navigation and client static-data authority.

## In scope

- Polygon offset/order/range validation and visibility-edge endpoint validation
  in `NavData::Read` before derived acceleration is built.
- Routing invalid topology through the existing corrupt static-data result.
- The read/acceleration gate and its direct query consumers named above.

## Out of scope

- Changing nav contour generation, world-space winding, A* heuristics, query
  predicates, count limits, packet format/version, or server-built valid nav
  data.
- Island CRC membership, frame-area geometry, or unrelated collection indices.
- Repairing malformed graphs by clamping or silently dropping individual
  polygons.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Navigation bytes
arrive through serialized network/static-data boundaries and determine
CRC-visible client movement.

Preserve these invariants:

- Every accepted polygon range and visibility edge is valid before acceleration
  construction.
- Server/client use the same build/query predicates for valid graphs; malformed
  input is rejected rather than turned into an empty graph.
- Existing count/stream bounds, world-space geometry, nav version, and
  reconciliation behavior remain unchanged.

Tier rationale: the Design fully specifies added checks inside one function,
`NavData::Read`, that reject malformed topology through the existing
corrupt-stream exception. The packet format, nav version, build and query
predicates, and every valid graph are untouched, so only corrupt input takes a
new path.

## Acceptance criteria

- A count-valid static payload with `polygonOffsets = {vertexCount}` is rejected
  before acceleration/query publication and cannot create a walk-through path.
- Valid multi-polygon data still builds the same acceleration and query results.
- Client and server `Debug|x64` builds clean through `/compile`; a static-data
  receive exercise confirms malformed topology is dropped without an uncaught
  client-update exception.

## Notes

The consolidated index records no duplicate-family hint or external claim for
this candidate.
