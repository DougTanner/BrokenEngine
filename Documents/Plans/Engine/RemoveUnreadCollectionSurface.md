<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T14:22:31.605Z","dependsOn":[]} -->
# Remove collection members and results that nothing reads

## Context

Found by a deep analysis of `Engine/Source/Frame/Collections` (2026-11-14):
items 1, 2, and 4 by its architecture review phase, item 3 by its in-function
analysis phase. Line numbers are from commit `ae681f03` plus the change this
Plan landed with; locate by symbol if they moved. `C/` abbreviates
`Engine/Source/Frame/Collections/`.

Four exported pieces of the collection framework have no reader:

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

No use of any of these exists in Engine, BrokenEngineSandbox, ModelViewer,
Tools, or DataPacker. The private game's C++ source was not available to the
authoring session, so "no private-game caller" is unconfirmed.

Unmet criterion: the collection framework exposes no entry point, member, or
result that nothing reads.

## Design

1. Delete `AllocateAndCopyIds`, and reword the two feature-document lines that
   name it so they name `AllocateAndCopyMembers`.
2. Delete `Uuid::Write`, `Uuid::Read`, `Id<T>::Write`, and `Id<T>::Read`.
3. Change `GrowPairedCollections` to return `void` and delete its two `return`
   values.
4. Delete `PointLightsType::fVisibleArea` and the `.fVisibleArea` line of the
   `PointLightsType` initializer at `Explosions.cpp:40`.

Nothing here changes behavior: an uncalled template is never instantiated, the
deleted members are non-virtual so `Id`/`Uuid` size and column layout are
unchanged, the growth rule is untouched, and every `PointLightsType`
initializer uses designated members.

## Critical files

- `Engine/Source/Frame/Collections/CollectionMemory.h`
- `Engine/Source/Frame/Collections/CollectionId.h`
- `Engine/Source/Frame/Collections/CollectionLifecycle.h`
- `Engine/Source/Frame/Collections/PointLights/PointLights.h`
- `Engine/Source/Frame/Collections/Explosions/Explosions.cpp`
- `Documents/Features/Graphics/DecalSystem.md`
- `Documents/Features/Graphics/FlipbookSpriteAnimations.md`

## In scope

- The four deletions and the one return-type change listed under `## Design`.
- The one line in each of the two feature documents that names
  `AllocateAndCopyIds`.

## Out of scope

- `AllocateAndCopyMembers`, `GrowCapacityWithCopy`, and the growth rule.
- `ControllerKeyframe::fVisibleArea`, `SyncData::fVisibleArea`, and the
  `pfVisibleAreas` column.
- Every other unused name in these headers.

## Risk tier

Tier 2 — scoped behavior. Trigger: the change removes public members and
changes a public return type, which Tier 1 excludes. No determinism/CRC,
serialization, layout, wire, or threading exposure.

## Acceptance criteria

- None of `AllocateAndCopyIds`, `Uuid::Write`, `Uuid::Read`, `Id<T>::Write`,
  `Id<T>::Read`, or `PointLightsType::fVisibleArea` remains, and
  `GrowPairedCollections` returns `void`.
- `/compile` builds the BrokenEngineSandbox client and server.
- The executing session confirms no private-game caller by building the
  private game where its source is available, and reports it as unconfirmed
  otherwise.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
