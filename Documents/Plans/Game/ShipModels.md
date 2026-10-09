<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-09T00:05:45.634Z","dependsOn":["Documents/Plans/Game/FlightPlaneClimbing.md","Documents/Plans/Game/ShipTypeSplit.md"]} -->
# Give each ship type a fleet model and a random-spawn model

Tier 2 (`.agents/references/risk-tiers.md`): the change adds four source model assets and their DataPacker intermediates, and rewires the client-only model selection, scale, and orientation constants in the three `*Render.cpp` files. No CRC'd state, wire format, save, or replay format changes; the server is untouched. Line numbers cite baseline `c1420821` where a file exists there; `Documents/Plans/Game/ShipTypeSplit.md` (prerequisite) renames `Players/` to `Omnis/` and `Spaceships/` to `Fighters/`, adds `Battleships/`, and introduces the per-controller model constants this Plan fills in, so carry its names forward. Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other. Paths below `Frame/` mean `Projects/BrokenEngineSandbox/Source/Frame/`; `Models/` means `Projects/BrokenEngineSandbox/Data/Models/`.

## Context

`ShipTypeSplit` decision 17 gives every ship type two render pipelines, one per `ShipController`, but fills both with placeholders: fleet rows draw `data::kModelsspaceship2scenegltfCrc` (today's Player model, `PlayersRender.cpp:19-20`, scale `0.3667f`) and random rows draw `data::kModelsSpaceshipscenegltfCrc` (today's Spaceship model, `SpaceshipsRender.cpp:15-16`, scale `0.00175f`), with Battleships at three times both scales. So a fleet Fighter, a fleet Omni, and a fleet Battleship all look alike apart from size, and so do the three random types.

The user decided (this session, binding): each type gets two distinct models, one for human-fleet ships and one for random spawns, so the two are told apart at a glance. Sizes: Battleship large and battleship-like (lowest flight plane), Omni medium and omni-directional, Fighter small and including an airplane-like craft. The existing `spaceship2` (JCarvajal, CC-BY-4.0) is one of the two Omni models and the existing `Spaceship` (MatiasMyma, CC-BY-4.0) is one of the two Fighter models, so four models are new: one small airplane-like craft, one medium omni-directional craft, two large battleships.

Pipeline facts the design rests on (verified at baseline):

- Scene discovery takes only `.gltf` (`DataPacker/Source/ExportJobs/ExportScene.cpp:11`); a `.glb` is never discovered. Sketchfab's auto-converted download is `scene.gltf` + `scene.bin` + `textures/`, the shape of every existing `Models/` folder.
- Only the metallic-roughness workflow is accepted; a material carrying `KHR_materials_pbrSpecularGlossiness` asserts (`ExportScene.cpp:871-872`).
- `COLOR_0` is never read (`DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:121-129` reads POSITION, NORMAL, TEXCOORD_0-4, WEIGHTS_0, JOINTS_0 only). A material with no `baseColorTexture` renders with its `baseColorFactor` alone (`Common/DataFile.h:258` defaults `iColorTextureSet = -1`; `Engine/Data/Shaders/Model/Model.frag:199` gates the sample on it), so an untextured flat-colour model is fine; a model whose colour lives only in vertex colours is not.
- At most 64 textures and 128 materials per scene (`Common/DataFile.h:93-94`); one node-referenced skin (`DataPacker/Source/ExportJobs/AGENTS.md` `## Scene and Model`).
- Scene textures are encoded from the images tinygltf decodes (`ExportScene.cpp:498-503`), so `.png`, `.jpg`, and `.jpeg` references all work (`Models/aim-9_missile` uses `.jpeg`).
- Pre-export writes tracked `scene.gltf.MODEL`, `scene.gltf.PreExport`, and `scene.gltf.Texture<N>.<FORMAT>` intermediates beside the source (`DataPacker/Source/ExportJobs/AGENTS.md` `## Scene and Model`); `.gitattributes` already marks every such suffix binary. The generated symbol is the relative path with non-alphanumerics dropped: `Models/Spaceship/scene.gltf` is `data::kModelsSpaceshipscenegltfCrc`.
- Rendered size is `radius * kfModelScale` applied as a uniform scale to the model's native units (`SpaceshipsRender.cpp:162,172`; `PlayersRender.cpp:230,240`); orientation fixups are hard-coded per model (`SpaceshipsRender.cpp:94` `smatPreRotate = RotationX(PI/2) * RotationY(0) * RotationZ(PI/2)`; `PlayersRender.cpp:242-243` `RotationX(PI/2)`, `RotationY(0)`). Forward is `+X` before yaw (`common::RotationMatrixFromDirection(dir, {1,0,0,0})`, `SpaceshipsRender.cpp:174`).
- Each existing model folder carries Sketchfab's `license.txt` sidecar (`Models/spaceship2/license.txt`); `Projects/BrokenEngineSandbox/Data/AGENTS.md` requires one per asset.
- `Projects/BrokenEngineSandbox/Data` is inside the session worktree's sparse checkout (only `Engine/Data/Islands` and `Engine/Data/Textures` are excluded) and the repository has no Git LFS rules; the existing model folders already track 3-5 MB textures directly, so the new folders are tracked the same way.

## Decisions

Each decision is recorded as made; the alternatives are not open.

1. **The six slots.** License, author, and size come from each Sketchfab page as read on 2026-10-09; the exact credit line is the `license.txt` Sketchfab ships in the download.

   | Type | Controller | Folder under `Models/` | Model | Source | Author | License | Size |
   |---|---|---|---|---|---|---|---|
   | Fighter | `kFleet` | `JetFighter` (new) | Sci Fi Jet Fighter Plane | `https://sketchfab.com/3d-models/sci-fi-jet-fighter-plane-2956c1941f8a46b49b2b63f9737a7f99` | Matrix Rex (`https://sketchfab.com/matrixrex`) | CC-BY-4.0 | 2.6k triangles |
   | Fighter | `kRandomSpawn` | `Spaceship` (existing) | Practica - Texturizado con Substance Painter | `https://sketchfab.com/3d-models/practica-texturizado-con-substance-painter-8046a0c79e2a4bd5b82b57ae1e09645c` | MatiasMyma | CC-BY-4.0 | 67.9k triangles (in tree) |
   | Omni | `kFleet` | `spaceship2` (existing) | Spaceship | `https://sketchfab.com/3d-models/spaceship-00c7005a1ae74487874a9f518cc25d24` | JCarvajal | CC-BY-4.0 | 39.3k triangles (in tree) |
   | Omni | `kRandomSpawn` | `FlyingSaucer` (new) | Classic Flying Saucer - Retro UFO Spacecraft | `https://sketchfab.com/3d-models/classic-flying-saucer-retro-ufo-spacecraft-a7676df726364bf08de47b1599036335` | INGSOC1984 (`https://sketchfab.com/INGSOC1984`) | CC-BY-4.0 | 8.7k triangles |
   | Battleship | `kFleet` | `Battleship` (new) | Battleship | `https://sketchfab.com/3d-models/battleship-9282c849bb414ffb9bd04c931856a637` | Matthaiios (`https://sketchfab.com/Matthaiios`) | CC-BY-4.0 | 17.4k triangles |
   | Battleship | `kRandomSpawn` | `LowPolyBattleship` (new) | Low poly battleship | `https://sketchfab.com/3d-models/low-poly-battleship-8d5e7633b6084c5f947f7134c495a59b` | minehffd (`https://sketchfab.com/minehffd`) | CC-BY-4.0 | 19.3k triangles |

   Fleet-versus-random assignment is the trivial choice: the two existing models keep the controller they already draw (`spaceship2` was the human Player, `Spaceship` the random Spaceship), the airplane goes to the human fleet because it is the model the user named, and the Battleship with emissive detail goes to the fleet. No franchise-derived model (Star Wars, StarCraft) was chosen because the uploader's CC licence cannot clear the underlying IP. The generated symbols are `data::kModelsJetFighterscenegltfCrc`, `data::kModelsFlyingSaucerscenegltfCrc`, `data::kModelsBattleshipscenegltfCrc`, `data::kModelsLowPolyBattleshipscenegltfCrc`.

2. **Download is a user step; the agent never fetches model files.** Sketchfab downloads need a signed-in account. For each new folder the user downloads the **glTF** format from the source page and unzips it so the folder holds exactly `scene.gltf`, `scene.bin`, `textures/`, and `license.txt`, as the existing folders do. No decimation, re-export, or texture resizing: every chosen model is already under 20k triangles.

3. **Each downloaded `scene.gltf` is checked before DataPacker runs, and a failing model stops the Plan.** Read the JSON: `extensionsUsed` must not name `KHR_materials_pbrSpecularGlossiness`; no primitive `attributes` may carry `COLOR_0` unless every material of that scene also has a `baseColorTexture` or a non-white `baseColorFactor`; `images` at most 64, `materials` at most 128, `skins` at most one. A model that fails is not converted or substituted; the implementer returns the failure to main with the offending field, because choosing a replacement model is not a trivial choice.

4. **Scale from measured extent.** For each of the four new models, compute the model's native longest horizontal extent `L` from `scene.gltf`: the per-primitive POSITION accessor `min`/`max`, transformed by the node chain's `scale`/`matrix` down to the root (Sketchfab commonly bakes a unit conversion into a root node). Set `kfModelScale` so the rendered longest extent equals the type's collision diameter: `kfModelScale = 2 / L`, written as a literal with four significant digits and a comment naming `L`. Radii are `ShipTypeSplit`'s: `kfFighterRadius = 1.5f`, `kfOmniRadius = 1.1f`, `kfBattleshipRadius = 4.5f`. The two existing models keep their scales (`0.00175f` for `Spaceship`, `0.3667f` for `spaceship2`); the Battleship placeholders' tripled scales are deleted with the placeholders.

5. **Orientation per model.** Each `*Render.cpp` carries one pre-rotation matrix per model, selected with the model by `pController`. Start every new model from its type's existing fixup (Fighters and Battleships: `RotationX(PI/2) * RotationZ(PI/2)`; Omnis: `RotationX(PI/2)`), then correct by multiples of `PI/2` about Z (and `PI` about X if upside down) until a harness screenshot shows the hull upright and the nose pointing along the ship's direction of travel; record the final matrix as a `static` constant like `SpaceshipsRender.cpp:94`. Nothing else in the transform chain (death shrink, roll, the Fighter climb tilt, yaw, translation, rotation-acceleration tilt) changes.

6. **No new engine or DataPacker code.** The four assets go through the existing scene export unchanged; no format version moves.

## Design

### Asset folders

`Models/JetFighter/`, `Models/FlyingSaucer/`, `Models/Battleship/`, `Models/LowPolyBattleship/`, each holding the Sketchfab glTF download (`scene.gltf`, `scene.bin`, `textures/*`, `license.txt`) plus, after pre-export, the tracked `scene.gltf.MODEL`, `scene.gltf.PreExport`, and `scene.gltf.Texture<N>.<FORMAT>` files. All of them are committed; `.gitattributes` already classifies every suffix.

### DataPacker run

After the four folders are in place, run DataPacker for the sandbox through `/compile`'s Local generation build (the only authorized way to run it in a session worktree; `.agents/skills/compile/references/runtime-data-mode.md`). Acceptance is a clean run: four new scene jobs exported, no aggregate diagnostic, and `Data.h`/the models header carrying the four new symbols. Intermediates appear beside each `scene.gltf`; `git status` must show them untracked and they are added with the sources.

### Render wiring (`Frame/Collections/Fighters/FightersRender.cpp`, `Omnis/OmnisRender.cpp`, `Battleships/BattleshipsRender.cpp`)

`ShipTypeSplit` decision 17 leaves each file with two model CRCs, two scales, two dynamic buffers, two pipelines (`"<Type>Fleet"`, `"<Type>Random"`), and a `Render` that partitions rows by `pController`. `Documents/Plans/Game/FlightPlaneClimbing.md` (prerequisite, its decision 13) then inserts the client-only climb tilt `XMMatrixRotationY(-pfClimbTilts[i])` between roll and yaw in `FightersRender`. This Plan changes only the constants and adds the per-model pre-rotation, which takes the existing pre-rotation slot ahead of roll:

```cpp
// FightersRender.cpp
extern const common::crc_t kFighterFleetModel = data::kModelsJetFighterscenegltfCrc;
constexpr float kfFighterFleetModelScale = /* 2 / L, decision 4 */;
static const XMMATRIX smatFighterFleetPreRotate = /* decision 5 */;
extern const common::crc_t kFighterRandomModel = data::kModelsSpaceshipscenegltfCrc;
constexpr float kfFighterRandomModelScale = 0.00175f;
static const XMMATRIX smatFighterRandomPreRotate = XMMatrixRotationX(XM_PIDIV2) * XMMatrixRotationY(0.0f) * XMMatrixRotationZ(XM_PIDIV2);
```

Omnis mirror this with `kModelsspaceship2scenegltfCrc`/`0.3667f`/`RotationX(PI/2)` for `kFleet` and `kModelsFlyingSaucerscenegltfCrc` for `kRandomSpawn`; Battleships with `kModelsBattleshipscenegltfCrc` for `kFleet` and `kModelsLowPolyBattleshipscenegltfCrc` for `kRandomSpawn`, both scaled from `kfBattleshipRadius`. Inside each `Render`, the per-row `fSize` and pre-rotation come from the row's controller, the same branch that already selects the pipeline. The animation-clock lookup (`Players.cpp:516-518`, `Spaceships.cpp:263-265` at baseline, per-model after `ShipTypeSplit`) keeps using the row's own model CRC; none of the four new models carries an animation, so the clock stays inert for them.

If `ShipTypeSplit` landed any `#if 0` model block or any "three times the Fighter scale" placeholder expression, delete it here; the intended end state is six live constants and no dead alternatives.

### Attribution

Each new folder's `license.txt` is the one Sketchfab ships. `Projects/BrokenEngineSandbox/Data/LICENSE.md` already says every asset carries its own licence and needs no edit.

## Critical files

- `Projects/BrokenEngineSandbox/Data/Models/JetFighter/`, `FlyingSaucer/`, `Battleship/`, `LowPolyBattleship/` (new: sources, `license.txt`, and pre-export intermediates)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersRender.cpp`, `Omnis/OmnisRender.cpp`, `Battleships/BattleshipsRender.cpp`

## In scope

- The four new model folders of decision 1 with Sketchfab's `license.txt`, and their DataPacker intermediates.
- The decision 3 pre-checks on each downloaded `scene.gltf`.
- In the three `*Render.cpp` files: the model CRC, `kfModelScale`, and pre-rotation constants per controller (decisions 4 and 5), the per-row selection of scale and pre-rotation by `pController` in `Render` (the pre-rotation in the existing pre-rotation slot, ahead of roll and, in `FightersRender.cpp`, ahead of FlightPlaneClimbing's climb tilt), and deletion of any leftover placeholder reuse or `#if 0` model block.
- One DataPacker sandbox run and the client build and harness screenshots in `## Acceptance`.

## Out of scope

- Changing collision radii, pusher radii, explosion or blaster sizes, flight constants, or any CRC'd column; the rendered size follows the existing radius.
- Engine render code, shaders, `DataPacker` code, scene or texture export formats and versions, `.gitattributes`, Git LFS.
- Editing, decimating, retexturing, or re-exporting any model; converting a rejected model; substituting a different model for a rejected slot (returned to main instead).
- Server code, the HUD, the harness commands, and the existing `Spaceship`, `spaceship2`, and `aim-9_missile` folders.
- Per-flight-plane visuals, animations, hex-shield or wind-trail sizing for the new models.

## Risk triggers and invariants

- Client-only: every edited line is under `#if defined(BT_CLIENT)`; PostRender state and the CRC are untouched, so replay determinism cannot move.
- Each new scene must pass `ExportScene`'s metallic-roughness assert and the texture, material, and skin limits (decision 3) before DataPacker runs; a failing model is a stop, never a workaround.
- Generated CRC symbols are unique across the run (`DataPacker/Source/AGENTS.md` `## Design Patterns`); the four folder names of decision 1 collide with nothing in `Models/`.
- Pre-export intermediates are tracked beside their sources and committed with them, or the next session's pre-export re-runs and `git status` dirties (`DataPacker/Source/ExportJobs/AGENTS.md` `## Scene and Model`).
- Every new folder carries its licence sidecar (`Projects/BrokenEngineSandbox/Data/AGENTS.md`); only CC-BY-4.0 and CC0 licences are accepted, no NC, SA, or ND term.
- Each `Render`'s transform chain order is the one FlightPlaneClimbing leaves (Fighters: `scaling * preRotate * roll * climbTilt * yaw * translation`) and is unchanged here; only the per-model pre-rotation matrices and scale literals differ.
- No main-loop allocation beyond `ShipTypeSplit`'s two pipelines per type.

## Acceptance

| Criterion | Expected observation | Method |
|---|---|---|
| Pre-checks pass | Each of the four `scene.gltf` files has no `KHR_materials_pbrSpecularGlossiness`, no colour-only `COLOR_0` primitive, at most 64 images, 128 materials, 1 skin | Read the JSON |
| DataPacker export clean | The sandbox run reports four exported scene jobs and no failed job or aggregate diagnostic; four new `data::kModels*scenegltfCrc` symbols exist; intermediates exist beside each source | `/compile` Local generation build, then `git status` |
| Client builds | Client succeeds | `/compile` BrokenEngineSandbox client |
| Six distinct models | With a fleet containing one Fighter, one Omni, and one Battleship and random Fighters, Omnis, and Battleships in the same cell (`inject_payload` `SpawnShip` plus `edit_frame` of `controller` on copies), a screenshot shows six different hulls; the fleet and random ship of each type are told apart | `/agent-harness` screenshot, `describe_scene` |
| Orientation | Each ship's nose points along its `dir` from `query_collection` and the hull is upright | `/agent-harness` screenshot with the debug direction overlay |
| Size matches collision radius | Each ship's longest on-screen extent is within 25% of its type's collision diameter drawn by `DebugRender` (3.0 m Fighter, 2.2 m Omni, 9.0 m Battleship) | `/agent-harness` screenshot with `DebugRender` circles |
| No placeholders | A search of the three `*Render.cpp` files finds no `#if 0`, no shared model CRC between two controllers, and no `* 3.0f` Battleship scale | Search |
