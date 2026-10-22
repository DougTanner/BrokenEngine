<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-21T18:41:25.738Z","dependsOn":[]} -->
# Normalize exported model space

## Context and recommended design

DataPacker currently exports models with asset-specific dimensions and origins. The user requested exported model space from -1.0 to 1.0, with 0.0 as the default center of rotation, and corresponding BrokenEngineSandbox scale updates.

The author's recommended design is to center the complete default-pose bounding box at `(0,0,0)` and uniformly scale its longest dimension to span `-1` to `+1`. Other dimensions remain inside that range. This preserves proportions and gives every model a consistent default rotation pivot. Compensating sandbox factors preserve current displayed dimensions; the pivot intentionally moves to the bounding-box center, so the old off-center placement is not preserved.

Calculate bounds over complete exported geometry in its default pose, including node transforms and skinning. Use `center = (min + max) / 2`, `scale = 2 / longestExtent`, and `position' = (position - center) * scale`.

The author recommends baking normalization into static vertices and converting exported root
translations, root scales, and their animation values for animated models. Scale cubic-animation
tangents without the centering offset. Preserve rotations, child channels, inverse-bind matrices,
and the existing hierarchy. Compute normalization on both fresh pre-export and cached-intermediate
export paths. Keep it transient, without serialized normalization metadata or runtime normalization;
this keeps the conversion inside the existing exporter and preserves the payload layout.

Reject empty, non-finite, or zero-size geometry through the existing export failure mechanism. Bump ExportScene's manual version from 68 to 69 and regenerate affected intermediates and packs so the changed exported semantics invalidate old outputs.

## In scope

The following regions are the target and ceiling for the smallest complete implementation. File names grant permission only for the named regions and their necessary includes and declarations.

- `DataPacker/Source/ExportJobs/ExportScene.cpp`: `Export`, `PreExport`,
  `LoadVerticesAndOptimizeMeshes`, `MainExport`, and `WriteAnimationSection`, plus directly required
  local normalization helpers. Compute default-pose bounds, normalize static output vertices, carry
  transient normalization through both export paths, and convert exported animated root transforms
  and their translation/scale channels before serialization. Existing `LoadVertices`,
  `ComputeNodeWorldTransform`, `LoadSkeletonData`, and `LoadAnimations` are available for reuse;
  their implementations are outside this change's ceiling.
- `DataPacker/Source/ExportJobs/ExportScene.h`: `kiVersion` and only the private declarations or transient state required by the preceding normalization operations.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersRender.cpp`: `kfFighterFleetModelScale` and `kfFighterRandomModelScale`.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsRender.cpp`: `kfBattleshipFleetModelScale` and `kfBattleshipRandomModelScale`.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/OmnisRender.cpp`: `kfOmniFleetModelScale` and `kfOmniRandomModelScale`.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`: the numeric mesh compensation factor in `kfHexShieldSizeScale`, retaining `kfOmniRadius`.
- Generated model intermediates: the vertex output in `Engine/Data/Models/DualGeodesicIcosahedron/DualGeodesicIcosahedron.gltf.MODEL` and `Projects/BrokenEngineSandbox/Data/Models/{spaceship2,Spaceship,aim-9_missile}/scene.gltf.MODEL`, plus their exporter-owned markers and generated packs as required by ordinary regeneration. Source assets are not edited.
- `DataPacker/Source/ExportJobs/AGENTS.md`, `## Scene and Model`: document the new exported-space invariant and default-pose normalization ownership through `/update-claude-docs`.

Apply `newScale = oldScale * originalLongestExtent / 2`, using full-precision asset bounds. The existing factors and expected approximate replacements are:

| Consumer | Current factor | New factor, approximately |
|---|---:|---:|
| Fleet ship models, all three collections | 0.3667 | 1.09270874 |
| Random-spawn ship models, all three collections | 0.00175 | 0.67238903 |
| Shield mesh | 0.0667 | 1.334 |
| Missile | 0.5 | Unchanged |

Retain existing radius multipliers, orientations, and missile width scaling.

## Out of scope

No new abstractions, configuration, runtime normalization, public runtime API, serialized
normalization metadata, payload layout changes, adjacent fixes, or unrelated refactors. Do not
change source models, animation hierarchy, rotations, child channels, inverse-bind matrices,
simulation sizing, collision radii, existing render orientations, or missile scaling. Add no unit
tests. Existing scene loaders and runtime/shader consumers are inspection targets for contract
verification, not edit targets. Asset additions from other plans are independent of this work.

## Risk and invariants

This is Tier 3: exported model-space semantics change a serialized-data invariant and the implementation coordinates DataPacker with game rendering. Before implementation, prepare the execution card and complete required alternatives follow-through, plan audit, simplicity review, and Tier-3 decision review under the Change Workflow.

Bounds describe the complete model in default-pose model space; animated mesh-local buffers need not individually fit the cube. Animation may extend outside the cube. Normalization never changes during playback. Preserve model proportions, displayed dimensions, animation transform composition, and the current payload layout. The version bump invalidates old exported semantics without adding backward compatibility.

## Verification and observable acceptance

- Before editing, capture original model bounds and representative sandbox views.
- Build DataPacker in Release, regenerate local data, and build sandbox client and server through `/compile`.
- Check every shipped model: default-pose bounds centered at zero, longest extent approximately 2, other dimensions inside [-1,1], preserved proportions, and unchanged displayed dimensions after compensation.
- Verify fresh pre-export and cached-intermediate export paths produce equivalent normalized results.
- Inspect animated matrix composition and translation/scale interpolation, including cubic tangents, multiple roots, and skinned geometry. Where shipped assets provide no animation fixture, use code inspection and build evidence.
- Use `/agent-harness` to inspect ships, missiles, and shields for preserved dimensions and orientation, with the intended centered rotation pivot.
- Complete applicable correctness, shader-contract, style, documentation, and acceptance reviews. Inspect shader contracts without editing shaders unless a separately approved scope change becomes necessary.
