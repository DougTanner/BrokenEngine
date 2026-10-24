<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T19:30:56.609Z","dependsOn":[]} -->
# Import Sketchfab "Original format" scenes whose textures are present but not linked

## Context

The user asked for this Plan in these words: "Write a plan file with these
problems for a future session to fix, name the downloads files (I will keep them
around)." The same session recorded that the user wants the model importer to
handle these formats. Ten Sketchfab "Original format" downloads (all
CC-BY-4.0) fail to import with today's DataPacker scene path, or import with
their textures missing, although every needed image is inside the zip. Only
inputs 3, 5 and 6 are integrated separately, from their Sketchfab glTF
downloads (`Models/SpaceBattleshipAquamarine/`, `Models/BattleshipJesterz86/`,
`Models/SpaceFighter/`); the other seven models' glTF downloads were not kept
in the repository. This Plan covers only the non-glTF path (Assimp FBX/OBJ and
`.blend`).

Line numbers cite baseline `bae72d574`.

### Test inputs

The user keeps these files. Identity comes from the Sketchfab API through each
file's download URL. Facts below were measured from the zip listings, the
binary FBX property strings, the MTL text, and the `.blend` headers.

| # | Zip | Model | Measured contents and problem |
|---|---|---|---|
| 1 | `C:\Users\dougt\Downloads\space-ship.zip` | "space ship" by yanix, 949 faces, https://sketchfab.com/3d-models/space-ship-356a3acb00164c698d657146caa5ebf3 | Nested `source/sp1.zip.zip` holds `sp_1.blend` (header `BLENDER-v276`) and `sp_1_{diff,emit,nor,spec}.JPG`. The blend's images name `//sp_1_*.JPG` (siblings) and a missing `//..\uow_battleship_by_evilchaotic-d3igrjl.jpg`. The outer `textures/sp_1_*.jpeg` are re-encoded copies (different CRCs). Specular-workflow `_spec` map. A pre-2.8 file; Blender 2.8+ dropped legacy material texture slots, so the converted GLB is expected to carry no textures. |
| 2 | `C:\Users\dougt\Downloads\spaceship-eav-2-crab.zip` | SPACESHIP EAV 2 "CRAB" by shilma513, 75k faces, https://sketchfab.com/3d-models/spaceship-eav-2-crab-f4bfa996007e4a6fbd0f31fa807230ce | `source/1.fbx` (Maya, FBX SDK 2019.2, Lambert), materials `Bochka1`, `Bochka2`, `DECALS_MAP`, `GLASS_MAP`, `UV_1`, `UV_2`, `UV_3`, and no texture links. 30 PNGs (468 MB) `textures/<Set>_{Base_Color,Metallic,Roughness,Normal_OpenGL,Emissive,Opacity}.png`, with sets `Decals` and `Glass` differing in case and missing the `_MAP` part of the material name. |
| 3 | `C:\Users\dougt\Downloads\space-battleship-aquamarine.zip` | Space Battleship Aquamarine by kirikom9000, 113k faces, https://sketchfab.com/3d-models/space-battleship-aquamarine-c508be042d9a4d358359623f6be34bc3 | `source/HSMS_AquaMarine.fbx` (Maya, FBX SDK 2020.0.1, Lambert), materials `lambert1` and `mat_{cannonrail,deck,engine,hull,miraclecannon,sexybeam,superstructure}`. Only `DiffuseColor` links, by the foreign paths `..\images\aquamarine\…` and `C:/Users/kirik/…`. 43 images hold AO/BaseColor/Emissive/Metallic/Normal/Roughness for every material. Two linked base colors exist only under truncated names (`…_miraclecannon_BaseColo.png`, `…_superstructure_BaseCol.png`), and suffixes `Roughnes`, `Roughne`, `Metalli`, `Emissiv` are truncated too. |
| 4 | `C:\Users\dougt\Downloads\spaceship (1).zip` | "spaceship" by easyidpyj, 42.8k faces, https://sketchfab.com/3d-models/spaceship-781e531c00d94c9e89dcc8ad9b967d87 | Nested `source/model.zip` holds `playship2F.FBX` (FBX SDK 2018.1, Phong) with materials `body` and `parts` and no texture links. Its upper-case extension is not discovered. Loose `textures/{body,parts}_{albedo,AO,emissive,metallic,normal,roughness}`. |
| 5 | `C:\Users\dougt\Downloads\battleship.zip` | Battleship by Jesterz86, 62k faces, https://sketchfab.com/3d-models/battleship-0fef17ea0320415c920f9d793c02bf2b | `source/Battleship_finish.fbx` (Blender 3.1 FBX IO, Phong), material `TT_checker_4096x4096_UV_GRID`. It links `EmissiveColor`, `NormalMap`, `ReflectionFactor` and `ShininessExponent` to `E:\Blender Modelle\Spaceship3\Battleship_{Emissive,Normal_OpenGL,Metallic,Roughness}.png`, all present by file name. `Battleship_Base_color.png` and `Battleship_Mixed_AO.png` are present but not linked. |
| 6 | `C:\Users\dougt\Downloads\space-fighter.zip` | Space Fighter by comrade1280, 17.7k faces, https://sketchfab.com/3d-models/space-fighter-e766136d4871441289d37d44a4bbcd3b | `source/space fighter.FBX.fbx` (3ds Max, FBX SDK 2013.1, Phong), materials `15 - Default` and `gun`, and no texture links. Loose `15_-_Default_{Base_Color,Emissive,Height,Normal_DirectX,Roughness}.png`, `gun_{Base_Color,Metallic,Normal_DirectX}.png`, `internal_ground_ao_texture.jpeg`. |
| 7 | `C:\Users\dougt\Downloads\v-t-f-003-space-cruiser.zip` | V T. F-003 Space cruiser by schmok, 93.8k faces, https://sketchfab.com/3d-models/v-t-f-003-space-cruiser-0bd5ef0554ae49a596dc4712bf58a9e9 | `source/vt_f003.fbx` (Blender FBX IO, Phong), 8 materials `Material`, `Material.001`–`Material.007`, and no texture links. Loose `New_Graph_{basecolor,metallic,normal,roughness}.png`, plus `MetalCorrugatedIronSheet002_NRM_3K_SPECULA.jpg` and `texture_spaceship_01_by_tiagocomicart-d6ra.jpg` (both truncated). No material name matches a texture name. |
| 8 | `C:\Users\dougt\Downloads\space-ship (1).zip` | Space Ship by comrade1280, 12.9k faces, https://sketchfab.com/3d-models/space-ship-63ce372c1aa843e98bf1548109e055d8 | `source/space ship.FBX.fbx` (3ds Max, FBX SDK 2015.1, Phong), material `03 - Default`. It links `DiffuseColor` to `1\colour.jpg` and `NormalMap` to `1\Normals.jpg`; both are present as `textures/colour.jpg` and `textures/Normals.jpg`. |
| 9 | `C:\Users\dougt\Downloads\spaceship.zip` | SpaceShip by JazOone, 22.6k faces, https://sketchfab.com/3d-models/spaceship-6164a883f57f4f13938c3c5999bc0e1f | Nested `source/SpaceShip.zip` holds three scene files and a nested `Texture.zip`. `SpaceShip.blend` (header `BLENDER-v304`) names images at `//..\Texture\StarShip2\{Black and Blue\,Black and Red\,}Material.001_*.jpg`. `SpaceShip.fbx` (Blender 3.4.1, Phong, material `Material.003`) links `DiffuseColor`, `EmissiveColor`, `NormalMap`, `ReflectionFactor` and `ShininessExponent` at `C:/Users/dovgo/…`. `StarShip2.obj`/`.mtl` has `illum 2`, `Ks 0.5`, and `map_Kd`/`map_Ns`/`map_refl`/`map_Ke`/`map_Bump` at the same foreign paths. `Texture.zip`'s root `Material.001_*.jpg` files are byte-identical to the outer `textures/`. Its `Black and Blue/` and `Black and Red/` folders hold color variants of Base_color and Emissive. `Mixed_AO` is not linked. |
| 10 | `C:\Users\dougt\Downloads\max-delta-dl-spaceship.zip` | MAX Delta DL Spaceship by miru302, 6.4k faces, https://sketchfab.com/3d-models/max-delta-dl-spaceship-364b8b1821164c9c83c9102c235dc331 | `source/Final.fbx` (3ds Max, FBX SDK 2016.0, Phong), material `Material #482`, and no texture links. Loose `Final_Material__482_{BaseColor,Normal}.png` and `Material__482_{AO,metallic,roughness}.jpg`. |

None of the ten zips contains a `license.txt`. The same models' glTF download
zips, which the user also keeps in `C:\Users\dougt\Downloads`, do.
Across all ten zips the longest file stem is exactly 42 characters, and 10 stems
have that length, so Sketchfab truncates stems to 42 characters.

### Verified code facts

- Scene discovery compares extensions case-sensitively
  (`DataPacker/Source/ExportJobs/ExportScene.cpp:10-14`, `:24`, `:37`), so
  `playship2F.FBX` is not discovered.
- `LoadAssimpImage` resolves a non-embedded texture only as
  `scene folder / referenced path`
  (`DataPacker/Source/ExportJobs/Scene/AssimpSceneDecoder.cpp:101-109`). An
  absolute or foreign relative path fails, and the scene throws "Assimp failed
  to decode image" (`:111-114`).
- `LoadAssimpMaterial` accepts only the diffuse/base-color, normal, emissive,
  AO/lightmap and combined glTF metallic-roughness texture types. Any other
  linked type is rejected (`:226-233`). Assimp's FBX importer maps
  `ReflectionFactor` to `aiTextureType_METALNESS` and `ShininessExponent` to
  `aiTextureType_SHININESS`
  (`ThirdParty/assimp/code/AssetLib/FBX/FBXConverter.cpp:2109-2124`). Its OBJ
  importer maps `map_bump` to `HEIGHT`, `map_refl` to `REFLECTION`, and `map_Ns`
  to `SHININESS` (`ThirdParty/assimp/code/AssetLib/Obj/ObjFileImporter.cpp:682`,
  `:708`, `:732`). Inputs 5 and 9 link Blender's metallic and roughness images
  through exactly these types.
- A Phong material with nonzero specular color and strength is rejected
  (`AssimpSceneDecoder.cpp:208-214`), as is a nonzero `$mat.reflectivity`
  (`:237-242`). Assimp sets Phong only for FBX `ShadingModel` "phong"
  (`FBXConverter.cpp:1793-1797`) and stores FBX `SpecularColor`/`SpecularFactor`
  and `ReflectionFactor` as those keys (`:2275-2341`). Seven FBX inputs (4, 5,
  6, 7, 8, 9 and 10) are Phong. Inputs 5, 6, 7, 8 and 10 were seen carrying
  `SpecularColor`/`SpecularFactor`, and the OBJ has `Ks 0.5`. Their values were
  not decoded, so whether each one trips these checks is for the fix session's
  first run to record.
  `ExportJobs/AGENTS.md` `## Scene and Model` states this rejection as policy.
- The engine reads the physical-descriptor texture as glTF metallic-roughness:
  roughness from G and metallic from B, each multiplied by its factor
  (`Engine/Data/Shaders/Model/Model.frag:205-213`). The Assimp decoder starts
  the metallic factor at 0 (`AssimpSceneDecoder.cpp:184`), and Assimp derives an
  FBX roughness factor from `ShininessExponent` (`FBXConverter.cpp:2288-2294`).
- `ExportBlenderScene.py` `record_image` raises "Unreadable consumed Blender
  image" for a consumed image whose file is missing
  (`DataPacker/Scripts/ExportBlenderScene.py:25-36`). It never relocates a
  missing file.
- The Assimp scene dependency sidecar (format `"version": 1`,
  `ExportScene.cpp:213-330`) records only files actually opened or read. It is
  checked only for Assimp scenes (`:54`). Dependencies must lie inside an input
  root (`:322-330`).
- `ExportTexture::Handles` claims every loose `.png`/`.tga`/`.jpg` under the
  project data root as a standalone texture (case-sensitive,
  `ExportTexture.cpp:13-15`). Existing glTF model folders' `textures/` are
  already exported this way.

## Design

All choices below are the author's recommendations with their rationale. None is
user-approved yet; the claim session's plan review confirms them with the user.

1. **Import layout (manual step, no DataPacker zip support).** Each test input
   becomes `Projects/BrokenEngineSandbox/Data/Models/<Name>Original/` (the
   suffix keeps inputs 3, 5 and 6 apart from their glTF folders) holding:
   - its scene file at the folder root, extracted from `source/` or from the
     nested scene zip;
   - Sketchfab's `textures/` folder;
   - a `license.txt` in the existing Sketchfab format
     (`Models/Spaceship/license.txt`), copied from the same model's glTF
     download zip or written from the identity in the table above.

   Two inputs need a different source for their images:
   - Input 1 uses the nested zip's original `sp_1_*.JPG` beside the blend
     instead of the re-encoded `textures/` copies.
   - Input 9 omits `Texture.zip`, which duplicates `textures/` apart from the
     color variants. Its three scene files go in three folders
     (`…Fbx`, `…Obj`, `…Blend`), each with its own `textures/`, so each path
     is exercised once. Discovery would otherwise export all three as separate
     models.

   Rationale: DataPacker never reads archives. Extracting the zips by hand
   keeps the change in the scene decoder.
2. **Case-insensitive scene extensions** in every extension comparison of
   `ExportScene.cpp`: `IsAssimpScene`, `Handles` with its `.glb` sibling
   suppression, the constructor's `.blend` checks, and the `.glb` binary checks
   in `ReadExternalUris` and `Export`. The same applies to every extension
   comparison of `AssimpSceneDecoder.cpp`: the `AssimpSceneDecoder`
   constructor's `aiProcess_FlipUVs` check and FBX centimeter scale, and
   `LoadAssimpMaterial`'s `.ply` two-sided `ASSERT`. Otherwise an accepted
   `playship2F.FBX` would skip the UV flip and import at scale 1.0.
3. **Linked-texture fallback by file name** in `LoadAssimpImage`. This applies
   only when `scene folder / referenced path` does not exist. Search the scene
   folder tree for a candidate image (`.png`, `.jpg`, `.jpeg`, `.tga`,
   case-insensitive) in this order:
   1. the same file name;
   2. the same stem with any image extension;
   3. a 42-character stem equal to the first 42 characters of the wanted stem.

   Matching is case-insensitive and accepts both `\` and `/` in the reference.
   If more than one candidate matches at the first tier that matches, fail with
   the candidate list rather than guess.
4. **Classify each texture by its file-name channel suffix** wherever the
   Assimp type is unsupported or absent. The stem's case-insensitive ending,
   with underscores ignored, names one channel:
   - base color: `basecolor`, `albedo`, `diffuse`, `diff`, `colour`, `color`;
   - normal: `normalopengl`, `normaldirectx`, `normal`, `normals`, `nor`;
   - metallic: `metallic`, `metalness`;
   - roughness: `roughness`;
   - occlusion: `mixedao`, `ao`;
   - emissive: `emissive`, `emit`, `glow`;
   - opacity: `opacity`;
   - ignored with a warning: `height`, `specular`, `spec`.

   A 42-character stem may end in a proper prefix (at least 3 characters) of
   one of those words. The stem before the suffix is the texture set name. A
   linked `METALNESS`, `REFLECTION`, `SHININESS`, `DIFFUSE_ROUGHNESS` or
   `HEIGHT` texture whose name classifies as a supported channel is used as that
   channel. A linked texture of an unsupported type that does not classify stays
   rejected as today.
5. **Loose-set inference** fills only channels a material does not link. A
   material's set is chosen as follows, and channels merge across every matched
   set:
   1. the set of any texture the material already links;
   2. otherwise every set whose name equals the material name, ends with `_`
      plus the name, or is a prefix of the name followed by `_`. Names are
      compared case-insensitively with every non-alphanumeric character read
      as `_`.
   3. otherwise, when the folder holds exactly one set with a supported channel
      and no material matched under rules 1 or 2, that set applies to every
      material.

   One channel supplied by two matched sets fails the scene. Unused candidates
   are logged as a warning.

   Predicted matches: input 2 by name (`DECALS_MAP` and `Decals`); input 3 by
   its links; input 4 by name; input 5 by its links; input 6 by name
   (`15 - Default` and `15_-_Default`); input 7 by the single set; input 9 by
   its links (adds `Mixed_AO`); input 10 by name (`Material__482` exact, and
   `Final_Material__482` ends with it).
6. **Channel conversion** happens in the decoded `SceneImage` pixels. All
   inputs and outputs are RGBA8 images.
   - Metallic and roughness images combine into one image: G is roughness R, B
     is metallic R, and R and A are 255. A missing half is 255. Mismatched
     dimensions fail the scene.
   - When a texture supplies metallic or roughness, that factor becomes 1.0.
   - A `normaldirectx` image has its green channel inverted.
   - Opacity R goes into base-color alpha, using the decoder's existing
     opacity alpha mode (`AssimpSceneDecoder.cpp:199-202`).
   - A synthesized image's identity names all of its inputs, so deduplication
     in `LoadAssimpTexture` stays correct.
7. **Legacy Phong specular and reflectivity are ignored with a per-material
   warning** instead of rejected. They have no metallic-roughness equivalent.
   The user's goal of importing these files outranks the documented rejection
   policy, and `ExportJobs/AGENTS.md` changes to match. Before changing code,
   the fix session runs DataPacker on every extracted input and records each
   rejection. Any rejection beyond the ones this Plan names is surfaced for
   re-planning, not relaxed.
8. **Dependency tracking.** Inference makes the candidate image list an
   input, so the scene dependency sidecar also records the sorted
   input-root-relative paths of every scanned candidate. Adding, removing or
   renaming one then re-runs pre-export. The sidecar `"version"` goes to 2 and
   `ExportScene::kiVersion` is bumped for the decoder's changed meaning.
9. **`.blend` path.** After `open_mainfile`, `ExportBlenderScene.py` runs
   Blender's `bpy.ops.file.find_missing_files` over the source folder. This
   relinks missing images by file name in memory, and the source is never
   saved. The script content is already part of the conversion marker, so
   `kiBlenderExportVersion` need not change. If the converted GLB of input 1
   carries no textures, as expected, rules 4–6 and the sidecar of rule 8 also
   apply to `.blend`-derived scenes. They never apply to plain glTF/GLB
   sources. If it does carry them, that extension is dropped.

## Critical files

- `DataPacker/Source/ExportJobs/ExportScene.cpp`, `ExportScene.h`
- `DataPacker/Source/ExportJobs/Scene/AssimpSceneDecoder.cpp`
- `DataPacker/Source/ExportJobs/Scene/GltfSceneDecoder.cpp`,
  `GltfSceneDecoder.h` (`.blend`-derived scenes under Design 9 only)
- `DataPacker/Source/ExportJobs/Scene/SceneTextureSets.h`,
  `SceneTextureSets.cpp` (new; recommended home for rules 3–6, shared by the
  Assimp and `.blend` paths), plus DataPacker project membership through
  `/update-vcxproj`
- `DataPacker/Scripts/ExportBlenderScene.py`
- `DataPacker/Source/ExportJobs/AGENTS.md` `## Scene and Model`

## In scope

- Design 2's extension comparisons in `ExportScene.cpp`: `IsAssimpScene`
  (`:10-14`), `ExportScene::Handles` (`:24`) and its `.glb` sibling
  suppression (`:37`), the `ExportScene` constructor's `.blend` comparisons
  (`:17-18`), and the `.glb` binary checks in `ReadExternalUris` (`:101`) and
  `ExportScene::Export` (`:468`).
- Design 2's extension comparisons in `AssimpSceneDecoder.cpp`: the
  `AssimpSceneDecoder` constructor's `aiProcess_FlipUVs` check (`:656`) and
  FBX `fMeters` scale (`:709`), and `LoadAssimpMaterial`'s `.ply` two-sided
  `ASSERT` (`:225`).
- `LoadAssimpImage`, `LoadAssimpTexture` and `LoadAssimpMaterial`: name
  fallback, channel classification, inference, conversion, factor handling,
  and the Phong specular and reflectivity checks.
- The new `SceneTextureSets.h`/`.cpp` holding rules 3–6, shared by the Assimp
  and `.blend`-derived paths, and its DataPacker project membership.
- `ExportScene::CaptureSceneDependencies`, `SceneDependenciesDirty`, and
  `kiVersion`.
- Only while Design 9's condition holds (input 1's converted GLB carries no
  textures), the `.blend`-derived GLB path:
  - `GltfSceneDecoder`'s constructor (`GltfSceneDecoder.cpp:280-299`), adding
    inferred and synthesized images and textures;
    `GltfSceneDecoder::DecodeTextureFormats` (`:308-315`), giving them formats;
    and `GltfSceneDecoder::DecodeMaterialShaderDatas` (`:423-511`), filling
    unlinked channels and texture-supplied factors;
  - the `GltfSceneDecoder` construction in `ExportScene::Export`
    (`ExportScene.cpp:468`), telling the decoder the scene is `.blend`-derived
    when `mBlenderSourcePath` is set;
  - the scene dependency sidecar sites gated by `IsAssimpScene`: the
    `CheckDirty` check (`:54`), the write in `UpdateCacheMetadata`
    (`:911-914`), and the removal in `CleanupOnFailure` (`:932-936`). In
    `Export`, the Assimp branch's marker removal and capture (`:453-457`) stay
    Assimp-only; the `.blend`-derived path gains its own marker removal and
    capture after the `GltfSceneDecoder` construction (`:468`).
- `ExportBlenderScene.py` `main`, adding the missing-file search.
- `ExportJobs/AGENTS.md` `## Scene and Model` wording for the above.
- Temporary test folders under `Projects/BrokenEngineSandbox/Data/Models/` as
  Design 1 describes. The recommendation is to remove them before landing:
  inputs 3, 5 and 6 already ship as glTF folders, the other seven models were
  not kept in the repository, and a later fix session re-extracts its test
  folders from the zips the user keeps in `C:\Users\dougt\Downloads` (all ten
  original-format zips and the glTF zips). The user confirms whether to keep
  any.

## Out of scope

- Reading zip archives in DataPacker.
- Decimation or texture resizing. Inputs 2, 3 and 7 are 75k–113k faces, and
  input 2 has 468 MB of PNGs.
- `ExportTexture` discovery of loose images.
- Decoding of plain glTF/GLB sources, meaning any not converted from a
  `.blend`.
- Game model selection: the `k<Type>FleetModel` and `k<Type>RandomModel`
  constants in the sandbox `FightersRender.cpp`, `OmnisRender.cpp` and
  `BattleshipsRender.cpp`.
- Converting a specular map to metallic or roughness.
- Height or parallax support.
- Changes under `ThirdParty/`.

## Risk tier and invariants

Tier 3. The trigger is serialization: the scene dependency sidecar format
changes (version 1 to 2). The change also bumps the scene export version.
Single subsystem (DataPacker). No runtime, CRC, wire, save or replay exposure,
and the packed scene payload layout is unchanged.

Invariants:
- Opaque source images stay trust-boundary inputs under
  `DataPacker/Source/AGENTS.md`.
- Every dependency stays input-root-relative.
- Failures stay per-asset through the existing aggregate.
- Resolution is deterministic: ties fail instead of picking.
- A clean warm run exports nothing.

`BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT` must still stop before texture encoding.

## Acceptance criteria

- With the Design 1 folders in place, a `/compile` Local generation build
  (ModelViewer or BrokenEngineSandbox) runs DataPacker. Every test scene exports
  without a failure aggregate:
  - inputs 2–8 and 10 through Assimp;
  - input 9 as three scenes;
  - input 1 through Blender 5.2.
- For each test scene, a ModelViewer screenshot taken through `/agent-harness`
  shows the inferred or linked base color, normal and emissive detail. A
  `normaldirectx` input does not show inverted green lighting. The supplied
  evidence records no harness run; the setup is the build above, then selecting
  each scene in the ModelViewer catalog.
- An immediately repeated DataPacker run exports nothing. Adding then removing
  an unrelated image in one test folder re-runs only that scene's pre-export.
- The DataPacker log lists the expected warnings: ignored specular and
  reflectivity, `Height` and `spec` maps, and unused
  `internal_ground_ao_texture.jpeg`.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  triggered row passing.

## Notes

- Correction to the originating evidence: input 3's two "absent" base colors
  are present under 42-character truncated names. The suffix and fallback
  tables above are recommendations for the user to confirm, not user
  decisions.
- Origin: client claude; conversation session ID
  5dcbdc70-8ebe-4339-a698-b4d8b614d50e; worktree/branch UUID
  f524889d-98ac-40da-8484-00ea2c8f9455; session branch
  `claude/f524889d-98ac-40da-8484-00ea2c8f9455`; worktree
  `.claude\worktrees\BrokenEngine\f524889d-98ac-40da-8484-00ea2c8f9455`.
