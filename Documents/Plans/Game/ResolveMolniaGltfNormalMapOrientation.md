<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T01:19:49.191Z","dependsOn":[]} -->
# Settle the MolniaGltf normal map orientation

## Context

`Projects/BrokenEngineSandbox/Data/Models/MolniaGltf/textures/Molnia_normal.png`
disagrees with its own file's tangent frame by one measurement and agrees with
it by another, so nothing yet shows whether the map is wrong, where, or in which
channel. The map stands unedited. `GetNormal` now reads the file's authored
`TANGENT`, and that frame equals the one the scene had before: a capture under
each differs on 0 of the 65936 model mask pixels above noise floor 8.

The frame is uniform. All 18330 triangles have h of +1 (h is the UV handedness
sign `fHandedness` in `GetNormal`), every `TANGENT` `w` is positive, and the
authored bitangent runs +V on 100.00% of corners. A map agreeing with that frame
measures positive under the curl test.

The curl test: a height-field normal map is curl-free, so red and green must
agree about the image axes. Decode both to -1..1, reconstruct blue from unit
length (0.1 where its square is at most 0.01), and take p = red/blue and
q = green/blue. Over texels some triangle covers, using central differences and
dropping texels where either difference exceeds 0.5, correlate dp/d(row) with
dq/d(column). Positive means green runs +row, which is +V in raw glTF UV, and
negative means -row. It sees only the relative sign of red and green, so a
flipped red measures the same as a flipped green.

Whole-map statistics. Over 11887828 covered texels the map measures -0.5463. The
statistic with no coverage mask is -0.2747, and +0.2691 for the vertically
flipped copy. In 32-texel tiles classed by curl sign, opposed tiles hold 84.9% of
the curl energy (10666 tiles), agreeing tiles 9.7% (2477) and neutral tiles 5.4%
(1787). On three calibration maps the agreeing share is 54.9% to 94.7% and the
opposed share under 1%, so the map's mix is unusual but not one-sided.

Per primitive, the sign differs, which is why no single verdict covers the map:

| Primitive, by first triangle | Covered texels | Curl |
|---|---|---|
| 0 (main) | 11424603 | -0.5566 |
| 13950 | 144684 | -0.8571 |
| 16914 | 198844 | +0.7677 |
| Ten smaller primitives | 9553 to 21179 | -0.18 to +0.19 |

The occlusion result on the main primitive points the other way from its curl.
That primitive holds 63956 of the 65936 mask pixels. Fitting render luminance
over albedo on the geometric normal gives the render's own light direction, and
the residual was regressed on (L dot T) times the relief the occlusion texture
predicts along U, and (L dot B) times the same along V. Positive means the render
leans the surface as occlusion says:

| Map state on primitive 0 | b1, along T (t) | b2, along B (t) |
|---|---|---|
| Stored | +0.458 (4.2) | +0.182 (2.9) |
| Green inverted | +0.448 (4.1) | -0.492 (-7.8) |
| Red inverted | -0.155 (-1.4) | +0.278 (4.4) |

A channel's own contribution is half the change its inversion causes, which gives
red +0.306 and green +0.337: stored green already agrees with occlusion on those
pixels. Occlusion as a sign reference is calibrated: its slope correlation is
(+,+) on mat_hull (+0.67, +0.62), mat_engine (+0.75, +0.65) and Battleship (+0.32,
+0.31). But on this map the slope correlation, red then green, is +0.0494, +0.0088
in strict opposed 16-texel tiles (curl under -0.8, 352014 texels) and +0.4562,
+0.2664 in agreeing tiles, so the regression's agreement may come only from the
agreeing regions. That reading is untested. The geometry test gives no signal
here (+0.0027, +0.0188), against +0.665, +0.665 on Battleship, and base-luma slope
is not calibrated, so no verdict rests on either.

The reproducible symptom is the disagreement between the two scenes built from
one image. `MolniaFbx/source/MolniaAnimated.fbx.glb` holds the same 4096 by 4096
image flipped vertically, sourced from `MolniaFbx/textures/NormalDirectX.png`,
whose green the conversion inverts because `ExportBlenderScene.py` reads that stem
as a DirectX map. The mate has no `TANGENT`, so its derived frame runs -V, which
cancels that inversion in its render. Counting mask pixels whose largest channel
difference between the two scenes exceeds a floor, against both mate captures:

| `Molnia_normal.png` state | Floor 1 | Floor 2 | Floor 3 |
|---|---|---|---|
| Stored | 17991 to 18020 | 13092 to 13098 | 10766 to 10777 |
| Green inverted | 14524 to 14542 | 8134 to 8138 | 5462 to 5469 |
| Red inverted | 24254 to 24267 | 19174 to 19186 | 16538 to 16540 |
| Restored, rebuilt | 17992 to 17994 | 13090 to 13094 | 10766 to 10770 |

Those counts predate the authored frame. MolniaGltf's render has not moved since,
as above. The mate's moved by 3514 mask pixels at floor 2, 883 at floor 4 and 283
at floor 8, so recapture them rather than reuse them.

Any count criterion uses one noise floor. An earlier record compared 9078 with
13092, but 9078 was floor 1 and 13092 floor 2, and 9078 belongs to a shader state
that no longer exists, so it cannot be recaptured. The verdict changed with the
floor: at floor 1 the trial's 14530 is nearer 13092, and at floor 2 its 8135 is
nearer 9078. Repeat captures of one scene measured a floor of 1 on every pair.

The fall under green inversion is expected by construction: both scenes then
resolve the same Y term. It shows the two agree, not which one reads right.

## Design

The author's recommendation is two steps, in order.

First, confirm through `/external-diagnose-bug` whether the map's orientation is
wrong, on which regions, and in which channel, working per primitive because the
whole-map figure mixes signs. Take the main primitive first, since it fills the
mask, and start from why its curl and occlusion results disagree: split the
regression by tile class to test the reading above. Then take the primitive at
triangle 16914, whose sign is opposite. Trial edits stay in the worktree and are
reverted unless they confirm.

Then make the smallest fix inside `Molnia_normal.png`, keeping it 8-bit RGB at its
size with the unedited channels byte-identical: invert the confirmed channel over
the whole map if every confirmed region opposes the frame the same way, or only
over the confirmed regions if the primitives differ. Let the ordinary export
regenerate the tracked intermediates; the scene dirty check covers a referenced
texture. Record the edit in a new `MolniaGltf/PROVENANCE.md` in the
`SeaCreature2/PROVENANCE.md` form, since the scene is CC-BY-4.0.

If the map reads right as stored, leave it and report that. If the fix lies
outside it, for example in the mate's map or the file's `TANGENT` data, stop and
return for re-planning instead of widening scope.

## Critical files

- `Projects/BrokenEngineSandbox/Data/Models/MolniaGltf/textures/Molnia_normal.png`
- The same scene's `scene.gltf.Texture3.BC5_UNORM_BLOCK` and
  `scene.gltf.PreExport`, regenerated by export.
- `Projects/BrokenEngineSandbox/Data/Models/MolniaGltf/PROVENANCE.md`, new.
- `Engine/Data/Shaders/Model/AGENTS.md` `## Architecture Notes` — read for the
  frame rule; no change expected.

## In scope

- The root-cause investigation in `## Design`.
- `Molnia_normal.png`, which bounds the fix, and the tracked export outputs of
  MolniaGltf that the edit changes.
- A new `MolniaGltf/PROVENANCE.md` recording only the map edit.

## Out of scope

No change to `GetNormal` or any other shader, to the `TANGENT` packing, or to
DataPacker. No edit to `scene.gltf`, `scene.bin`, `license.txt` or any other
MolniaGltf texture. No edit to `MolniaFbx/`, or to any other scene or map. No new
fixture model, harness command or unit test.

## Acceptance criteria

- `/external-diagnose-bug`'s result states which regions read wrong and in which
  channel, or that the map reads right, and explains the primitive 0 disagreement
  between curl and occlusion. A count alone does not settle it.
- The recorded symptom no longer reproduces: with a fix, each primitive the
  diagnosis found opposed measures positive under the curl test, and each it
  found right keeps its table value to within 0.01.
- With a fix, the cross-scene count at floor 2, used for every count in this
  criterion, falls below the same count recaptured before the edit. For a
  whole-map green fix it lands closer to 8135 than to 13092.
- With no fix, the map is byte-identical and the result is reported.
- The diff touches only the files `## In scope` names.
- The scene exports through the ordinary DataPacker scene export via `/compile`,
  and its capture returns code `ok` with no `DEBUG_BREAK at` in the log.

Live setup, in order, as the observing runs recorded it:

1. `/compile` `-Target ModelViewer -Configuration Debug`. Before any map edit a
   Shared-mode build is correct data for the pre-edit captures. A Client build
   neither re-roots nor rebuilds the viewer.
2. Two `/agent-harness` ModelViewer captures per scene before any map edit, with
   `-ModelPath` set to `Models\MolniaGltf\scene.gltf` and
   `Models\MolniaFbx\source\MolniaAnimated.fbx.glb`. Each is one PNG at the
   viewer's fitted framing and exposure 1.0.
3. After the edit, re-run the step 1 build, which counts only when its summary
   shows `RunDataPacker=true` and the worktree `GameDataDirectory`, then take one
   capture per scene.

The model mask is the rasterized default-scene triangles, back faces culled and
the nearest surface winning, through `common::GetModelAlignment` and the orbit
pose in `Projects/ModelViewer/Source/Graphics/Camera.cpp`, eroded by 8-neighbours
to 65936 pixels. Both scenes' `.model.json` sidecars hold the same alignment and
project to that mask.

## Notes

Future implementation is Tier 2: trigger is one subsystem's runtime behavior, the
rendered output of one tracked model, with no format or layout change.

Reach is ModelViewer only, because no game or engine source names MolniaGltf.
Texture bytes change and formats do not, so there is no serialization change or
`kiVersion` bump. Model shading is outside PostRender state, so determinism/CRC,
replay, wire, threading and allocation are unaffected, and no shader or C++
changes.

The author's record is that the user directed this map to a follow-up Plan rather
than changing it with the frame work. No prerequisite Plan or mandatory
coordination constraint was identified.
