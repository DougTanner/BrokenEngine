# Model-Shaped Hitboxes

Revisit When: the game adds ships whose top-down shape is far from round (long, wide, or winged) and circle hits visibly miss or overreach.

## Context

Every entity-to-entity hit today is a sphere test with one radius per collision layer:

- `CollisionLayer` (`Engine/Source/Frame/Collision.h`) carries a single `fRadius` shared by every object in the layer; there is no per-object radius and no facing.
- The broad phase buckets objects into an 8x8 zone grid (`kiCollisionZonesX`/`kiCollisionZonesY`) padded by that radius (`Collision::CalculateZoneRange`, `Collision::CalculateObjectZoneRange`).
- The narrow phase (`TestAndCollectPair` in `Engine/Source/Frame/Collision.cpp`) runs `SweptSphereTest` for swept pairs and otherwise a 3D distance check (`XMVector3LengthSq`) against the summed radii, then places the contact point along the center line.
- Sandbox radii are constants: `kfPlayerRadius` (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h`), `kfSpaceshipRadius` (`Spaceships/Spaceships.h`), and the blaster and missile collision radii bound in `BlastersUpdate.cpp` and `MissilesUpdate.cpp`. Facing exists in frame state as `pVecDirections` on both Players and Spaceships.

Model data never reaches the simulation:

- Models are glTF scenes. `ExportScene` writes a `SceneHeader` chunk and a generated `.MODEL` file that `ExportModel` packs under a `ModelHeader` chunk (`Common/DataFile.h`); both are eager, client-loaded types (`IsEagerChunk` in `Engine/Source/File/PackChunks.cpp`).
- The server opens only the Islands pack (`IsServerChunk` and the `BT_SERVER` filter in `PackChunks::LoadPackFiles`), so it has no Scene or Model chunks loaded.
- Which model an entity draws and its scale are client-only: `kPlayerModel` and `kfModelScale` live in `PlayersRender.cpp`, and `kuiSpaceshipModel` and its scale in `SpaceshipsRender.cpp`, both inside `BT_CLIENT`.

## Design

- **Bake.** DataPacker projects each model straight down, extracts and simplifies the silhouette, and fits a few primitives covering it. The primitives are stored with the model data.
- **Narrow phase.** A pair hits when any primitive of one entity, rotated by its facing, touches any primitive of the other.
- **Broad phase.** Unchanged in structure: the 8x8 zone grid still uses one bounding circle per entity, which encloses all its primitives.

Open choices a plan must decide:

- **Primitive.** Circles or capsules.
- **Vertical extent.** The current narrow phase is 3D, so footprint circles become either spheres at the entity's height or vertical cylinders (a 2D test).
- **Bounding radius per object.** `CollisionLayer::fRadius` is per layer today; the plan either keeps one layer-wide bound (the largest model in the layer) or adds a per-object radius.
- **Sweeps.** How a swept pair handles multiple rotated primitives: sweep each primitive with facing held for the tick or interpolated, or sweep the bounding circle and then test primitives at the time of impact.
- **Server data.** Where the server reads the primitives: open the Scene or Model pack on the server, or carry them in a chunk type the server already opens. Either way the sim, not render code, must know each entity's model and scale.
- **Contact point.** The current center-line contact point needs a replacement derived from the hitting primitive pair.
- **Animated or skinned models.** Bake from the bind pose, a chosen frame, or the union over a clip.

## Out of scope

- Terrain collision and navigation clearance (`ResolveDiscAgainstTerrain` in `Engine/Source/Frame/IslandTerrain.h`, and the navigation values `TerrainUtils.h` derives from `kfPlayerRadius`).
- Area damage (`Engine/Source/Frame/AreaDamage.cpp`), which measures distance to the entity center.
- Concave or per-triangle mesh collision.

## Notes

- Tier 3 when executed: it changes deterministic collision results, so PostRender CRC output, and it changes the model pack format. Bump the owning version named by the `Common/DataFile.h` layout `static_assert` for whichever chunk carries the primitives (`DataHeader::kiVersion` and the `ExportScene` or `ExportModel` `kiVersion`); no backward compatibility.
- Client and server simulate identically, so both must read the same baked bytes; the silhouette fit runs once in DataPacker, never at runtime.
- Reference: Godot, "Collision shapes (2D)," https://docs.godotengine.org/en/stable/tutorials/physics/collision_shapes_2d.html — favor primitive shapes for dynamic bodies, simplify detailed silhouettes, and several convex shapes beat one concave shape up to a point.
- Original note: "Dynamic hitboxes based on 'shadow' from above of 3D object, baked at Data Packer export."
