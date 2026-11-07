<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T14:08:20.730Z","dependsOn":[]} -->
# Check glTF normal mapping against the Khronos tangent test models

## Context

Broken Engine's glTF tangent-space normal mapping has never been checked
against a known-correct reference. Three tracked models were each judged to
need a green-channel inversion: SeaCreature2 and aim-9_missile in commit
`0f9524fab8b7f4c80beadf11d1a5b28ad0976c01`, and MolniaGltf in a diagnosis that
did not edit it. Three models needing the same flip suggests a possible
systematic pipeline cause. It is not proven. Per the user's rule, source art is
not edited to compensate for a convention, so if the pipeline has no bug there
is nothing to change.

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
  the spec frame: `GetNormal` uses the spec bitangent (`Model.frag:185`), and
  DataPacker passes w and every TEXCOORD set unchanged. That does not show an
  engine fault; whether the asset or the engine is wrong is unsettled, as for
  MolniaGltf's majority convention below.
- MolniaGltf: `textures/Molnia_normal.png` is unedited. Diagnosis found it is a
  vertical flip of `MolniaFbx/textures/NormalDirectX.png` with values unchanged,
  and that it mixes two green conventions, interleaved by feature. That is an
  artwork defect no loader rule can fix. The majority population, mostly noise
  and panel detail, encodes green along -V, which is glTF green-up. All 13
  primitives have h = +1 and TANGENT w = +1, so the authored bitangent runs +V.
  The engine's frame therefore reads that majority green-inverted. Whether that
  is the asset or the engine is unsettled.

The engine's frame, from code reading:

- `GetNormal` (`Engine/Data/Shaders/Model/Model.frag:140-192`) uses the
  authored tangent when the vertex has one, with
  B = sign(w) * cross(N, T) (`:158`, `:185`). That is the glTF spec's
  bitangent formula.
- Without an authored tangent it derives T = dP/du from screen derivatives
  (`:161-173`) and sets the bitangent sign to -h (`:179-181`), so B runs -V on
  both handedness signs.
- Where h = +1 and w = +1, as on aim-9_missile and MolniaGltf, the two paths
  disagree: authored B runs +V and derived B runs -V.
- Before `0f9524fa`, `GetNormal` used B = cross(N, T) from the derived T on
  every model. That also runs +V where h = +1.
- DataPacker packs TANGENT xyz transformed by the mesh world matrix, and w and
  every TEXCOORD set unchanged (`BuildVertices`,
  `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:166-182`). The
  normal texture's set and scale come from `GltfSceneDecoder.cpp:512-517`.
- Unverified candidate from reading: the glTF path never tests the node
  determinant, so a mirrored node (negative determinant) would keep w
  unchanged while flipping the frame's handedness. The Assimp path accounts
  for a negative determinant (`AssimpSceneDecoder.cpp:637`).

The reference is Sascha Willems' Vulkan-glTF-PBR (`SaschaWillems/Vulkan-glTF-PBR`,
commit `128db90`), checked out at `Documents\Vulkan-glTF-PBR` under the user
profile, with a built `build\bin\Release\Vulkan-glTF-PBR.exe`. The checkout
carries local uncommitted edits in `base/VulkanExampleBase.cpp`,
`src/CMakeLists.txt` and `src/main.cpp`: enabling `fillModeNonSolid`, a
debugger working directory, and a Debug-only stdio redirect. None touches the
tangent frame. Its frame differs from ours in ways that limit what it proves:

- It never reads TANGENT. The vertex input has no tangent attribute
  (`src/main.cpp:935-943`).
- `getNormal` (`data/shaders/material_pbr.frag:99-115`) derives
  T = normalize(q1 * st2.t - q2 * st1.t) without dividing by the UV
  determinant, so T follows the determinant's sign. It then sets
  B = -normalize(cross(N, T)).
- `data/shaders/pbr.vert` negates world-position Y (`locPos.y = -locPos.y`)
  but leaves the normal unchanged.
- Its `data/models` holds only Box and DamagedHelmet, and it has no
  downloader. Its README points to the Khronos sample repository.
- It loads a `.gltf` or `.glb` named on the command line (`src/main.cpp:577`).
  It resolves assets from `./../data/` relative to its working directory
  (`:120`), so that directory must sit directly under the checkout root, for
  example `build`.
- It has no screenshot command.

The test models are from `KhronosGroup/glTF-Sample-Assets`, under
`Models/<Name>/`, as `glTF-Binary/<Name>.glb`, with `README.md`, `LICENSE.md`
and a `screenshot` folder:

- NormalTangentTest (CC0 1.0) supplies no tangents.
- NormalTangentMirrorTest (CC-BY 4.0, (c) 2018 Analytical Graphics, Inc.)
  supplies tangents and mirrored UVs.
- Both READMEs give the pass criterion: all test blocks show similar
  reflections with a level horizon, and from the back the sky appears below
  the ground in the reflections.

NormalTangentTest therefore exercises our derived path, and
NormalTangentMirrorTest our authored path. Vulkan-glTF-PBR renders both
through its derived frame, so it cannot check our authored path directly.

Recorded by:

- Client: claude.
- Conversation session ID: 0c5e7172-8806-4c51-b8c9-e77443220b45.
- Worktree/branch UUID: 940d2f3f-e78d-4f15-83d7-4408222014bb.
- Session branch: claude/940d2f3f-e78d-4f15-83d7-4408222014bb.
- Worktree: `.claude\worktrees\BrokenEngine\940d2f3f-e78d-4f15-83d7-4408222014bb`.
- Landing ref: that session branch.

## Design

This Plan has an investigation portion: the root cause is not confirmed. The
author recommends these steps, in order.

1. Obtain the assets. Download both models' `glTF-Binary/<Name>.glb`,
   `screenshot` image, `README.md` and `LICENSE.md` from
   `KhronosGroup/glTF-Sample-Assets` `main` into a new, empty directory per
   model under `Temp/`, and record the sample repository commit used. They are
   never committed. The check runs once, and committing them would add
   third-party assets with their license obligations for no lasting use.
2. Read both frames. Confirm, from the GLBs' attributes, which test model
   carries TANGENT, whether it has negative-determinant nodes, and whether
   every material's `normalTexture` samples set 0 (`texCoord` 0 or absent). If
   any samples another set, stop and report for re-planning, because
   `Documents/Plans/Engine/GetNormalTangentFrameUvSet.md` then becomes a
   prerequisite. Compare each test block's expected frame in our `GetNormal`
   with Vulkan-glTF-PBR's `getNormal`. From that reading, record per block whether the reference's
   frame is valid there; its folded determinant sign and Y-negated positions
   are the suspects.
3. Render the reference. Tell the user first, because the reference opens a
   visible window. Run its Release executable with the GLB path as its argument
   and its working directory under the checkout root (for example `build`).
   Capture its default view of each model to a PNG under `Temp/` with any
   window capture.
4. Render ours. Copy each GLB, uncommitted, into
   `Projects/BrokenEngineSandbox/Data/Models/<Name>/`. ModelViewer shows only
   packed catalog scenes from a Data root, so the files must be there for the
   check. Then run `/compile -Target ModelViewer -Configuration Debug`. Its
   summary counts only when it shows `RunDataPacker=true` and the worktree
   `GameDataDirectory`. Capture each model through `/agent-harness`'s ModelViewer
   capture with `-ModelPath` set to the packed scene path the catalog shows.
   That capture takes no camera input, so each model is judged from its single
   default view. Delete each copied model folder by its exact path before
   landing, together with the export files DataPacker writes beside it.
5. Judge per block, with a scratch script under `Temp/`, following the
   harness image rule. The criterion is whether each test block's relief reads
   raised, its bumps lit consistently with the reference. The author recommends
   a per-block measure of lighting orientation rather than a whole-image
   difference, because the two renderers differ in environment, exposure, sun
   and framing. The README's horizon and back-view criterion is not used,
   because ModelViewer's capture camera is fixed straight down and back faces
   are culled (`Engine/Source/Graphics/Managers/DynamicPipelines.cpp:103`,
   `GltfSceneDecoder.cpp:470-473` warns on `doubleSided`), so `doubleSided`
   support stays out of scope. The truths, in order:
   - the Khronos screenshot;
   - Vulkan-glTF-PBR, on blocks where step 2 found its frame valid.
6. Act on the result:
   - If every block of both models passes, record that the engine already
     matches, and stop with no code or asset change.
   - If any block fails, confirm the root cause through
     `/external-diagnose-bug`, then make the smallest fix inside the region
     `## In scope` names. A fix outside that region, including the vertex-side
     tangent transform in `Engine/Data/Shaders/Model/ModelCommon.h:101`, stops
     for re-planning instead of widening scope.
7. After a pipeline fix only, re-check every model whose source art was flipped
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
  step 7.

## In scope

- The investigation in `## Design` steps 1-5. Its scratch files and
  uncommitted test-model copies never land.
- The fix region:
  - DataPacker glTF loading of TANGENT, TEXCOORD sets and the normal texture
    (the `BuildVertices` attribute reads, and the normal texture handling in
    `GltfSceneDecoder.cpp`);
  - `GetNormal` in `Model.frag`;
  - a `kiVersion` bump in `ExportScene.h` if packed scene data changes, with
    the `<scene>.MODEL` and `<scene>.PreExport` intermediates of every
    affected tracked scene that the ordinary export then regenerates.
- Step 7's restoration of aim-9_missile's supplied normal map, its regenerated
  intermediates, and its `PROVENANCE.md` inversion sentence.

## Out of scope

- Any edit to SeaCreature2, MolniaGltf, MolniaFbx or any other model's art.
- The Assimp path, `ModelCommon.h`, other shaders, packed material or vertex
  layouts, and the Vulkan-glTF-PBR checkout.
- `GetNormal`'s derivative UV-set selection, owned by
  `Documents/Plans/Engine/GetNormalTangentFrameUvSet.md`.
- Re-deciding aim-9_missile's height truth, which `aim-9_missile/PROVENANCE.md`
  records as raised slotted screw heads.
- Committing the test models, any new harness command or fixture, and unit
  tests.

## Acceptance criteria

- The result records the sample repository commit and, for each test model and
  block, the verdict from step 5 and the truth it was judged against.
- One of these holds:
  - Every block of both test models passes in ModelViewer with no code change,
    and the result says the engine already matches.
  - `/external-diagnose-bug` names the confirmed root cause inside the fix
    region, and after the fix every block of both test models passes. That
    includes bumps reading raised where Vulkan-glTF-PBR shows raised on blocks
    whose reference frame step 2 found valid. The recorded symptom no longer
    reproduces.
  - `/external-diagnose-bug` confirms the root cause outside the fix region,
    the result reports it for re-planning, and no code or asset changes.
  - Step 2 finds a `normalTexture` sampling a set other than 0, the result
    reports it for re-planning, and no code or asset changes.
- After a fix, aim-9_missile meets step 7: either its supplied map is restored
  byte-identical to commit `3ccd5352`, with `PROVENANCE.md` no longer
  recording an inversion, or the stop is reported.
- Every capture returns code `ok` with no `DEBUG_BREAK at` in its log.
- No test-model file, scratch file or Vulkan-glTF-PBR change is in the diff,
  and the diff touches only files `## In scope` names.

Live setup: the supplied evidence records no harness run of these test models.
Establish and record the setup in step 4. The closest recorded setup is the
one `Documents/Plans/Game/ConfirmAim9MissileNormalMapChannel.md` recorded
(last at `04b2648b^`):
`/compile -Target ModelViewer -Configuration Debug`, then one
ModelViewer capture with `-ModelPath` at the viewer's fitted framing and
exposure 1.0.

## Notes

The investigation alone is Tier 2. Its trigger is one subsystem's runtime
behavior, observed without a tracked change.

The fix tier depends on where the root cause lands:

- A fix confined to `GetNormal`, with or without step 7, is Tier 2, because it
  is one client shader's behavior with no layout change.
- A fix that changes packed vertex or material data in DataPacker is Tier 3,
  because its trigger is serialization: the packed scene contents change and
  `ExportScene.h` `kiVersion` bumps.

Model shading is outside PostRender state, so determinism/CRC, replay, wire,
threading and allocation are unaffected. A fix changes the look of every
normal-mapped model by design.

Step 7 judges aim-9_missile against the height truth
`aim-9_missile/PROVENANCE.md` records. `GetNormalTangentFrameUvSet.md` edits
other lines of the same function. It is not a prerequisite while both test
models sample their normal maps with set 0; step 2 stops for re-planning
otherwise.
