<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T14:22:36.710Z","dependsOn":[]} -->
# Move the point-light controller types out of the framework header

## Context

Found by the architecture review phase of a deep analysis of
`Engine/Source/Frame/Collections` (2026-11-14). Line numbers are from commit
`ae681f03` plus the change this Plan landed with; locate by symbol if they
moved. `C/` abbreviates `Engine/Source/Frame/Collections/`.

`ControllerKeyframe` and `ControllerType` (`C/CollectionController.h:15-55`)
carry point-light fields and are named only at
`C/PointLights/PointLightsUpdate.cpp:39,41,119`. The default template argument
`CONTROLLER_TYPE = ControllerType` (`CollectionController.h:107`) has one user,
`C/PointLights/PointLights.h:31`. The two sibling controlled collections
define their types in their own leaf headers (`C/Puffs/Puffs.h:19-51`,
`C/WindRadials/WindRadials.h:13-38`). No file under `Projects/` names either
type.

So a single-collection type sits in a framework header the server also
compiles, and the three siblings do not match.

Unmet criterion: the framework controller header holds no single-collection
type (root `AGENTS.md` `## Directives`, "Mirrored patterns stay parallel").

## Design

1. Move `ControllerKeyframe` and `ControllerType` to `PointLights.h`.
2. Drop the default argument at `CollectionController.h:107` and name the type
   explicitly at `PointLights.h:31`.
3. Fix the comment at `CollectionController.h:105`, which describes the
   default.

`PointLights.h` is whole-file `BT_CLIENT`, so the inner guard at
`CollectionController.h:46-52` goes with the move. `C/Puffs/Puffs.h:47-48`
uses `Wrapper*` with no declaration of its own and relies on the forward
declaration at `CollectionController.h:6-8`: that declaration stays, or
`Puffs.h` gains one.

Recommended: keep the type names as they are; a rename to match the siblings'
naming is not needed for the criterion.

A static controller registry: no CRC, serialization, or layout exposure.

## Critical files

- `Engine/Source/Frame/Collections/CollectionController.h`
- `Engine/Source/Frame/Collections/PointLights/PointLights.h`

## In scope

- `ControllerKeyframe` and `ControllerType`: moved unchanged.
- The default template argument of `ControllerTypeRegistry` and the comment
  above it.
- The base-class template arguments of `PointLightsInterpolate`.

## Out of scope

- Renaming either type, and every field of them.
- `InterpolateKeyframes`, `AddControlledElement`, `DestroyExpiredControlled`.
- The Puffs and WindRadials controller types.

## Risk tier

Tier 2 — scoped behavior. Trigger: a public template's default argument is
removed and two public types change header, which Tier 1 excludes. No
determinism/CRC, serialization, layout, wire, or threading exposure.

## Acceptance criteria

- `CollectionController.h` defines neither `ControllerKeyframe` nor
  `ControllerType`, and `ControllerTypeRegistry` has no default controller
  type.
- `/compile` builds the BrokenEngineSandbox client and server.
- The executing session confirms no private-game use of either type by
  building the private game where its source is available, and reports it as
  unconfirmed otherwise.

## Coordination

`Documents/Plans/Engine/ControlledCollectionOwnsGrowAndRemove.md` edits the
same header, `CollectionController.h`, and both touch what that header
declares or sees ahead of it. Whichever lands second re-derives its regions
from the landed header.
