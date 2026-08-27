<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:30:41.765Z","dependsOn":[]} -->
# Keep animation section markers consistent with emitted clips

## Context

The frozen audit retained `CAI/shard-0001/003`. `ExportScene::MainExport`
enters `WriteAnimationSection` whenever `rGltfModel.animations.size() > 0`
(`DataPacker/Source/ExportJobs/ExportScene.cpp:707-711`), while the animation
loader filters invalid channels and may emit no clips. The writer can publish
`bHasAnimation=true` with `uiAnimationCount=0`; runtime rejects that count at
`Engine/Source/Graphics/AnimationData.cpp:65-75`. Source/runtime bytes are
unchanged from frozen baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`, so
the failure is pre-existing.

## Design

Make the serialized animation marker and section depend on the clips that
survive `SceneAnimationLoader` filtering. When no clip remains, omit the
animation section and leave the scene's animated marker clear before
publication; never emit the contradictory zero-count section. Keep the runtime
zero-count rejection as defense in depth.

## Critical files

- `DataPacker/Source/ExportJobs/ExportScene.cpp` — section writing and marker.
- `DataPacker/Source/ExportJobs/Scene/SceneAnimationLoader.cpp` — filtering/emission.
- `Engine/Source/Graphics/AnimationData.cpp` — runtime load precondition.

## In scope

- The `WriteAnimationSection` decision and `bHasAnimation`/count pairing.
- The filtered-clip count needed to make every emitted animated scene loadable.

## Out of scope

- New animation features, channel interpolation, skeleton layout, or runtime animation evaluation.
- Removing the runtime corruption guard or changing pack version compatibility.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: serialized scene `.pack` data
crosses the DataPacker/runtime boundary. `bHasAnimation` must imply a nonzero
loadable clip section, while valid animated scenes retain their current data
and behavior.

Tier rationale: the fix corrects one emission condition in the exporter so the
marker follows the surviving clip count; the pack layout is unchanged and only
scenes that currently produce a contradictory zero-count section — which
runtime already rejects — export differently.

## Acceptance criteria

- A glTF with only filtered animation channels exports without an animation section and never publishes a zero-count animated section.
- A scene with retained clips still loads and evaluates unchanged on the client.
- No marker/count contradiction remains in generated scene chunks.

## Notes

Origin: `CAI/shard-0001/003`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0001.md:85`.
This Plan records debt only; no source fix or build was performed.
