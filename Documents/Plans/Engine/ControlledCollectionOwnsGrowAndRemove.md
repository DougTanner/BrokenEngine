<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T14:22:35.341Z","dependsOn":[]} -->
# Let the controlled-collection helpers own growing and removing rows

## Context

Found by the architecture review phase of a deep analysis of
`Engine/Source/Frame/Collections` (2026-11-14). Line numbers are from commit
`ae681f03` plus the change this Plan landed with; locate by symbol if they
moved. `C/` abbreviates `Engine/Source/Frame/Collections/`.

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

Unmet criterion: a controlled collection supplies only the callbacks that
differ per collection (root `AGENTS.md` `## Directives`, DRY).

## Design

Recommended:

1. `DestroyExpiredControlled` removes the expired row itself — the
   id-indexed removal when the collection has an id-to-index map, the plain
   swap-and-pop otherwise — and steps the index back, so the revisit rule has
   one owner. Its removal-callback parameter goes.
2. `AddControlledElement` takes the PostRender collection and calls
   `GrowPairedCollections` itself. Its grow-callback parameter goes.
3. The six call sites drop the two lambdas each no longer passes.

Row order after a removal stays the same forward walk. All three collections
are client-only (`Engine/Source/Frame/FrameBase.h:244-251` lists them only
under `BT_CLIENT`), so nothing reaches the shared CRC or the wire.

Two things the change must also handle:

- `C/Collection.h:3-6` includes `CollectionController.h` before
  `CollectionLifecycle.h` and `CollectionMemory.h`, which declare
  `HasIdToIndex`, `GrowPairedCollections`, and both removal functions.
  `CollectionController.h` therefore includes `CollectionLifecycle.h` itself,
  which already includes `CollectionMemory.h` (`CollectionLifecycle.h:3-4`)
  and names nothing `CollectionController.h` declares. `Collection.h` is the
  only file that includes `CollectionController.h`, and it stays unchanged.
- `C/AGENTS.md` `## Identity, Serialization, and CRC` names "a
  `DestroyExpiredControlled` callback" and needs matching wording.

## Critical files

- `Engine/Source/Frame/Collections/CollectionController.h`
- `Engine/Source/Frame/Collections/PointLights/PointLights.cpp`,
  `PointLightsUpdate.cpp`
- `Engine/Source/Frame/Collections/Puffs/Puffs.cpp`, `PuffsUpdate.cpp`
- `Engine/Source/Frame/Collections/WindRadials/WindRadials.cpp`,
  `WindRadialsUpdate.cpp`
- `Engine/Source/Frame/Collections/AGENTS.md`

## In scope

- `DestroyExpiredControlled` and `AddControlledElement`: parameters and
  bodies.
- The three `Destroy` call sites and the three `AddControlled` call sites.
- The `CollectionLifecycle.h` include added to `CollectionController.h`.
- The sentence in `C/AGENTS.md` that names the callback.

## Out of scope

- The add and seed callbacks, `InterpolateKeyframes`, and
  `ControllerTypeRegistry`.
- A removal-dispatch helper shared with `CompactCollections`
  (`FrameUtils.h:401-408`): more than the minimum.
- `RemoveIndexableElement` and `DestroyElement` themselves.

## Risk tier

Tier 2 — scoped behavior. Trigger: the public signatures of two framework
helpers change, which Tier 1 excludes; the change stays inside the client-only
controlled collections. No determinism/CRC, serialization, wire, or threading
exposure.

## Acceptance criteria

- No call to `DestroyExpiredControlled` or `AddControlledElement` passes a
  removal or grow lambda.
- `/compile` builds the BrokenEngineSandbox client and server.
- Through `/agent-harness`, an explosion still shows its point lights and
  puffs, and they disappear when their controllers end. The supplied evidence
  records no harness setup.
- The private game's C++ source was not available to the authoring session.
  The executing session confirms no private-game caller of either helper by
  building the private game where its source is available, and reports it as
  unconfirmed otherwise.

## Coordination

`Documents/Plans/Engine/MovePointLightControllerTypes.md` edits the same
header, `CollectionController.h`, and both touch what that header declares or
sees ahead of it. Whichever lands second re-derives its regions from the
landed header.
