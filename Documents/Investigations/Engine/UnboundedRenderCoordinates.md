# Unbounded Render Coordinates

Status: Open investigation; no implementation decision has been made.

Area: Engine

Record type: Non-executable reference material. This document intentionally has
no scheduler metadata and is not a Plan input.

Audit source: `CAI/shard-0014/005` in the frozen C++ adversarial audit.

Frozen audit commit: `76d303f0eeeb86c1ed241edc81634e60070ba5a5`

## Finding under investigation

The candidate is anchored at `Engine/Source/Frame/FrameUtils.h:121-128`
(`ComputeFrameArea`) and `Engine/Source/Frame/GridCoord.h:6-16`
(`GridCoord`). `GridCoord::x` and `GridCoord::y` are signed `int32_t` values.
`ComputeFrameArea` takes a 900 m base-cell width and height, converts the cell
coordinate to `float`, and adds the resulting absolute offset to the four
float lanes that represent the frame area.

This conversion does not preserve a stable cell area across the declared
coordinate domain. At the legal coordinate `x = 16,777,216`, the single-
precision offset is approximately `1.5099494E+10`; adding `-450` and `+450`
rounds both X edges to the same float, so the computed width is zero. Even at
`x = 1,000,000`, the rounded width is 896 m instead of 900 m. These values are
the concrete single-precision calculation recorded by the audit, not a
render-only visual comparison.

The active-frame path accepts the coordinate: `ServerSessionRuntime::SyncActiveFrames`
materializes it, and `GameBase::CreateFrameAtCoord` publishes the resulting
`vecArea`. `Collision::SetupZones` derives zone dimensions from those edges,
while `TracePointToFrameExit`, `IsOutOfBounds`, and transfer handling consume
the bounds. Terrain sampler origins and world placements also perform
large-magnitude float subtraction. A zero-width area can therefore make zone
mapping divide by zero or become invalid, classify ordinary points outside the
strict interval, and break the own-cell rule for collision, transfer, and
terrain queries.

## Controlling contract and invariant

The root engine contract describes an unbounded sparse grid of independently
simulated cells. The Frame authority requires an entity position to remain in
its own cell; an out-of-bounds position transfers to a neighboring cell. A
supported coordinate must consequently retain a nonzero, usable frame area and
enough local precision for fixed-tick bounds, collision, transfer, and terrain
sampling. The `GridCoord` key can still identify the cell while the float area
fails to represent it, so preserving the integer key alone does not satisfy
this invariant.

## Open alternatives

These are alternatives for a future user decision. Their order is not a
recommendation, and no behavior is selected here.

1. **Camera/floating-origin relative math.** Determine whether a camera or
   floating-origin rebase can keep frame and simulation geometry local before
   any large absolute float is formed. The decision must identify the rebase
   owner and whether the same representation is available to deterministic
   collision, transfer, terrain, client, and server paths.
2. **Wider/relative intermediate bounds.** Keep the integer cell identity but
   calculate the origin and edges using a wider type or an origin-plus-local
   representation. The decision must establish where a float is eventually
   required, whether that conversion still collapses the edges, and which
   frame-area and consumer interfaces would carry the representation.
3. **Explicit finite range.** Define a supported coordinate limit and reject or
   otherwise handle coordinates outside it before frame creation. This changes
   the root unbounded-grid authority and must be documented as an intentional
   contract change; it cannot be silently treated as a numerical fix.

## Decisive questions and evidence needed

- Is the signed `int32_t` coordinate domain normative, or is a finite supported
  range intended? If finite, what are the limits and what behavior is required
  at each boundary?
- Must deterministic frame geometry preserve exactly the 900 m cell extent for
  every supported coordinate, or is a defined local-precision tolerance
  acceptable? The answer must cover both client and server and the shared CRC
  boundary.
- Which consumers require an absolute world position, and which can consume a
  cell-relative position? A complete call-site trace should cover
  `ComputeFrameArea`, `vecArea`, `FrameBounds`, collision zone setup, frame
  exit/transfer, terrain origins, and render/network conversion boundaries.
- For each alternative, can a coordinate sweep around the first width collapse
  and at the supported extrema show nonzero bounds, correct strict-boundary
  behavior, stable terrain samples, and identical client/server deterministic
  results? The evidence should include negative coordinates as well as positive
  ones.
- If the finite-range alternative is chosen, which authority documents change,
  which caller enforces the limit, and how is an out-of-range active coordinate
  reported without creating a partial frame?

The resulting evidence should be a source-level representation trace plus
focused boundary scenarios. No source fix or finite-range assumption is
authorized by this record.

## Earning an executable Plan

After a user chooses the coordinate representation and, if applicable, the
supported domain and authority changes, this investigation can be converted
into a decision-complete Plan under `Documents/Plans/Engine/`. That Plan must
carry the required byte-zero scheduler metadata, name the exact frame-area and
consumer regions in scope, state the out-of-scope boundaries and invariants,
and define the client/server and boundary acceptance evidence. Until those
choices exist, this record remains reference material and WorktreeCli must
ignore it.

## Provenance

- Frozen audit report: `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0014.md`
- Candidate: `CAI/shard-0014/005` — “world-area float construction collapses cells in the unbounded int32 grid”.
- Governing authorities read for this record: `AGENTS.md`,
  `Engine/Source/AGENTS.md`, and `Engine/Source/Frame/AGENTS.md`.
- No option above is a chosen behavior; no source, shader, build, or scheduler
  change is part of this investigation.
