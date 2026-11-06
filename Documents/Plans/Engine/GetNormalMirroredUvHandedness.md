<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-05T13:56:44.434Z","dependsOn":[]} -->
# Honor mirrored UV handedness in the GetNormal tangent frame

## Context

glTF defines the bitangent as `cross(normal, tangent.xyz) * tangent.w`, so a
mirrored UV region (`w` of -1) has the opposite bitangent from an unmirrored
one. `GetNormal` always uses `B = cross(N, T)`
(`Engine/Data/Shaders/Model/Model.frag:173`), so its frame assumes `w` of +1:
`T` follows the UV direction because it divides by `det`, but `B` never takes
that sign, and the sampled Y term is negated on mirrored regions. The DataPacker
scene vertex loader reads no `TANGENT` attribute
(`DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:118-122` reads the
UV sets), so authored `w` does not reach the shader. This is present at commit
`5c2e6a8d3c22cc8815256342cd63ec7fe8f4d8b3`.

An adversarial review of glTF normal strength work found this; it is outside
that change's boundary and failed none of its acceptance criteria, because
fixing it changes rendered output of tracked models.

The authored data proves the case is reachable. Share of triangles in
normal-mapped primitives whose first vertex has `TANGENT` `w` below 0, read from
each tracked `scene.gltf` and its buffer before node transforms:

| Model | `w` below 0 | Triangles |
|---|---|---|
| LightFighterSpaceship | 34283 (47.2%) | 72677 |
| BattleshipJesterz86 | 27707 (44.5%) | 62208 |
| SpaceBattleshipAquamarine | 50391 (44.5%) | 113125 |
| RavenStarConflictV2 | 2303 (43.7%) | 5270 |
| Spaceship | 21728 (32.0%) | 67847 |
| SpaceFighter | 3929 (22.5%) | 17456 |
| aim-9_missile | 16 (0.9%) | 1700 |
| SeaCreature2 | 2 (0.0%) | 15474 |
| MolniaGltf | 0 | 18330 |

In these models `w` matches the sign of the triangle's area in raw glTF UV space
on at least 97% of triangles: positive area pairs with `w` of +1, which is the
case the current frame gets right.

The rendered effect has not been measured. Relief on the affected regions is
expected to read inverted along Y, but no screenshot or numeric frame comparison
records it, so the symptom to retire is that recorded data plus the frame
mismatch above.

Do not assume the sign of `det` alone is the required factor. glTF UV V points
down the image and screen `dFdy` points down, so the relation between `det` and
`w` must be derived, and the measured pairing above is not the mapping the glTF
convention suggests for a front-facing unmirrored triangle. `det` also flips for
any back face that reaches `GetNormal`, which has no `gl_FrontFacing` handling,
and several tracked materials are `doubleSided`.

## Design

The author's recommendation is to first use `/external-diagnose-bug` to confirm
the effect: for a tracked model from the table, compare `GetNormal`'s frame with
authored `T`, `w` and mirrored regions, and capture a ModelViewer view where a
mirrored region and its unmirrored mate disagree. From that, establish how the
shader-available sign maps to authored `w` for front faces, back faces and
reflected node transforms. Then make the smallest correction to the bitangent
sign in `GetNormal`.

If the confirmed sign cannot be derived in `GetNormal` from existing inputs, so
that the fix needs a packed vertex tangent or a vertex-layout change, stop and
return for re-planning instead of widening scope.

## Critical files

- `Engine/Data/Shaders/Model/Model.frag` — `GetNormal`.
- `Engine/Data/Shaders/Model/AGENTS.md` — tangent space note, if the confirmed
  fix changes what it states.

## In scope

- Investigate the rendered effect of `B = cross(N, T)` on mirrored UV regions
  and the sign mapping described in `## Context`.
- The bitangent sign in `GetNormal`, including the region from the `det`
  computation to `TBN` construction, bounding the fix.
- The tangent space note in the Model shader `AGENTS.md`, only as the fix
  requires.

## Out of scope

No packing of `TANGENT`, vertex-input or `.pack` format change, DataPacker
change, authored model, texture or material edit, normal-map encoding or
strength change, or which UV set the derivatives read. No lighting, exposure or
tone mapping change, no `doubleSided` or culling policy change, no shader other
than `Model.frag`, no new fixture model or harness command, and no unit tests.

## Acceptance criteria

- The recorded symptom no longer reproduces: on the diagnosed tracked model, a
  mirrored region's normal-map relief reads the same way as its unmirrored mate,
  and the numeric frame comparison agrees with authored `w` on sampled mirrored
  and unmirrored fragments.
- MolniaGltf, whose triangles are all `w` of +1, stays visually unchanged under
  unchanged framing and lighting in ModelViewer.
- `Model.frag` compiles through the ordinary DataPacker shader export via
  `/compile`.
- Both visual checks run through `/agent-harness`. The supplied evidence records
  no ordered harness setup commands; establish and record the ordinary setup
  during execution.

## Notes

Future implementation is Tier 2: trigger is one subsystem's runtime behavior,
confined to one client shader function, with no CPU/GPU layout change. The
re-planning exit above covers the Tier 3 case. The fix intentionally changes
rendered output of tracked models, so a before and after comparison is expected
rather than a regression. Model shading is outside PostRender state, so
determinism/CRC, replay, wire, threading and allocation are unaffected. No
serialization change or `kiVersion` bump: the shader recompiles through its
dependency fingerprint. No prerequisite Plan or mandatory coordination
constraint was identified.
