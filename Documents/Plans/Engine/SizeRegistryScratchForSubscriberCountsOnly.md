<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T22:26:50.575Z","dependsOn":[]} -->
# Size Registry Scratch for Subscriber Counts Only

## Context

`BuildRegistryQueryContext` (`Engine/Source/Frame/FrameRegistry.cpp:108`) takes one `scratch`
span whose shape depends on whether a subscription layer is bound. `RegistryScratchBytes`
(`Engine/Source/Frame/FrameRegistry.h:64`) sizes a block laid out as the caller's eligible row
indices followed by the `uint16_t` subscriber counts, so the caller writes its
`RegistrySourceLayer::rows` into the front of the registry's own scratch. A context built with no
subscription layer derives no counts and takes only that row prefix. The result is a two-shape
contract:

- The size check at `FrameRegistry.cpp:119` has two arms: rows only when `subscriptionLayers` is
  empty, otherwise `RegistryScratchBytes`.
- The counts start at an offset past the rows (`FrameRegistry.cpp:127-128`).
- The header describes both shapes twice: `FrameRegistry.h:59-63` (with the no-subscription
  shape at :61-62) and the declaration comment at :69-70.
- The sole caller, `BuildShipRegistryWindow`
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipQueries.cpp:218`), binds no
  subscription layer and passes a rows-only `scratch` span (:243, :280-281), with a comment
  explaining the rows-only shape (:241).
- `Engine/Source/Frame/AGENTS.md` `## Frame Registry` states that `RegistryScratchBytes` sizes
  the eligible-row and subscriber-count storage and that a context with no subscription layer
  needs scratch for its row indices only, and
  `Documents/Architecture/FrameUpdatePipeline.md` `## Frame Registry Query Windows` calls the
  window's eligible source-row indices "its whole registry scratch".

Root cause: the row indices are caller data, because the caller fills them and binds them into
`RegistrySourceLayer::rows` before the context exists, yet they are counted as registry scratch.
That makes the scratch size depend on whether counts are derived.

Originating gap: an `/over-engineering-review` design suggestion that the user deferred to a
follow-up Plan when they chose to have counts derived only when a subscription layer is bound.
The rows-in-scratch layout predates that change (at `8fa794fa`, `RegistryScratchBytes` already
summed `sizeof(int64_t) + sizeof(uint16_t)` per eligible row).

## Design

The author recommends moving the row indices out of the registry scratch, because the scratch
then holds only what the registry owns and has one shape:

- `RegistryScratchBytes(iEligibleRows)` returns `iEligibleRows * sizeof(uint16_t)`, the
  subscriber counts alone.
- `BuildRegistryQueryContext` places the counts at `scratch.data()`. Its size check takes one
  form, required only when a subscription layer is bound, for example
  `ASSERT(subscriptionLayers.empty() || static_cast<int64_t>(scratch.size()) >= RegistryScratchBytes(iEligibleRows));`.
  The comment at `FrameRegistry.cpp:127` about the row prefix goes away.
- The header comment above `RegistryScratchBytes` describes the subscriber-count block only, and
  the declaration comment at :69-70 states that `scratch` is that block, empty when no
  subscription layer is bound.
- `BuildShipRegistryWindow` keeps its eligible rows in its own part of the same workbuffer
  reservation and passes an empty `scratch` span. The reservation size, the row and layer
  offsets, and the row values stay as they are; only the naming of the row-byte count
  (`iScratchBytes`) and the two comments that call the rows scratch (:239-241, :250) change.
- The two documents above drop the rows-as-scratch wording.

## Critical files

- `Engine/Source/Frame/FrameRegistry.h`
- `Engine/Source/Frame/FrameRegistry.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipQueries.cpp`
- `Engine/Source/Frame/AGENTS.md`
- `Documents/Architecture/FrameUpdatePipeline.md`

## In scope

- `RegistryScratchBytes`: its return value and its comment (`FrameRegistry.h:59-67`).
- The `BuildRegistryQueryContext` declaration comment (`FrameRegistry.h:69-70`).
- `BuildRegistryQueryContext`: the scratch-size ASSERT (`FrameRegistry.cpp:119`) and the
  subscriber-count placement and its comment (`FrameRegistry.cpp:125-131`).
- `BuildShipRegistryWindow`: the `scratch` span passed to `BuildRegistryQueryContext`, the
  row-byte count's name, and the comments at :239-241 and :250.
- `Engine/Source/Frame/AGENTS.md` `## Frame Registry`: the `RegistryScratchBytes` sentence and
  the no-subscription scratch clause.
- `Documents/Architecture/FrameUpdatePipeline.md` `## Frame Registry Query Windows`: the
  "(its whole registry scratch)" parenthetical.

## Out of scope

- Removing `RegistrySubscriptionLayer`, `AcquireRegistryTargets`, or subscriber counting, even
  though no caller binds a subscription layer or acquires today.
- The acquisition ranking, `ResolveRegistryHandle`, `RegistryBatch`, and the ownership layer.
- Which ships `BuildShipRegistryWindow` admits, its layer order, and its bound columns.
- The "64 KiB" workbuffer wording in `FrameUpdatePipeline.md`, owned by
  `Documents/Plans/Engine/DropCopiedWorkbufferSizeFromFrameUpdatePipeline.md`.

## Risk and invariants

- Tier 3. Trigger: a change spanning independently owned subsystems, the engine frame registry
  contract and its game caller in Ship queries. It changes nothing the CRC surface computes.
- Determinism: PostRender stays bit-identical. The window binds no subscription layer, so no
  subscriber count is derived before or after, and `ResolveRegistryHandle`, the window's only
  query, reads only the source layers, whose row values and order do not change. Nothing the
  registry produces is serialized, CRC'd, or transferred (`Engine/Source/Frame/AGENTS.md`
  `## Frame Registry`).
- No wire, save, replay, `.pack`, shader, or threading exposure. Allocation stays one workbuffer
  reservation per window, formed before any pointer is published.
- The subscriber counts need `uint16_t` alignment at `scratch.data()`; a future subscription
  caller supplies it, and the empty span the ship window passes needs none.

## Acceptance criteria

- `RegistryScratchBytes` sizes the subscriber counts alone, and the scratch-size ASSERT in
  `BuildRegistryQueryContext` has one form.
- `BuildShipRegistryWindow` passes an empty `scratch` span, and its reservation still holds the
  eligible rows and the layer descriptors.
- `FrameRegistry.h` describes one scratch shape, and neither document calls the row indices
  registry scratch.
- The Sandbox client and server build in Debug and Release through `/compile`.
- A Debug client/server `/agent-harness` run in which Battleships launch Missiles reaches the
  Missile-Update window with no ASSERT, and a replay determinism check passes. The supplied
  evidence records no setup for such a run.

## Coordination

No directional prerequisites. Reciprocal coordination with
`Documents/Plans/Engine/DropCopiedWorkbufferSizeFromFrameUpdatePipeline.md`: both Plans edit the
second paragraph of `Documents/Architecture/FrameUpdatePipeline.md`
`## Frame Registry Query Windows`, so whichever lands second refreshes that paragraph. Each Plan
keeps its own acceptance.

## Notes

- `BuildShipRegistryWindow` is the only caller of `BuildRegistryQueryContext`, and
  `BuildRegistryQueryContext` is the only reader of `RegistryScratchBytes`.
