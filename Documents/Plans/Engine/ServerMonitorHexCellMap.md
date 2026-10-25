<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-23T17:42:12.289Z","dependsOn":[]} -->
# Server Monitor cell map drawn as flat-top hexes

## Context

The Server Monitor's `PaintMap` in `Tools/ServerMonitor/ServerMonitor.cpp` draws
a square grid from `common::ServerMonitorCell` records. Bounds come from active
records with one cell of padding. Color and labels distinguish active,
client-authorized, idle and subscribed cells. The monitor runs independently
of the server and compiles no engine or game headers.

Cell coordinates are axial q/r. A square drawing of those coordinates shows false adjacency:
(1,-1) is a neighbour of (0,0) but draws diagonally, while (1,1) is not a
neighbour yet draws diagonally too. This Plan changes only monitor presentation.

## Design

The author's recommendations below retain the existing map's filled padded
rectangle, colors, labels and North-up orientation.

1. Enumerate a padded rectangle of active centres in axial coordinates. The
   dimensionless flat-top basis is x = sqrt(3) * q, y = q + 2*r, with apothem 1
   and circumradius R = 2/sqrt(3). Use monitor-local mathematics; physical cell
   dimensions cancel under fitting and the monitor needs no engine headers.
   Bounds use int64_t: q from active minimum minus 1 to maximum plus 1,
   constrained to int32_t, and s = q + 2*r from active minimum minus 2 to maximum
   plus 2. For each q enumerate only s with s-q even, yielding r = (s-q)/2;
   skip r outside int32_t. Record lookup retains the received flags and counts.
2. Position centres relative to the first active record using the same local
   basis, negating screen y. Fit the enumerated centres' bounds widened by
   (R,1) to the map rectangle. Cap hex height at 60 pixels and skip the map
   below 4 pixels, matching the present size limits. Keep bounds in integers
   until projecting to display coordinates; subtract the reference coordinate
   before floating-point conversion.
3. Draw six flat-top vertices at centre plus (+/-R,0) and (+/-R/2,+/-1),
   scaled and rounded to POINT. Select the existing brush and pen and use GDI
   Polygon for fill and border. Preserve the client-authorized, active and idle
   palette and precedence.
4. For subscribed cells at least 20 pixels tall, use NULL_BRUSH and the red pen
   to draw the same polygon scaled about its centre by (a_px-2)/a_px. This
   preserves a 2-pixel inset on every edge.
5. For cells at least 24 pixels tall, draw (q,r) at
   (centre x-R_px/2+2, top+2), entities at (centre x-R_px/2+2, top+28), and
   client count at (centre x+R_px/2-20, top+2), where top=centre y-a_px.
   Server snapshot capture reads `GridCoord::iQ`/`iR`; the existing
   `ServerMonitorCell::iX`/`iY` wire fields carry q/r. Keep the current count and label visibility rules.
6. Snapshot arrival remains the repaint trigger. The monitor reads only its
   received model; server snapshot capture and transport remain unchanged.

## Critical files

- `Tools/ServerMonitor/ServerMonitor.cpp` — `PaintMap`.
- `Tools/ServerMonitor/AGENTS.md` — Presentation.
- Read only: `Common/ServerMonitorProtocol.h` and
  `Engine/Source/Server/ServerMonitorHost.cpp` capture, which own received cell
  identity, flags and counts; no new data is needed.
- Read only: `.agents/skills/agent-harness/references/server-monitor.md`,
  `### Server Monitor window capture and click`.

## In scope

- `Tools/ServerMonitor/ServerMonitor.cpp` `PaintMap`: enumeration, bounds,
  centre projection, fit, hex fill/border, subscribed inset and label positions
  described in Design. Preserve received-model lookup and palette selection.
- `Tools/ServerMonitor/AGENTS.md` `## Presentation`: document that the map draws
  flat-top hexes at monitor-local axial centre offsets with North upward.

## Out of scope

- Engine/game code, the Common wire contract, server capture and transport,
  simulation geometry, deterministic CRC, saves and replays.
- New shared geometry helpers, engine/game dependencies, map pan, zoom,
  selection or map click handling.
- Statistics/Profile panels, tabs, Copy handling, GDI cache and palette,
  connection logic and snapshot repaint cadence.
- Drawing outside the padded rectangle or changing active, authorized,
  subscribed or entity-count semantics.
- Harness recipes and documents.

## Change Workflow tier

Tier 2: one subsystem's runtime behavior, the standalone monitor's map paint.
All changes are display-only reads of the received model on its UI thread;
threading, trust and wire contracts remain unchanged.

## Acceptance criteria

- A1 Build: `/compile -Target ServerMonitor` Release x64 succeeds with no new
  warnings; the static-check runner passes.
- A2 Live map (`/agent-harness`, Debug server/client): connect and authorize a
  client; queue two edge-coast SpawnOmni payloads using the game's
  `Documents/AgentHarness/commands-server.md` Edge coast recipe,
  across SE toward (1,-1) and N toward (0,1). Wait for
  `query_collection` to find each omni in its neighbour and read
  `describe_scene.subscribedCoords`. Launch the standalone monitor and capture
  it using `.agents/skills/agent-harness/references/server-monitor.md`
  `### Server Monitor window capture and click`, `-Target Map`. Evidence shows
  flat-top hexes: client-authorized blue, active green, idle grey padding,
  red subscribed insets and (q,r) labels. Relative (1,-1) shares the lower-right
  edge, (0,1) shares the top edge, and (1,1) shares no edge with the client's
  cell. Retain the monitor PID and stop it after capture.
- A3 Code read: only monitor-local dimensionless axial mathematics places
  cells; integer enumeration respects int32_t coordinate limits; the monitor
  includes no engine/game header and changes no input or wire contract.

## Notes

Pixel rounding can separate neighboring polygon edges by one pixel; this is
presentation-only and needs no special handling.
