<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T14:08:20.730Z","dependsOn":["Documents/Plans/Engine/GetNormalTangentFrameUvSet.md"]} -->
# Check glTF normal mapping against the Khronos tangent test models

## Context

Three tracked models were each judged to need a green-channel inversion:
SeaCreature2 and aim-9_missile in commit
`0f9524fab8b7f4c80beadf11d1a5b28ad0976c01`, and MolniaGltf in a diagnosis that
did not edit it, which suggested a systematic pipeline cause. Per the user's
rule, source art is not edited to compensate for a convention, so if the
pipeline has no bug there is nothing to change.

A first run of this check, at primary `97fc2976`, found no pipeline defect:

- All 45 normal-mapped blocks of NormalTangentTest and NormalTangentMirrorTest
  best-matched their row's real-geometry block as rendered, under the measure
  in `## Design` step 4. Dielectric blocks scored NCC 0.987-0.999, metallic
  gold blocks 0.782-0.966. A positive control, a temporary green flip of the
  sampled normal in `GetNormal` captured before the session rebased onto
  `97fc2976`, failed the same pass rule on all 20 dielectric blocks it
  measured, NormalTangentMirrorTest blue and NormalTangentTest blue;
  NormalTangentTest pink was not measured. So the measure detects a flipped
  frame.
- NormalTangentTest carries no TANGENT, so it exercises the derived frame;
  every triangle has UV handedness h = -1. NormalTangentMirrorTest carries
  TANGENT with mirrored UVs, so it exercises the authored frame; its authored
  bitangent runs -V on every triangle across both handedness signs (w = +1
  where h = -1, w = -1 where h = +1). Neither model has a node transform, and
  both materials' `normalTexture` sample set 0.
- aim-9_missile and MolniaGltf read green-inverted because of the asset, not
  the engine: their own TANGENT w puts the authored bitangent along +V, mostly
  with h = +1 and w = +1 (on aim-9_missile, the bitangent runs +V on all 1700
  triangles, 1684 of them with h = +1 and w = +1), against the glTF convention
  the Khronos authored-frame model follows (-V), while their maps are green-up.
  The engine follows w as the glTF spec requires.

This Plan depends on `GetNormalTangentFrameUvSet.md` because that Plan changes
the code under test: it switches `GetNormal`'s tangent-frame derivatives to the
normal texture's UV set. The check reruns after that Plan lands, against the
code it leaves; the first run's result does not carry over.

Current state of the three models:

- SeaCreature2: its green inversion was reverted to the supplied art by the
  session that recorded this Plan. `SeaCreature2/PROVENANCE.md` no longer
  records an inversion.
- aim-9_missile: the green inversion stays. The supplied map is
  `textures/lambert1_normal.png` at commit `3ccd5352`. As of main commit
  `04b2648b`, `aim-9_missile/PROVENANCE.md` records the inversion as confirmed
  by the map's slotted screw heads: with red read along +U, each sampled slot
  that `textures/lambert1_metallicRoughness.png` marks lies below both halves of
  its head, so the supplied relief is raised, and only green opposed the scene's
  authored bitangent, which runs +V. The Plan that settled this,
  `Documents/Plans/Game/ConfirmAim9MissileNormalMapChannel.md` (last at
  `04b2648b^`), measured that
  bitangent as +V on 100.00% of corners, with UV handedness h = +1 on 1692 of
  1700 triangles. So the supplied map and its authored TANGENT w disagree under
  the spec frame, an asset fault per the first run above; the inversion stays
  unless a pipeline fix under `## Design` step 6 changes the frame.
- MolniaGltf: `textures/Molnia_normal.png` is unedited. Diagnosis found it is a
  vertical flip of `MolniaFbx/textures/NormalDirectX.png` with values unchanged,
  and that it mixes two green conventions, interleaved by feature. That is an
  artwork defect no loader rule can fix. The majority population, mostly noise
  and panel detail, encodes green along -V, which is glTF green-up. All 13
  primitives have h = +1 and TANGENT w = +1, so the authored bitangent runs +V.
  The engine's frame therefore reads that majority green-inverted, the same
  asset-side mismatch as aim-9_missile's.

The engine's frame at `8fa794fa`, from code reading:

- `GetNormal` (`Engine/Data/Shaders/Model/Model.frag:140-192`) uses the
  authored tangent when the vertex has one, with
  B = sign(w) * cross(N, T) (`:158`, `:185`). That is the glTF spec's
  bitangent formula.
- Without an authored tangent it derives T = dP/du from screen derivatives
  (`:161-173`) and sets the bitangent sign to -h (`:178-181`), so B runs -V on
  both handedness signs.
- Where h = +1 and w = +1, as on aim-9_missile and MolniaGltf, the two paths
  disagree: authored B runs +V and derived B runs -V.
- `BuildVertices`
  (`DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:133`) bakes a
  static export's normals with the inverse transpose of the mesh-world matrix
  (`:153-155`, `:172`) and TANGENT xyz with the matrix itself, and negates w
  under a negative-determinant node (`:180-192`); `LoadVertices` sets that
  mirrored flag, which also flips winding (`:399-400`). A skinned export keeps
  vertices mesh-local and w unchanged. Every TEXCOORD set passes unchanged.
  The normal texture's set and scale come from `GltfSceneDecoder.cpp:512-517`.
- The Assimp path packs no tangents and flips winding for a negative
  determinant (`AssimpSceneDecoder.cpp:638`).
- No tracked reference model exercises the derived frame at h = +1, a
  negative-determinant glTF node, or a normal `texCoord` other than 0.

The reference models are tracked in BrokenEngineSandbox, from
`KhronosGroup/glTF-Sample-Assets` commit
`8f492390ddc7a2ae221a1369ef560589cf8c70fb`; each folder under
`Projects/BrokenEngineSandbox/Data/Models/` holds the supplied files
byte-for-byte, `LICENSE.md`, `PROVENANCE.md` and the generated intermediates:

- `NormalTangentTest/` (CC0 1.0): no TANGENT, so the derived frame.
- `NormalTangentMirrorTest/` (CC-BY 4.0): supplied TANGENT and mirrored UVs,
  so the authored frame.
- `SciFiHelmet/` (CC0 1.0): one TANGENT material, a realistic normal-mapped
  sanity reference.
- `FlightHelmet/` (CC0 1.0): six TANGENT materials, a second sanity reference.
  Its `KHR_materials_transmission` (LensesMat) and double-sided HoseMat are
  unsupported, so its visor and hose differ from the Khronos screenshot by
  design.

Neither helmet has a node transform, and every material samples set 0.
DamagedHelmet is not usable: its CC-BY-NC terms. In each test model, every row
starts with a real-geometry block that the row's normal-mapped blocks must
match; that is the comparison the Khronos README and screenshot define as
passing.

Optional second truth: Sascha Willems' Vulkan-glTF-PBR
(`SaschaWillems/Vulkan-glTF-PBR`, commit `128db90`), checked out at
`Documents\Vulkan-glTF-PBR` under the user profile with a built
`build\bin\Release\Vulkan-glTF-PBR.exe` and three local edits that do not touch
the tangent frame (`base/VulkanExampleBase.cpp`, `src/CMakeLists.txt`,
`src/main.cpp`). The first run did not need it: the real-geometry comparison
decided every block. Its frame limits what it proves:

- It never reads TANGENT (`src/main.cpp:935-943`), so it cannot check our
  authored frame.
- `getNormal` (`data/shaders/material_pbr.frag:99-115`) derives
  T = normalize(q1 * st2.t - q2 * st1.t) without dividing by the UV
  determinant, so T follows the determinant's sign, then sets
  B = -normalize(cross(N, T)).
- `data/shaders/pbr.vert` negates world-position Y but not the normal.
- It loads the `.gltf` or `.glb` named on its command line
  (`src/main.cpp:577`) and resolves assets from `./../data/` relative to its
  working directory (`:120`), so run it from a directory directly under the
  checkout root, such as `build`. It has no screenshot command.

Recorded by:

- Client: claude.
- Conversation session ID: 0c5e7172-8806-4c51-b8c9-e77443220b45.
- Worktree/branch UUID: 940d2f3f-e78d-4f15-83d7-4408222014bb.
- Session branch: claude/940d2f3f-e78d-4f15-83d7-4408222014bb.
- Worktree: `.claude\worktrees\BrokenEngine\940d2f3f-e78d-4f15-83d7-4408222014bb`.
- Landing ref: that session branch.

## Design

This Plan has an investigation portion: whether the pipeline that
`GetNormalTangentFrameUvSet.md` leaves still matches the reference is not
confirmed. The author recommends these steps, in order.

1. Read the frame at the tested tip. Re-read `GetNormal`, `BuildVertices` and
   whatever `GetNormalTangentFrameUvSet.md` changed, note any `## Context`
   citation that moved, and record which frame each tracked reference model
   now exercises. The rerun covers only the four tracked models; the paths
   `## Context` lists as unexercised are recorded as not exercised, and no
   fixture is added for them.
2. Build. Run `/compile -Target ModelViewer -Configuration Debug`. Its summary
   counts only when it shows `RunDataPacker=true` and the worktree
   `GameDataDirectory`. `git status --short` over the four reference model
   folders must then be empty; any change there means the prerequisite left
   stale intermediates, so stop and report for re-planning.
3. Capture. Capture each model through `/agent-harness`'s ModelViewer capture
   with `-ModelPath` set to `Models\NormalTangentTest\NormalTangentTest.glb`,
   `Models\NormalTangentMirrorTest\NormalTangentMirrorTest.glb`,
   `Models\SciFiHelmet\SciFiHelmet.gltf` and
   `Models\FlightHelmet\FlightHelmet.gltf`. That capture takes no camera
   input, so each model is judged from its single default view.
4. Judge per block, with a scratch script under `Temp/`, following the
   harness image rule:
   - Locate each block. In the first run's 1280x800 captures the top-left
     corners were: NormalTangentTest, 57 px blocks, rows at y 172, 258, 344,
     431, 517, geometry/normal-mapped column pairs at x 404/473 (pink),
     577/646 (blue), 749/818 (gold); NormalTangentMirrorTest, 48 px blocks,
     rows at y 214, 288, 361, 435, 509, columns geometry, Normal, VMirror,
     UMirror at x 382, 441, 501, 563 (blue) and 672, 731, 789, 848 (gold).
     Re-measure if the framing differs.
   - For each normal-mapped block, take the luminance of a centred disc of
     radius one third of the block size and compute its normalized
     cross-correlation (NCC) with the same disc of its row's real-geometry
     block, for the block as rendered, mirrored in X, mirrored in Y and
     rotated 180 degrees, each the best over offsets of up to 3 px. A flipped
     bitangent or tangent makes a mirrored variant match best.
   - Pass rule: the best match is as rendered, and dielectric blocks
     (NormalTangentTest pink and blue, NormalTangentMirrorTest blue) score NCC
     at or above 0.94. Metallic gold blocks need only the best match as
     rendered, because their reflections lower NCC.
   - Positive control: rebuild with a temporary green flip of the sampled
     normal in `GetNormal` (`nXY.y = -nXY.y`), recapture both test models,
     confirm the pass rule fails every dielectric block, then revert the edit.
     In the first run 12 of 20 control blocks best-matched a mirrored variant
     and the other 8 fell below 0.94, so both conditions are needed.
   - The README's horizon and back-view criterion is not used, because
     ModelViewer's capture camera is fixed straight down and back faces are
     culled (`Engine/Source/Graphics/Managers/DynamicPipelines.cpp:103`,
     `GltfSceneDecoder.cpp:470-473` warns on `doubleSided`), so `doubleSided`
     support stays out of scope.
   - The helmets are informational: record whether their normal-mapped detail
     reads raised and lit consistently with the sun; they gate nothing beyond
     a clean capture.
   - Vulkan-glTF-PBR is optional, only for a block the real-geometry
     comparison leaves undecided. Because it never reads TANGENT, it judges
     only a NormalTangentTest block, and only after comparing that block's
     frame in `GetNormal` with its `getNormal` and `pbr.vert` as `## Context`
     describes them and recording that the two agree there. Tell the user
     first, because it opens a visible window; run its Release executable
     with the model path as its argument and capture its default view to a PNG under `Temp/` with any window capture.
5. Act on the result:
   - If every block of both models passes, record that the engine already
     matches, and stop with no code or asset change.
   - If any block fails, confirm the root cause through
     `/external-diagnose-bug`, then make the smallest fix inside the region
     `## In scope` names. A fix outside that region, including the vertex-side
     tangent transform in `Engine/Data/Shaders/Model/ModelCommon.h:101`, stops
     for re-planning instead of widening scope.
6. After a pipeline fix only, re-check every model whose source art was flipped
   to compensate. Today that is aim-9_missile. Capture it with its supplied map,
   `lambert1_normal.png` as of commit `3ccd5352`, under the fixed engine.
   - If the supplied map then renders the slotted screw heads raised, the
     height truth `aim-9_missile/PROVENANCE.md` records, restore the supplied
     map. Let the ordinary export regenerate the scene's
     tracked intermediates, and remove the inversion sentence from
     `aim-9_missile/PROVENANCE.md`.
   - If the current map is no longer the `0f9524fa` green inversion, or the
     re-check disagrees, stop and surface the result for re-planning.

## Critical files

- `Engine/Data/Shaders/Model/Model.frag`: `GetNormal`.
- `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp`:
  `BuildVertices` TANGENT and TEXCOORD reads.
- `DataPacker/Source/ExportJobs/Scene/GltfSceneDecoder.cpp`: normal texture
  material fields and normal texture format selection.
- `DataPacker/Source/ExportJobs/ExportScene.h`: `kiVersion`, only if the fix
  changes packed scene data.
- `Projects/BrokenEngineSandbox/Data/Models/aim-9_missile/textures/lambert1_normal.png`,
  `aim-9_missile/PROVENANCE.md`, and the scene's regenerated
  `scene.gltf.Texture2.BC5_UNORM_BLOCK` and `scene.gltf.PreExport`, only under
  step 6.
- Inputs: the four reference model folders under
  `Projects/BrokenEngineSandbox/Data/Models/`. Their supplied files stay
  unchanged; their generated intermediates change only by the regeneration a
  fix's `kiVersion` bump causes.

## In scope

- The investigation in `## Design` steps 1-4, including the positive
  control's temporary `GetNormal` edit, reverted before landing. Its scratch
  files never land.
- The fix region:
  - DataPacker glTF loading of TANGENT, TEXCOORD sets and the normal texture
    (the `BuildVertices` attribute reads, and the normal texture handling in
    `GltfSceneDecoder.cpp`);
  - `GetNormal` in `Model.frag`;
  - a `kiVersion` bump in `ExportScene.h` if packed scene data changes, with
    the `<scene>.MODEL` and `<scene>.PreExport` intermediates of every
    affected tracked scene that the ordinary export then regenerates.
- Step 6's restoration of aim-9_missile's supplied normal map, its regenerated
  intermediates, and its `PROVENANCE.md` inversion sentence.

## Out of scope

- Any edit to SeaCreature2, MolniaGltf, MolniaFbx or any other model's art.
- The Assimp path, `ModelCommon.h`, other shaders, packed material or vertex
  layouts, and the Vulkan-glTF-PBR checkout.
- `GetNormal`'s derivative UV-set selection, owned by
  `Documents/Plans/Engine/GetNormalTangentFrameUvSet.md`.
- Re-deciding aim-9_missile's height truth, which `aim-9_missile/PROVENANCE.md`
  records as raised slotted screw heads.
- Any new model, harness command or fixture, and unit tests.

## Acceptance criteria

- The result records the frame each tracked reference model exercised at the
  tested tip, the paths not exercised, and, for each test model and block, the
  step 4 best variant and NCC and the truth it was judged against, plus the
  positive control's per-block result.
- One of these holds:
  - Every block of both test models passes the step 4 rule in ModelViewer with
    no code change, and the result says the engine already matches.
  - `/external-diagnose-bug` names the confirmed root cause inside the fix
    region, and after the fix every block of both test models passes the step
    4 rule, including any block judged against Vulkan-glTF-PBR. The recorded
    symptom no longer reproduces.
  - `/external-diagnose-bug` confirms the root cause outside the fix region,
    the result reports it for re-planning, and no code or asset changes.
  - Step 2 finds a changed reference model folder, the result reports it for
    re-planning, and no code or asset changes.
- After a fix, aim-9_missile meets step 6: either its supplied map is restored
  byte-identical to commit `3ccd5352`, with `PROVENANCE.md` no longer
  recording an inversion, or the stop is reported.
- Every capture, including SciFiHelmet's and FlightHelmet's, returns code `ok`
  with no `DEBUG_BREAK at` in its log.
- No reference model file outside a fix's regenerated intermediates, no
  scratch file, no positive-control edit and no Vulkan-glTF-PBR change is in
  the diff, and the diff touches only files `## In scope` names.

Live setup, as the first run recorded it: `/compile -Target ModelViewer
-Configuration Debug` in Local data mode, then one `/agent-harness` ModelViewer
capture per model with the `-ModelPath` values in step 3, each returning code
`ok`, `windowVisible` false and a 1280x800 screenshot at the viewer's fitted
framing.

## Notes

The investigation alone is Tier 2. Its trigger is one subsystem's runtime
behavior, observed without a tracked change.

The fix tier depends on where the root cause lands:

- A fix confined to `GetNormal`, with or without step 6, is Tier 2, because it
  is one client shader's behavior with no layout change.
- A fix that changes packed vertex or material data in DataPacker is Tier 3,
  because its trigger is serialization: the packed scene contents change and
  `ExportScene.h` `kiVersion` bumps.

Model shading is outside PostRender state, so determinism/CRC, replay, wire,
threading and allocation are unaffected. A fix changes the look of every
normal-mapped model by design.

Step 6 judges aim-9_missile against the height truth
`aim-9_missile/PROVENANCE.md` records.
