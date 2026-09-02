<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-02T19:30:05.058Z","dependsOn":[]} -->
# Reject non-finite navigation vertices at the static-data trust boundary

## Context

`NavData::Read` (`Engine/Source/Frame/NavCellData.cpp:477`) bounds each
deserialized count against the stream with `common::ValidateDeserializedCount`
(`Common/Serialization.h:48`), and validates polygon offsets and
visibility-edge endpoints, but reads every vertex coordinate raw in the loop
immediately after the `"NavData::Read vertices"` count guard:

```
common::Read(rStream, vertices.at(i));   // NavCellData.cpp:487 at recording time
```

Nothing rejects a NaN or infinite `XMFLOAT2` arriving in the client static-data
message. `BuildNavAcceleration`'s global and per-polygon AABB reductions use
`std::min`/`std::max` (`NavCellData.cpp:385-450`), which silently skip a NaN and
propagate an infinity into `gridMin`/`gridMax`. With `gridMin` at `-inf` and
`gridMax` at `+inf` the extent is `+inf`, so the small-extent early-out in
`NavGridCell` (`Engine/Source/Frame/NavBuild.h:26-30`) does not fire;
`(fValue - fMin) / fExtent` is then `inf / inf`, which is NaN, and
`NavBuild.h:31` passes that NaN to `static_cast<int32_t>`. Converting a NaN to
an integer is undefined behavior, so the observation that the resulting graph
does not crash today rests on undefined behavior — on x86 the conversion yields
`INT_MIN`, which the following `std::clamp` maps to cell 0. The NaN AABB
comparison in `PointInAnyPolygon` fails closed
(`Engine/Source/Frame/NavQuery.cpp:162-182`).

The failure is silent divergence rather than a crash: the accepted vertices
still define the obstacle polygons and visibility edges that `NavQueryDirection`
(`Engine/Source/Frame/NavQuery.h:8`, `NavQuery.cpp:616`) steers units with, so a
hostile client-side nav graph moves CRC-fed client positions along a path the
server's graph never produces.

This is the same trust-boundary class as the polygon-offset and
visibility-edge-index topology validation added in the session that recorded
this Plan; that change's scope covered offsets and edge endpoints only, so
vertex values were outside its approved implementation boundary and remain
unvalidated. The gap is pre-existing at baseline commit
`0b68835a473722fc3c297905dcecd805e785b465`.

## Design

The author's recommendation is to validate vertex values in `NavData::Read`,
in the same place and by the same mechanism the existing count and topology
checks already use: after the vertex loop and before any derived data is built,
require every vertex's `x` and `y` to be finite, and throw the existing
`common::CorruptStreamException` otherwise so the receive boundary drops the
whole payload. This keeps one rejection path for corrupt navigation input
rather than adding clamping, repair, or per-polygon dropping.

The author does not recommend adding a world-extent range check alongside the
finite check: no observed evidence bounds legitimate nav vertices, and an
invented bound could reject valid server-built data.

## Critical files

- `Engine/Source/Frame/NavCellData.cpp:477-550` — `NavData::Read`, where the
  vertex loop and the existing count and topology checks live.
- `Common/Serialization.h:48` — the existing count guard and the
  `CorruptStreamException` convention the new check follows.
- `Engine/Source/Frame/NavCellData.cpp:385-450` — `BuildNavAcceleration`, the
  first consumer of the vertices (read-only reference; not modified).
- `Engine/Source/Frame/NavQuery.cpp:162-182,616` — query consumers whose
  divergence motivates the check (read-only reference; not modified).
- `Engine/Source/Network/Client/Client.cpp:303` — the
  `catch (const std::exception&)` in `Client::Receive` that drops the whole
  packet, which the Design and acceptance criterion 1 rely on (read-only
  reference; not modified).
- `Engine/Source/Frame/AGENTS.md` — navigation and client static-data authority.

## In scope

- A finite-value check over the deserialized `vertices` array inside
  `NavData::Read`, placed before derived acceleration data is built.
- Routing a non-finite vertex through the existing corrupt-static-data
  exception path already used by the count and topology checks in that function.

## Out of scope

- Any change to `BuildNavAcceleration`, `NavGridCell`, `PointInAnyPolygon`,
  `NavQueryDirection`, or any other build or query predicate.
- Polygon offset, polygon ordering, and visibility-edge index validation, which
  the already-present topology checks in the same function own.
- Clamping, sanitizing, or repairing non-finite vertices; dropping individual
  polygons; changing the packet format, `kiNavDataVersion`, count limits, or
  server-side nav generation.
- Adding a world-extent or magnitude bound on vertex coordinates.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: adding a check inside one unit at an
existing trust boundary, with the serialized format and the trust relationship
unchanged — the root `AGENTS.md` Tier-2 clause for that case. It is not Tier 3
because no wire format, serialization layout, nav version, replay
compatibility, or threading surface changes; only already-corrupt input takes a
new path.

Preserve these invariants:

- Valid server-built nav data deserializes to a bit-identical graph, so client
  and server queries and the tick CRC are unchanged.
- Corrupt navigation input is rejected as a whole payload, never repaired into
  a partially valid graph.
- The existing count and topology checks, `kiNavDataVersion`, and the
  static-data packet format stay as they are.

## Acceptance criteria

- A static payload whose vertex array contains a NaN or an infinite coordinate
  is rejected inside `NavData::Read` before acceleration data is built, and the
  client drops the payload without an uncaught update exception.
- A valid multi-polygon nav payload still deserializes and builds the same
  acceleration data and produces the same query results as before the change.
- Client and server `Debug|x64` builds are clean through `/compile`.

## Notes

Source: `/plan-audit` finding PA-F-003 raised while auditing
`Documents/Plans/Engine/NavDataTopologyValidation.md`, recorded as an
out-of-scope leftover of that session rather than fixed in it.

Session provenance (machine-local; not reproducible after cleanup):

- Client: claude
- Conversation session ID: ac8865d0-13e1-42b0-b2aa-446839ceb75b
- Worktree/branch UUID: 3883d918-d77a-4003-8d03-5ecc5653f131
- Session branch: claude/3883d918-d77a-4003-8d03-5ecc5653f131
- Worktree: .claude\worktrees\BrokenEngine\3883d918-d77a-4003-8d03-5ecc5653f131
- Landing ref: the session branch above, whose tip is that session's final
  commit.
