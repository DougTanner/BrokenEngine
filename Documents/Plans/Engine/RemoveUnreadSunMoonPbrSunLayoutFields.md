<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:50:25.997Z","dependsOn":[]} -->
# Remove Three Layout Fields No Shader Reads

## Context

Found by the architecture review phase of a deep analysis of
`Engine/Source/Graphics/Render` (the directory, not its subdirectories). Line
numbers are from commit `c675b6b2`; locate by symbol if they moved. `R/`
abbreviates `Engine/Source/Graphics/Render/` and `S/` abbreviates
`Engine/Data/Shaders/`.

Three layout fields are written every frame and read by nothing:

- `GlobalLayout::f4SunColor` (`S/ShaderGlobalLayout.h:38`), written at
  `R/GlobalUniforms.cpp:136`.
- `GlobalLayout::f4MoonColor` (`S/ShaderGlobalLayout.h:39`), written at
  `R/GlobalUniforms.cpp:142`.
- `MainLayout::fPbrSun` (`S/ShaderMainLayout.h:125`), written at
  `R/LightingUniforms.cpp:343`.

A search of the repository for `SunColor`, `MoonColor`, and `PbrSun` across
all file types finds, besides those declarations and writes, only: the folded
fields and their readers (`f4SunColorTerrain`, `f4MoonColorTerrain` at
`S/ShaderFunctions.h:249-250`; the `f4PhysicallyBasedRendering*` fields at
`S/Model/Model.frag:289-300`), derivation comments
(`S/ShaderGlobalLayout.h:45-68`, `S/Water/Water.frag:139`,
`S/Model/Model.frag:254-257`), and the `gPbrSun` tunable with its CPU fold
(`R/GlobalUniforms.cpp:194-197`). No shader, C++ reader, or alias reads the
three raw fields. `Projects/BrokenEngineSandbox` and `Projects/ModelViewer`
hold no match. The shaders consume the CPU-folded products instead, as
`R/AGENTS.md` `## Ordering and Publication` requires of day-cycle terms.

The private game project is absent from session worktrees. The authoring
session's search of the primary checkout's working tree found no non-Markdown
match for the three names there, not pinned to a commit.

Unmet criterion: every `GlobalLayout` and `MainLayout` field has a shader
reader.

## Design

Recommended: delete the three declarations and the three writes. `vecSunMoon`
and `vecMoon` stay as CPU locals in `PopulateDayCycleColors`, since the folded
products are built from them; `gPbrSun` and its Tweaks slider stay. Reword the
derivation comments that name `f4SunColor`, `f4MoonColor`, or `fPbrSun` as
layout fields (`S/ShaderGlobalLayout.h:45-68`, `S/Water/Water.frag:139`,
`S/Model/Model.frag:254-257`, and the comment at
`R/GlobalUniforms.cpp:190-192`) so they name the CPU terms.

Both layouts are shared CPU/GLSL headers, so the change rebuilds every program
and re-exports every shader. The shader repack step is the Local generation
build through `/compile`. A stale packed shader would read shifted fields.

The executing session also builds and repacks the private game from the
primary checkout, where its shaders are present; a reader there would fail its
shader compile at pack time. Not verified by the authoring session: whether an
existing mechanism rejects stale packed shaders after a layout change, or
whether a normal build always repacks. The executing session checks this
before relying on an incremental build.

## Critical files

- `Engine/Data/Shaders/ShaderGlobalLayout.h`
- `Engine/Data/Shaders/ShaderMainLayout.h`
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`
- `Engine/Source/Graphics/Render/LightingUniforms.cpp`
- `Engine/Data/Shaders/Water/Water.frag` and
  `Engine/Data/Shaders/Model/Model.frag` (comments only)

## In scope

- The declarations of `f4SunColor`, `f4MoonColor`, and `fPbrSun`.
- The three writes: `PopulateDayCycleColors` at `R/GlobalUniforms.cpp:136`
  and `:142`, and `RenderLightingMain` at `R/LightingUniforms.cpp:343`.
- The comments listed in `## Design` that name the removed fields.

## Out of scope

- `gPbrSun`, its slider, and every folded product field.
- Reordering, packing, or renaming any other layout field.
- Any other possibly unread layout field; only these three were proven.

## Risk tier

Tier 3, per `.agents/references/risk-tiers.md`. Trigger: data layout — the
shared CPU/GLSL scalar layouts of `GlobalLayout` and `MainLayout` lose fields,
which every shader of both sets and the Render populators must agree on.
Client-only visual state: nothing here is simulated, CRC-checked, serialized,
sent on the wire, saved, or replayed, and no `.pack` format or `kiVersion`
changes. The server includes the layout headers and is rebuilt.

## Acceptance criteria

- A repository search for the three field names finds no declaration, write,
  or comment naming them as layout fields.
- The Local generation build through `/compile` exports every shader with no
  error and builds the BrokenEngineSandbox client and server and ModelViewer.
- The private game builds and repacks from the primary checkout, or the
  executing session reports it as unconfirmed with the reason.
- `/glsl-review` reports no CPU/GLSL layout finding for the change.
- Through `/agent-harness`, screenshots of one scene before and after, at the
  same camera and time of day, show no visible difference in terrain, water,
  models, and smoke lighting. The evidence behind this Plan records no
  harness setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- `Documents/Plans/Engine/PublishAreaToUvInverseExtents.md` and
  `Documents/Plans/Engine/ShareSmokeSwirlTimeOffset.md` also change
  `GlobalLayout` fields, at other positions. No ordering is needed; whichever
  lands later applies its edit to the layout then in the tree.
