<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-23T17:42:12.289Z","dependsOn":[]} -->
# Server monitor cell map drawn as flat-top hexes

Authored against `86020b734f78e45a03374b78bd4e0fab58593078`, before its prerequisite, the Hexagonal simulation
cells Plan (the parent Plan), landed. Cells are flat-top hexagons whose `GridCoord` fields are the axial `iQ`/`iR`;
`Engine/Source/Frame/GridCoord.h` holds the `kCellDirections` table and the bounding half extents
`kfCellHalfWidth`/`kfCellHalfHeight`, and `Engine/Source/Frame/FrameUtils.h` holds the wrappers, among them the
float centre offset `CellCenterOffset`. `Engine/Source/Server/ServerDisplay.cpp` line numbers below are from the
authoring tree; the parent changed that file by its field rename only, so they shift by nothing more than that rename.

## Context

The user deferred the monitor redraw from the parent Plan: "'Option 1: Redraw the monitor's cell map as hexes.'
-> as a followup plan". The parent records that deferral as its D10 settled decision and as an `## Out of scope`
entry: the server monitor's cell map stays a square grid of the axial coordinates, sheared, until this Plan lands.

Root cause (current source): `PaintGridMap` (`Engine/Source/Server/ServerDisplay.cpp:188-317`) takes the
min/max of the active coordinates' two `GridCoord` fields, pads each by one cell (`:195-212`), and draws every
coordinate of that rectangle as a square, column from the first field and row from the second, North up
(`:231-240`); each square is a `FillRect` plus a four-segment outline (`:272-281`), with a red inset `Rectangle`
for a subscribed coordinate (`:283-292`) and text labels at the square's top-left and top-right corners
(`:294-314`). Once the parent makes those fields axial (q, r) with centre offset dq·A + dr·B, A = (a√3, a),
B = (0, 2a), a = 450 m (parent D2), that square grid is a sheared lattice: the edge neighbour (1,−1) draws as a
diagonal of (0,0), and the non-neighbour (1,1) draws as one too. The map's stated job — showing which cells are
active, client-authorized, idle, and subscribed, North up (`Engine/Source/Server/AGENTS.md`
`## Ownership and Cadence`, last bullet) — then shows false adjacency.

Exposure: server-only GDI paint (`ServerDisplay.cpp:3`, `#if defined(BT_SERVER)`), on the server main thread
inside `Main.cpp`'s existing allocation-tracking suppression (`Engine/Source/Server/AGENTS.md`
`## Ownership and Cadence`). It only reads `game::gpGame->mActiveCoordinates`, the client records, and per-cell
statistics; it writes no simulation state, so determinism, CRC, wire, save, replay, and trust are untouched.

## Design

Author's recommendations, each with its rationale; no user decision covers these details beyond the redraw itself.

1. **Which cells are drawn.** Today the map fills the padded bounding rectangle of the active coordinates with
   idle grey cells. Recommended: keep that look in hex form — draw every coordinate whose centre lies in the
   padded bounding rectangle of the active centres. On the parent's basis the column is q alone (x = q·a√3), and
   the centre's height in apothems is the integer s = q + 2r (y = a·s), so the rectangle enumerates in integers
   just as today's bounds do (`:195-212`): q from the active minimum − 1 to the active maximum + 1, clamped to
   the `int32_t` range as today (`:209-210`); s from the active minimum − 2 to the active maximum + 2 (one cell
   height of padding, as today's one row); for each q, only the s with s − q even, giving r = (s − q) / 2,
   skipped when r falls outside the `int32_t` range. Rationale: the padded filled rectangle is what the monitor
   shows today and fits the rectangular map area; integer bounds avoid any float-to-coordinate conversion.
2. **Positions.** Each drawn cell's centre is the parent's float centre-offset wrapper in
   `Engine/Source/Frame/FrameUtils.h` (parent Approach 1, "the centre offset between two `GridCoord`s"),
   from the first active coordinate to that cell, in metres; screen y is negated so North (+y) stays up. One
   pass over the enumerated cells takes the min/max of those offsets, widened by the bounding half extents
   (R, a) ≈ (519.62, 450) that `Engine/Source/Frame/GridCoord.h` reads from `kCellDirections` (parent
   Approach 1); that world rectangle is fitted and centred in the map area like today's grid (`:217-224`). The
   pixel scale is the smaller of map width / rectangle width and map height / rectangle height, capped so a hex
   is at most 60 px tall (today's 60 px cell cap, `:217-219`); when a hex would be under 4 px tall the map is
   skipped (`:226-229`). Rationale: reuses the parent's offset wrapper and half extents, so no lattice
   arithmetic beyond the enumeration bounds is restated here.
3. **Hex outline.** The six vertices of a flat-top hex are the centre plus (±R, 0) and (±R/2, ±a), from the same
   `GridCoord.h` half extents scaled to pixels and rounded to `POINT`. One GDI `Polygon` call with the cell
   brush and border pen selected replaces today's `FillRect` and four `LineTo` calls (`:263-281`); brush and pen
   choice is unchanged (client-authorized, active, idle). Rationale: `Polygon` fills and outlines in one call
   with the existing cached brushes and pens.
4. **Subscribed outline.** The red inset outline (`:283-292`) becomes a `Polygon` of the same six vertex
   offsets scaled about the centre by (a_px − 2) / a_px, with `sHpenRed` and `NULL_BRUSH`, so every edge moves
   in by the existing 2 px inset (`kiInset`); drawn when the hex is at least 20 px tall (today's
   `iCellSize >= 20`). Rationale: a regular hexagon's edges are equidistant from its centre, so one scale gives
   a uniform inset.
5. **Labels.** Drawn when the hex is at least 24 px tall (today's `iCellSize >= 24`, `:295`), with today's
   offsets applied to the hex's top edge instead of the square's top edge: the "(q,r)" label at
   (centre x − R_px/2 + 2, top + 2), the entity count at (centre x − R_px/2 + 2, top + 28), and the client count
   at (centre x + R_px/2 − 20, top + 2), where top = centre y − a_px. The "(q,r)" label prints the coordinate's
   `iQ`, `iR`. Rationale: the square's top-left corner lies outside the hex; the top edge's ends are the
   nearest inside points.
6. **Hash.** `ServerDisplayContentChanged` (`:121-186`) is unchanged: the redraw paints no new input, and it
   already folds every active, authorized, and subscribed coordinate (the hash rule in
   `Engine/Source/Server/AGENTS.md` `## Ownership and Cadence`).

## Critical files

- `Engine/Source/Server/ServerDisplay.cpp` — `PaintGridMap`.
- `Engine/Source/Server/AGENTS.md` — the map bullet.
- Read only, from the parent: `Engine/Source/Frame/GridCoord.h` (`kCellDirections`, bounding half extents),
  `Engine/Source/Frame/FrameUtils.h` (float centre-offset wrapper); `ServerDisplay.cpp` already reaches both
  through `Pch.h` (game `Frame/Frame.h:3` includes `Frame/FrameBase.h`, which includes `Frame/FrameUtils.h` at
  `FrameBase.h:22`).

## In scope

- `Engine/Source/Server/ServerDisplay.cpp` `PaintGridMap` (`:188-317`): the cell enumeration and bounds
  (Design 1), the fit and centre positions (Design 2), the per-cell hex polygon (Design 3), the subscribed inset
  polygon (Design 4), and the label positions (Design 5). The per-cell active, authorized, and subscribed tests
  (`:240-261`) and the brush/pen selection keep their logic.
- `Engine/Source/Server/AGENTS.md` `## Ownership and Cadence`, last bullet ("The map renders North (+y)
  upward ..."): state that cells draw as flat-top hexes at their axial centre offsets.

## Out of scope

- The parent Plan's files other than the `ServerDisplay.cpp` regions under `## In scope`, and its helpers:
  `kCellDirections`, `common::MakeHexDirections`, the `FrameUtils.h` wrappers, and the client debug cell-edge draw
  (`Engine/Source/Graphics/Render/MainUniforms.cpp`).
- Any new `common::` or engine geometry helper (vertex list, pixel-to-hex lookup, hex rounding) and any other
  new helper.
- `ServerDisplayContentChanged`, the left statistics panel, the tab bar, the Profile panel, `HandleServerClick`
  (no map click handling exists or is added), the cached GDI objects (no new brush, pen, or colour), and
  `Main.cpp` repaint cadence.
- Map pan, zoom, or selection; drawing cells outside the padded rectangle; changing what counts as active,
  authorized, or subscribed.
- The agent-harness server-window recipe and every harness document.

## Change Workflow tier

Tier 2 — trigger: one subsystem's runtime behaviour, the server monitor's paint in `PaintGridMap`
(`Engine/Source/Server/ServerDisplay.cpp:188-317`); it touches no determinism/CRC, wire, serialization, save,
replay, threading, or trust surface (server-only GDI paint that only reads state on the main thread).

## Acceptance criteria

- A1 Build: `/compile` BrokenEngineSandbox server Debug and Release x64 succeed with no new warnings, and the
  static-check runner passes.
- A2 Live map (`/agent-harness`, Debug). The supplied evidence records no harness run of this state; the
  recommended setup, in order: launch server and client; with the client connected and authorized at its cell,
  queue two edge-coast `SpawnOmni` payloads from that cell (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`
  "Edge coast", as the parent rewrites it for flat-top edges), one across the SE edge toward (1,−1) and one
  across the N edge toward (0,1) relative to the client's cell; wait until `query_collection` finds each omni
  in its neighbour; read `describe_scene.subscribedCoords`; then capture the server window with the recipe in
  `.agents/skills/agent-harness/references/server-window.md` `### Server GDI monitoring window capture and
  click`, `-Target Map`. The capture shows flat-top hexes with no square cells: the client's cell filled
  client-authorized blue, (1,−1) active green sharing its lower-right edge, (0,1) active green sharing its top
  edge, the cell at (1,1) relative to it not sharing an edge with it, idle grey hexes filling the padded
  rectangle, a red inset outline on each subscribed coordinate, and "(q,r)" labels on active cells.
- A3 Code read: positions come only from the parent's float centre-offset wrapper and `GridCoord.h` half
  extents, the enumeration uses only integer bounds, no helper is added, and no new input enters the
  repaint hash.

## Notes

- The parent's D3 tie rule, transfers, and subscription filter do not bear on this paint; the map only places
  coordinates.
- Pixel rounding may let a shared edge of two neighbouring hexes differ by one pixel; that is display-only and
  needs no handling.
