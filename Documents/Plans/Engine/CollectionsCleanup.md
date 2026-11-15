<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T14:22:36.710Z","dependsOn":[]} -->
# Collections cleanup: unread surface, unsampled type CRCs, point-light controller types, and controlled grow and remove

## Context

Found by a deep analysis of `Engine/Source/Frame/Collections` (2026-11-14),
mostly by its architecture review phase; the `GrowPairedCollections` return
value was found by its in-function analysis phase. Line numbers are from commit
`ae681f03` plus the change this Plan landed with; locate by symbol if they
moved, including where an earlier step of this Plan moved them. `C/`
abbreviates `Engine/Source/Frame/Collections/`, `BES/` abbreviates
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`.

The private game's C++ source was not available to the authoring session, so
"no private-game caller or user" is unconfirmed for every item below. No use of
the removed names exists in Engine, BrokenEngineSandbox, ModelViewer, Tools,
or DataPacker.

### Type CRCs lighting never samples

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

### Collection surface nothing reads

1. `AllocateAndCopyIds` (`C/CollectionMemory.h:306-317`) has no caller. The
   frame fold calls `AllocateAndCopyMembers` or a leaf `AllocateAndCopy`
   (`Engine/Source/Frame/FrameUtils.h:334-341`). It also copies `pIds` only,
   which cannot meet the `C/AGENTS.md` `## Core Contract` rule that
   `pLifecycleFlags` is carried to the next frame, so adopting it would break
   that rule. Its only other mentions are
   `Documents/Features/Graphics/DecalSystem.md:71` and
   `Documents/Features/Graphics/FlipbookSpriteAnimations.md:91`.
2. `Uuid::Write`/`Uuid::Read` (`C/CollectionId.h:35-42`) are called only from
   `Id<T>::Write`/`Id<T>::Read` (`:71-78`), which have no caller. Columns are
   written as raw spans (`C/Collection.h:47-70`), and no frame value is `Uuid`-
   or `Id`-typed (`Engine/Source/Frame/FrameBase.h:266-275`). The four members
   read as a wire contract and are not one.
3. `GrowPairedCollections` (`C/CollectionLifecycle.h:43-59`) returns `bool`.
   All 18 call sites (12 in engine collection sources, 6 in
   BrokenEngineSandbox) are bare statements.
4. `PointLightsType::fVisibleArea` (`C/PointLights/PointLights.h:18`) has no
   reader: `PointLightsRender.cpp:94-104` takes visible area from the column,
   not the type. Its only writer is `C/Explosions/Explosions.cpp:40`. The
   `.fVisibleArea` initializers elsewhere set `ControllerKeyframe` or
   `PointLightsInterpolate::SyncData`, different structs.

### Point-light controller types in the framework header

`ControllerKeyframe` and `ControllerType` (`C/CollectionController.h:15-55`)
carry point-light fields and are named only at
`C/PointLights/PointLightsUpdate.cpp:39,41,119`. The default template argument
`CONTROLLER_TYPE = ControllerType` (`CollectionController.h:107`) has one user,
`C/PointLights/PointLights.h:31`. The two sibling controlled collections
define their types in their own leaf headers (`C/Puffs/Puffs.h:19-51`,
`C/WindRadials/WindRadials.h:13-38`). No file under `Projects/` names either
type. So a single-collection type sits in a framework header the server also
compiles, and the three siblings do not match.

### Controlled-collection callbacks with no per-collection decision

`DestroyExpiredControlled` and `AddControlledElement`
(`C/CollectionController.h:128-152`, `:93-103`) take callbacks that carry no
per-collection decision:

- The three removal lambdas are fixed by the collection type:
  `RemoveIndexableElement` then `--i` (`C/PointLights/PointLights.cpp:13-17`),
  `DestroyElement` then `--i` (`C/Puffs/Puffs.cpp:13-17`,
  `C/WindRadials/WindRadials.cpp:13-17`). The same choice between the two
  removal functions already exists at
  `Engine/Source/Frame/FrameUtils.h:401-408`.
- The three grow lambdas are identical: each calls `GrowPairedCollections` on
  the pair's `Members()` (`C/PointLights/PointLightsUpdate.cpp:121-124`,
  `C/Puffs/PuffsUpdate.cpp:61-64`,
  `C/WindRadials/WindRadialsUpdate.cpp:62-65`).

The add and seed callbacks do differ per collection and stay.

### Unmet criteria

- Only types whose texture a lighting path samples reach
  `RegisterLightingTextureCrc`.
- The collection framework exposes no entry point, member, or result that
  nothing reads.
- The framework controller header holds no single-collection type (root
  `AGENTS.md` `## Directives`, "Mirrored patterns stay parallel").
- A controlled collection supplies only the callbacks that differ per
  collection (root `AGENTS.md` `## Directives`, DRY).

## Design

The steps run top to bottom. `Explosions.cpp`, `PointLights.h`, and
`CollectionController.h` are each edited by more than one step; the order
below applies the later-line `Explosions.cpp` edits before the earlier one and
settles `GrowPairedCollections`' return type and the controller types before
the controller helpers change.

1. Delete `PuffsType::uiCrc` (`C/Puffs/Puffs.h:15`) and
   `SmokeTrailsType::uiCrc` (`C/SmokeTrails/SmokeTrails.h:15`) with their five
   designated initializers: `C/Explosions/Explosions.cpp:87,127`,
   `BES/Omnis/Omnis.cpp:160`, `BES/Blasters/BlastersUpdate.cpp:71`, and
   `BES/Missiles/Missiles.cpp:198`. The `RegisterType` hook is unchanged; it
   simply stops matching these two types. The smoke texture still loads
   without the field (`Engine/Source/Graphics/Managers/TextureManager.h:153`).
2. Delete `PointLightsType::fVisibleArea` and the `.fVisibleArea` line of the
   `PointLightsType` initializer at `Explosions.cpp:40`.
3. Delete `AllocateAndCopyIds`, and reword the two feature-document lines that
   name it so they name `AllocateAndCopyMembers`.
4. Delete `Uuid::Write`, `Uuid::Read`, `Id<T>::Write`, and `Id<T>::Read`.
5. Change `GrowPairedCollections` to return `void` and delete its two `return`
   values.
6. Move `ControllerKeyframe` and `ControllerType` to `PointLights.h`,
   unchanged. `PointLights.h` is whole-file `BT_CLIENT`, so the inner guard at
   `CollectionController.h:46-52` goes with the move. `C/Puffs/Puffs.h:47-48`
   uses `Wrapper*` with no declaration of its own and relies on the forward
   declaration at `CollectionController.h:6-8`: that declaration stays, or
   `Puffs.h` gains one. Keep the type names as they are; a rename to match
   the siblings' naming is not needed for the criterion.
7. Drop the default argument at `CollectionController.h:107` and name the type
   explicitly at `PointLights.h:31`; fix the comment at
   `CollectionController.h:105`, which describes the default.
8. `DestroyExpiredControlled` removes the expired row itself — the id-indexed
   removal when the collection has an id-to-index map, the plain swap-and-pop
   otherwise — and steps the index back, so the revisit rule has one owner.
   Its removal-callback parameter goes.
9. `AddControlledElement` takes the PostRender collection and calls
   `GrowPairedCollections` itself. Its grow-callback parameter goes.
10. `C/Collection.h:3-6` includes `CollectionController.h` before
    `CollectionLifecycle.h` and `CollectionMemory.h`, which declare
    `HasIdToIndex`, `GrowPairedCollections`, and both removal functions.
    `CollectionController.h` therefore includes `CollectionLifecycle.h`
    itself, which already includes `CollectionMemory.h`
    (`CollectionLifecycle.h:3-4`) and names nothing `CollectionController.h`
    declares. `Collection.h` is the only file that includes
    `CollectionController.h`, and it stays unchanged.
11. The six call sites drop the two lambdas each no longer passes.
12. Reword the sentence in `C/AGENTS.md` `## Identity, Serialization, and CRC`
    that names "a `DestroyExpiredControlled` callback" to match.

Nothing in steps 1-7 changes simulation behavior: an uncalled template is never
instantiated, the deleted `Id`/`Uuid` members are non-virtual so `Id`/`Uuid`
size and column layout are unchanged, the growth rule is untouched, every
`PuffsType`, `SmokeTrailsType`, and `PointLightsType` initializer uses
designated members, and `sTypes` and the controller registry are static
registries, not member columns. Step 1 changes one runtime behavior: one fewer
lighting blur texture pair is created. After steps 8-11, row order after a
removal stays the same forward walk. All three controlled collections are
client-only (`Engine/Source/Frame/FrameBase.h:244-251` lists them only under
`BT_CLIENT`), so nothing reaches the shared CRC or the wire.

## Critical files

- `Engine/Source/Frame/Collections/CollectionController.h`
- `Engine/Source/Frame/Collections/CollectionMemory.h`
- `Engine/Source/Frame/Collections/CollectionId.h`
- `Engine/Source/Frame/Collections/CollectionLifecycle.h`
- `Engine/Source/Frame/Collections/PointLights/PointLights.h`,
  `PointLights.cpp`, `PointLightsUpdate.cpp`
- `Engine/Source/Frame/Collections/Puffs/Puffs.h`, `Puffs.cpp`,
  `PuffsUpdate.cpp`
- `Engine/Source/Frame/Collections/WindRadials/WindRadials.cpp`,
  `WindRadialsUpdate.cpp`
- `Engine/Source/Frame/Collections/SmokeTrails/SmokeTrails.h`
- `Engine/Source/Frame/Collections/Explosions/Explosions.cpp`
- `Engine/Source/Frame/Collections/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/BlastersUpdate.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp`
- `Documents/Features/Graphics/DecalSystem.md`
- `Documents/Features/Graphics/FlipbookSpriteAnimations.md`

## In scope

- `PuffsType::uiCrc`, `SmokeTrailsType::uiCrc`, and the five initializers that
  set them.
- `PointLightsType::fVisibleArea` and its `Explosions.cpp` initializer line.
- `AllocateAndCopyIds`, and the one line in each of the two feature documents
  that names it.
- `Uuid::Write`, `Uuid::Read`, `Id<T>::Write`, and `Id<T>::Read`.
- `GrowPairedCollections`' return type and its two `return` values.
- `ControllerKeyframe` and `ControllerType`: moved unchanged to
  `PointLights.h`, with the inner `BT_CLIENT` guard and, only if `Puffs.h`
  gains its own, the `Wrapper` forward declaration.
- The default template argument of `ControllerTypeRegistry` and the comment
  above it; the base-class template arguments of `PointLightsInterpolate`.
- `DestroyExpiredControlled` and `AddControlledElement`: parameters and
  bodies.
- The three `Destroy` call sites and the three `AddControlled` call sites.
- The `CollectionLifecycle.h` include added to `CollectionController.h`.
- The sentence in `C/AGENTS.md` that names the callback.

## Out of scope

- `RegisterType`, `RegisterLightingTextureCrc`, and `kiLightingBlurSlots`.
- `BillboardsType::uiCrc`: it has the same exposure and no registrant today,
  and whether billboards want the blur is a separate user decision.
- `SmokeTrailsInterpolate::kuiCrc`, a different constant.
- `AllocateAndCopyMembers`, `GrowCapacityWithCopy`, and the growth rule.
- `ControllerKeyframe::fVisibleArea`, `SyncData::fVisibleArea`, and the
  `pfVisibleAreas` column.
- Every other unused name in these headers.
- Renaming `ControllerKeyframe` or `ControllerType`, and every field of them.
- The Puffs and WindRadials controller types (`Puffs.h:19-51`,
  `WindRadials.h:13-38`).
- The add and seed callbacks, `InterpolateKeyframes`, and
  `ControllerTypeRegistry` beyond its default argument.
- A removal-dispatch helper shared with `CompactCollections`
  (`FrameUtils.h:401-408`): more than the minimum.
- `RemoveIndexableElement` and `DestroyElement` themselves.

## Risk tier

Tier 2 — scoped behavior. Triggers: public members are removed, a public
return type changes, a public template's default argument is removed, two
public types change header, the public signatures of two framework helpers
change, and runtime behavior of one subsystem changes (one fewer lighting blur
texture pair is created); Tier 1 excludes each. The controller-helper change
stays inside the client-only controlled collections. No determinism/CRC,
serialization, layout, wire, or threading exposure.

## Acceptance criteria

- Neither `PuffsType` nor `SmokeTrailsType` has a `uiCrc` member.
- None of `AllocateAndCopyIds`, `Uuid::Write`, `Uuid::Read`, `Id<T>::Write`,
  `Id<T>::Read`, or `PointLightsType::fVisibleArea` remains, and
  `GrowPairedCollections` returns `void`.
- `CollectionController.h` defines neither `ControllerKeyframe` nor
  `ControllerType`, and `ControllerTypeRegistry` has no default controller
  type.
- No call to `DestroyExpiredControlled` or `AddControlledElement` passes a
  removal or grow lambda.
- `/compile` builds the BrokenEngineSandbox client and server.
- Through `/agent-harness`, one run with an explosion and smoke trails on
  screen observes: the explosion still shows its point lights and puffs, and
  they disappear when their controllers end; and a screenshot with puffs and
  smoke trails on screen still shows both. The supplied evidence records no
  harness setup.
- The executing session confirms no private-game caller of
  `AllocateAndCopyIds`, the four `Write`/`Read` members,
  `DestroyExpiredControlled`, or `AddControlledElement`, no private-game use of
  `GrowPairedCollections`' return value, `ControllerKeyframe`,
  `ControllerType`, or `PointLightsType::fVisibleArea`,
  and no private-game initializer setting `PuffsType::uiCrc` or
  `SmokeTrailsType::uiCrc`, by building the private game where its source is
  available, and reports it as unconfirmed otherwise.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
