<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T14:22:33.902Z","dependsOn":[]} -->
# Stop Puffs spending a lighting blur slot on a texture lighting never samples

## Context

Found by the architecture review phase of a deep analysis of
`Engine/Source/Frame/Collections` (2026-11-14). Line numbers are from commit
`ae681f03` plus the change this Plan landed with; locate by symbol if they
moved. `C/` abbreviates `Engine/Source/Frame/Collections/`, `BES/` abbreviates
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`.

`RegisterType` (`C/Collection.h:139-146`) passes any non-zero `rType.uiCrc` to
`RegisterLightingTextureCrc`, which requests the chunk, inserts it into the
lighting texture set, and asserts the `kiLightingBlurSlots` bound
(`Engine/Source/Graphics/Managers/TextureManager.cpp:742-750`). So every type
struct with a `uiCrc` field is treated as a lighting texture.

Only three collections read the field when rendering:
`C/AreaLights/AreaLightsRender.cpp:98-99`,
`C/PointLights/PointLightsRender.cpp:121-122`, and
`C/Billboards/BillboardsRender.cpp:102`. Puffs reads only `rType.uiColor`
(`C/Puffs/PuffsRender.cpp:105`) and SmokeTrails only `fWidth` and `iColor`
(`C/SmokeTrails/SmokeTrailsRender.cpp:83,149`).

Observed cost: the two non-zero Puffs registrations
(`BES/Omnis/Omnis.cpp:160`, `BES/Blasters/BlastersUpdate.cpp:71`) pass the same
CRC, so Puffs spends one blur slot and the two blur textures for that CRC
(`TextureManager.cpp:752-764`) on a texture no lighting path samples. Every
SmokeTrails registration passes 0, so that field is unread but costs no slot.

Unmet criterion: only types whose texture a lighting path samples reach
`RegisterLightingTextureCrc`.

## Design

Delete `PuffsType::uiCrc` (`C/Puffs/Puffs.h:15`) and `SmokeTrailsType::uiCrc`
(`C/SmokeTrails/SmokeTrails.h:15`) with their five designated initializers:
`C/Explosions/Explosions.cpp:87,127`, `BES/Omnis/Omnis.cpp:160`,
`BES/Blasters/BlastersUpdate.cpp:71`, and `BES/Missiles/Missiles.cpp:198`. The
`RegisterType` hook is unchanged; it simply stops matching these two types.

The smoke texture still loads without the field
(`Engine/Source/Graphics/Managers/TextureManager.h:153`). `sTypes` is a static
registry, not a member column: no CRC, serialization, or layout exposure.

## Critical files

- `Engine/Source/Frame/Collections/Puffs/Puffs.h`
- `Engine/Source/Frame/Collections/SmokeTrails/SmokeTrails.h`
- `Engine/Source/Frame/Collections/Explosions/Explosions.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/BlastersUpdate.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp`

## In scope

- `PuffsType::uiCrc`, `SmokeTrailsType::uiCrc`, and the five initializers that
  set them.

## Out of scope

- `RegisterType`, `RegisterLightingTextureCrc`, and `kiLightingBlurSlots`.
- `BillboardsType::uiCrc`: it has the same exposure and no registrant today,
  and whether billboards want the blur is a separate user decision.
- `SmokeTrailsInterpolate::kuiCrc`, a different constant.

## Risk tier

Tier 2 — scoped behavior. Trigger: runtime behavior of one subsystem changes
(one fewer lighting blur texture pair is created) and two public type members
are removed. No determinism/CRC, serialization, layout, wire, or threading
exposure.

## Acceptance criteria

- Neither `PuffsType` nor `SmokeTrailsType` has a `uiCrc` member.
- `/compile` builds the BrokenEngineSandbox client and server.
- Through `/agent-harness`, a screenshot with puffs and smoke trails on screen
  still shows both. The supplied evidence records no harness setup.
- The executing session confirms no private-game initializer sets either field
  by building the private game where its source is available, and reports it
  as unconfirmed otherwise.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
