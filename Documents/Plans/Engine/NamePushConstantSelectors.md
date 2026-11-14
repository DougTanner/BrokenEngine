<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:15:37.100Z","dependsOn":[]} -->
# Name the push-constant area and rendering-mode selectors

## Context

`M/` is `Engine/Source/Graphics/Managers/` and `S/` is
`Engine/Data/Shaders/`. Lines are those at commit `401e4b75`, except that the
C++ literal lines reflect the authoring session's later edits to the two
recording files.

Record-once command buffers select a projection area or a rendering mode
through a push constant, and both sides write the selector as a bare number:

- C++ literals: `M/CommandBufferRecordGlobal.cpp:69`;
  `M/CommandBufferRecordMain.cpp:81`, `:85`, `:89`, `:232`, `:260`, `:264`,
  `:272`, `:276`, `:289`, `:444`, `:517`.
- Shader decodes: `S/Quads/QuadsVisibleArea.vert:47-57`,
  `S/Quads/QuadsAxisAlignedVisibleArea.vert:44-54`,
  `S/Objects/HexShield.vert:85`, `S/Model/ModelCommon.h:107-126`.

Two separate protocols are involved: the area selector in `.x` (quads, hex
shield) and the rendering mode in `.y` (models). The values agree today. A
mismatch falls silently into an `else` branch. `S/AGENTS.md`
`## Shared Contract` states "Push constants select projection/render modes
where command buffers are recorded once", and `S/ShaderLayoutsBase.h:177-185`
is the precedent for named selectors shared by both languages.

Separately, `S/ShaderLayoutsBase.h:98` defines `ELEVATION_FORMAT`, which
nothing uses.

Originating gap: the architecture phase (client/server and data shape lens)
of an analysis of `Engine/Source/Graphics/Managers` at that commit. Unmet
criteria: no bare selector literal on either side; no unused define in the
shared layout header.

## Design

Recommended:

- Add two constant groups to `S/ShaderLayoutsBase.h`, one per protocol, with
  the values in use today, and use them at the C++ sites and the four shader
  files. The payload is a `vec4`, so each C++ site converts the constant to
  `float`.
- The lighting area has no decoded value in the quad shaders: it is the
  "anything else" branch. Name it with a constant and leave that fallthrough
  as it is.
- Delete the `ELEVATION_FORMAT` line.

No selector value, push-constant layout, or shader branch changes.

## Critical files

- `Engine/Data/Shaders/ShaderLayoutsBase.h`
- `Engine/Data/Shaders/Quads/QuadsVisibleArea.vert`
- `Engine/Data/Shaders/Quads/QuadsAxisAlignedVisibleArea.vert`
- `Engine/Data/Shaders/Objects/HexShield.vert`
- `Engine/Data/Shaders/Model/ModelCommon.h`
- `Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp`
- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp`

## In scope

- Two selector constant groups in `ShaderLayoutsBase.h` and the
  `ELEVATION_FORMAT` line.
- The twelve C++ literals and the four shader decodes listed in
  `## Context`.

## Out of scope

- Selector values, the push-constant block layout, and the decode order.
- One macro per storage-image format for the GLSL format qualifiers.
- Descriptor binding numbers.

## Risk tier

Tier 2 — scoped behavior. Trigger: it edits shaders and a header shared by
C++ and GLSL, so shader data is rebuilt and a CPU/GPU selector contract is
re-expressed; Tier 1 covers only local work with no such exposure. The values
and the push-constant layout are unchanged, so there is no data-layout,
determinism, or wire exposure.

## Acceptance criteria

1. No bare selector literal remains at the listed sites, and
   `ELEVATION_FORMAT` is gone.
2. `/compile` builds the BrokenEngineSandbox client and ModelViewer with
   regenerated shader data.
3. Through `/agent-harness`, screenshots of a world frame with terrain,
   water, lights, a hex shield, and models (opaque and transparent, with
   shadows) match the same scene before the change. The supplied evidence
   records no harness setup.

## Coordination

`Documents/Plans/Engine/RemoveInertLightingDescriptorEntries.md` also edits
`Engine/Data/Shaders/ShaderLayoutsBase.h`, on other lines: it adds one Set 1
binding constant beside the existing Set 1 constants. Neither Plan depends
on the other; whichever lands later re-derives its lines by symbol.

## Notes

- A plan reviewer asked whether a wrong GLSL storage-image format qualifier
  is reported by validation; that was not settled and is not part of this
  Plan.
