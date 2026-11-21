<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T23:55:12.420Z","dependsOn":["Documents/Plans/Engine/RemoveFlightPlanes.md","Documents/Plans/Engine/SingleLayerWorldLighting.md"]} -->
# Merge RenderTargetTexturesLighting.cpp into RenderTargetTextures.cpp

## Context

`class RenderTargetTextures`
(`Engine/Source/Graphics/Managers/RenderTargetTextures.h:8`) defines its
members in two .cpp files: `RenderTargetTextures.cpp` holds nine definitions
(`Create` `:25` through `CreateTerrainTextures` `:329`), and
`RenderTargetTexturesLighting.cpp` holds `DestroyLightingTextures` (`:24`),
`CreateLightingTextures` (`:69`), and `RegisterDebugTextures` (`:415`), with
file-static name tables (`:16-22`) and a `static_assert` on the lighting layer
count (`:14`). `Documents/C++StyleGuide.txt` rule 67 requires a `class` to keep
every member definition outside its header in one .cpp file.

`RenderTargetTexturesLighting.cpp` is listed in
`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj:427`
and its `.filters:956`, and in
`Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj:121` and
its `.filters:110`.

The user chose the merge into `RenderTargetTextures.cpp`, executed after the
single-layer lighting work. `Documents/Plans/Engine/SingleLayerWorldLighting.md`
rewrites the lighting file (its include, `static_assert`, name tables, destroy,
create, and debug-slot regions), and
`Documents/Plans/Engine/SingleLayerSmoke.md` and
`Documents/Plans/Engine/RemoveFlightPlanes.md` cite line numbers at the top of
`RenderTargetTextures.cpp`, where the merge adds an include; landing this merge
first would invalidate their citations. `SingleLayerWorldLighting` also deletes
the `static_assert` that `Documents/Plans/Game/SingleFlightHeightShips.md`
cites at `RenderTargetTexturesLighting.cpp:14`, and it depends on that Plan.

## Design

A pure move with no behavior change:

1. Move every definition, file-static table, and `static_assert` that remains
   in `RenderTargetTexturesLighting.cpp` at execution time into
   `RenderTargetTextures.cpp`, after its last member definition, keeping their
   order. Add the lighting file's includes that `RenderTargetTextures.cpp` does
   not already have (today `Ui/LightingWrappersBase.h`) in its include block.
2. Delete `RenderTargetTexturesLighting.cpp` and remove it from the
   BrokenEngineSandbox and ModelViewer project and filter files through
   `/update-vcxproj`.
3. Measure the merged file with `/reduce-file`'s measurement and report one
   size observation. Report it only; do not switch to another mechanism, such
   as a static-function struct, whatever the measurement.

## Critical files

- `Engine/Source/Graphics/Managers/RenderTargetTextures.cpp`
- `Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp` (deleted)
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
  and `.filters`
- `Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj` and
  `.filters`

## In scope

- `RenderTargetTextures.cpp`: the include block and the appended lighting
  definitions, tables, and `static_assert`s.
- `RenderTargetTexturesLighting.cpp`: deleted.
- The four project and filter entries for the deleted file.
- Comments and documents that name `RenderTargetTexturesLighting.cpp` at
  execution time, renamed to `RenderTargetTextures.cpp`; today
  `Engine/Source/Ui/LightingWrappersBase.h:28` and
  `Documents/Features/Graphics/PlanarHolographicRadianceCascades.md:150`,
  `:246` (the predecessor Plans may already have changed them).

## Out of scope

- Any change to a moved function's body, signature, or declaration in
  `RenderTargetTextures.h`.
- Changing `class RenderTargetTextures` to `struct`, or a static-function
  struct for the lighting functions.
- Reducing or splitting `RenderTargetTextures.cpp`, whatever its measured size.
- Callers (`Graphics.cpp`, `TextureManager.cpp`, `Lighting.cpp`), which need no
  change.
- Unit tests.

## Risk triggers and invariants

Tier 1: a style move plus project membership, behavior-preserving, with no
public signature or invariant exposure. Client-only code (`BT_CLIENT`); no CRC,
wire, serialization, or shader exposure.

- Both projects that compiled the lighting file still compile its definitions
  through `RenderTargetTextures.cpp`, which both already list.

Risk: the merged size is estimated, not measured. Today the two files measure
4,618 and 4,996 bt-token-v1 (9,614 together) against the 10,000 .cpp
reduction threshold `/reduce-file` owns; the predecessor Plans change both
files before this runs, so the merged file is expected to land near, possibly
over, that threshold. The size observation records the outcome.

## Acceptance criteria

1. No `.cpp` other than `RenderTargetTextures.cpp` defines a
   `RenderTargetTextures::` member.
2. No tracked file outside `Documents/Plans/` and `Documents/Investigations/`
   names `RenderTargetTexturesLighting`.
3. `/update-vcxproj` validation passes for both projects.
4. `/compile` builds the BrokenEngineSandbox client and the ModelViewer.
5. The session report carries the merged file's measured bt-token-v1 size as
   a size observation.
