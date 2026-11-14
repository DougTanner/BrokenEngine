<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:00:29.539Z","dependsOn":[]} -->
# Keep CurveData::AddPoint From Admitting a Duplicate X

## Context

`CurveData` (`Engine/Source/Ui/CurveData.h`, client-only) keeps its control
points ordered in X. `MovePoint` enforces a gap: it clamps an interior point's
X between its neighbours plus and minus `kfSeparation` (`:76-79`). `AddPoint`
(`:38-53`) has no counterpart. It clamps X only to the open interval (0, 1)
(`:44`) and finds the insert slot with a strict `<` (`:47`), so a new point
whose X equals an existing interior point's X is inserted directly before it,
leaving a zero-width segment.

That segment makes `Evaluate` return NaN for nearby X values:

- `Secant` divides by the segment's width (`:168-169`), so the zero-width
  segment's secant is infinite when the two Y values differ.
- `SecantTangent` averages it with its neighbour when their signs agree
  (`:157-163`), giving an infinite tangent at a shared point.
- In `ComputeTangents` the ratio is then infinite, the squared magnitude
  exceeds 9, `fScale` is `3 / sqrt(inf) = 0`, and the rescaled tangent is
  `0 * inf`, which is NaN (`:134-141`).
- `Evaluate` multiplies that tangent in (`:115`), and `std::clamp` (`:116`)
  returns a NaN argument unchanged, because both of its comparisons are false.

Worked case: points (0, 1), new (0.5, 0.8), existing (0.5, 1.2), (1, 2). The
secants are -0.4, +infinity, and 1.6; `Evaluate(0.75)` uses the last segment,
whose left tangent becomes NaN. The zero-width segment itself is never
evaluated (`:96` is strict), and when the infinite secant's sign differs from
its neighbour's, the tangent is 0 and the curve stays finite.

Consumers: `Engine/Source/Graphics/Render/LightingUniforms.cpp:171-174` writes
`Evaluate` for every spread-pass fraction into
`GlobalLayout::pfCombineCurvePoints` each frame, so the NaN reaches the
lighting shaders as a per-pass weight; `CurveWidget.cpp:65` draws the same
values. No settings file holds a curve, so nothing persists to disk. The one
export is "Copy Curve To Clipboard"
(`Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp:120-134`),
which prints the points as source text for `LightingWrappersBase.h`; a
duplicate-X pair pasted there would produce NaN from startup.

Reachability: the editor adds a point on a left click that is not within the
grab tolerance of an existing point (`CurveWidget.cpp:72-98`). In ImPlot 1.1
WIP (`implot.cpp:3781-3785`) the same pixel column yields a bit-identical plot
X, so a second click in the column of an existing point, at least the grab
tolerance away from it vertically, reaches `AddPoint` with an equal X. It is
rare by hand and deterministic when it happens. The state lasts until the
point is dragged (`MovePoint` re-separates it), removed, or the process
restarts.

Originating gap: a deep analysis of `Engine/Source/Ui` — a refactor finding,
independently verified by re-tracing the arithmetic above. Unmet criterion:
after `AddPoint`, adjacent points are at least `kfSeparation` apart in X, as
after `MovePoint`.

## Design

The author's recommendation, all in `CurveData.h`:

1. Move `kfSeparation` from inside `MovePoint` to class scope so both
   functions share it.
2. In `AddPoint`, after the insert slot is found, compute the lowest and
   highest X that keep `kfSeparation` from the two neighbours of that slot.
   When the lowest exceeds the highest there is no room: return `-1` without
   inserting. Otherwise clamp the new X into that range and insert. The
   explicit no-room test is required because `std::clamp` with its lower bound
   above its upper bound is invalid.
3. Extend the comment above `AddPoint` to say it also returns `-1` when the
   neighbours leave no room.

The only caller already treats `-1` as "nothing added"
(`CurveWidget.cpp:98`).

## Critical files

- `Engine/Source/Ui/CurveData.h`

## In scope

- `CurveData::AddPoint` and its comment.
- The scope of `kfSeparation` (function-local in `MovePoint` to class scope).

## Out of scope

- `Evaluate`, `ComputeTangents`, `SecantTangent`, and `Secant`: no NaN guard
  in the evaluator.
- `MovePoint`'s behavior, `RemovePoint`, and the constructor.
- `CurveWidget`, the grab tolerance, and the clipboard export.
- The two shipped curves in `LightingWrappersBase.h`.

## Acceptance criteria

- The diff shows the shared constant and `AddPoint` separating the new point
  from both neighbours or returning `-1`.
- The BrokenEngineSandbox client and ModelViewer build.
- No live check: the harness drives the UI by item activation and cannot
  click a position on the plot canvas, so the criterion is settled by code
  reading — after the change no call to `AddPoint` can leave two adjacent
  points closer than `kfSeparation`.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- Change Workflow tier: Tier 2 (the author's classification). Trigger: scoped
  runtime behavior of one client-only editor type — a click can now be
  refused or have its X moved. The curve feeds a render uniform, and render
  sits outside the CRC, so there is no determinism, wire, serialization,
  replay, or threading exposure; no shader or layout change; no allocation
  change (`mPoints` is reserved to its cap).
- Whether a NaN weight lingers in the lighting temporal history after the
  duplicate point is removed was not traced.
