<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-03T21:46:57.910Z","dependsOn":[]} -->
# Preserve glTF normal texture strength

## Context

The Molnia source-fidelity repair exposed a separate material contract gap.
The ordinary generated MolniaAnimated.fbx.glb material has normalTexture.scale
equal to 0 and a normal texture index of 1. In
`DataPacker/Source/ExportJobs/Scene/GltfSceneDecoder.cpp:512-516`, material
decoding preserves only that texture's index and UV set. In
`Engine/Data/Shaders/Model/Model.frag:170-172`, GetNormal applies sampled XY
without an authored strength. Thus a zero authored strength still perturbs
normals. These decoder and shader behaviors predate the source-image repair.
The contribution to any dark hull screenshot region has not been proven;
this Plan does not diagnose a blue-glow cause or require a brighter hull.

## Design

The author's recommendation is to first use /external-diagnose-bug to confirm
the authored normal-strength contract from source material through generated
glTF, decoding, packed material data and GetNormal. Confirm the zero-strength
reproducer and the default-strength behavior before selecting the smallest
correction inside the regions below. Do not presume whether the correction
belongs in conversion, texture production or material/shader propagation.
If the confirmed fix exceeds this boundary, return for re-planning.

## Critical files

- `DataPacker/Scripts/ExportBlenderScene.py` — normal export evidence only,
  unless diagnosis proves authored strength is exported incorrectly.
- `DataPacker/Source/ExportJobs/Scene/GltfSceneDecoder.cpp` — material decoding.
- `Common/DataFile.h:249` — MaterialShaderData packed material payload.
- `Engine/Data/Shaders/ShaderLayoutsBase.h` — PbrMaterialLayout.
- `Engine/Data/Shaders/Model/Model.frag` — GetNormal.
- Owning scene version and material payload producers/consumers identified
  by propagation if the confirmed fix changes their contract.

## In scope

- Investigate normalTexture.scale preservation and application for authored
  zero, default and nondefault strength through the named material pipeline.
- Correct the proven loss or misapplication using existing mechanisms;
  update correctness-dependent material producers/consumers, owning version,
  generated outputs and documentation only as required by the confirmed fix.
- Verify default behavior, zero-strength unperturbed normals and one
  nondefault authored strength with source and numeric material evidence,
  plus ordinary ModelViewer verification through /agent-harness.

## Out of scope

No changes to authored models/textures, lighting, exposure, tone mapping,
base-color policy, emissive policy, normal-map encoding, unrelated material
features, game behavior or simulation. No screenshot brightness threshold,
Molnia-specific branch, new harness command or unit tests.

## Acceptance criteria

- The confirmed authored zero-strength case no longer receives a sampled
  normal perturbation; absent scale preserves the documented default, and a
  nondefault strength is applied according to the confirmed contract.
- Numeric source/export/material evidence settles strength independently
  of screenshot brightness; ordinary regeneration follows the existing
  invalidation path and a second export is clean.
- Relevant DataPacker and client builds pass through /compile; shader and
  layout review settle the selected representation. Use /agent-harness for
  a ModelViewer comparison with unchanged framing and material lighting.
  Supplied evidence records no ordered harness setup commands; establish
  and record the ordinary setup during execution.

## Notes

Future implementation is conservatively Tier 3: the possible correction
crosses independently owned asset decoding and runtime shader/material
contracts and may affect serialized or CPU/GPU layout. Preserve simulation
determinism/CRC, replay and wire behavior. Assess the actual representation
and owning format version before implementation; reject obsolete generated
data through that owner's current version if it changes. No prerequisite
Plan or mandatory coordination constraint was identified.
