# Ship Navigation Clearance and Destination Coverage

## Deferred requirement

The author records the user's requirement that every low- or medium-plane ship with a destination
use navigation waypoints, and that navigation clearance derive from the model's real bounding box
after pre-scale. During the saved Battleship valley-stall diagnosis, the user permitted broader work
to move to a follow-up so the immediate escape behavior could be fixed first. This record preserves
that deferred requirement; no architecture has been approved. It belongs in Investigations until the
choices below are resolved.

The active valley-stall fix remains responsible for escaping toward the next navigation point behind the ship, including the reached steering/thrust behavior. This follow-up does not defer that acceptance or claim that undersized clearance caused that saved stall.

## Confirmed gaps

- `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.h:31`, `NavigationClearanceMeters` and `NavigationThresholdElevation`, use only `kfOmniRadius` and `kfPushMargin`. Their definitions in `Collections/Omnis/Omnis.h:33` yield about 1.83337 m clearance, while `Collections/Battleships/Battleships.h:27` sets the Battleship contact radius to 4.5 m. These collection paths are relative to the same game Frame directory. This is an existing sizing mismatch, independently of the saved stall.
- `Common/DataFile.h:91` (`SceneHeader`) and `:306` (`ModelHeader`) carry no drawn bounds. The latter carries alignment rotation, scale and offset. `Engine/Source/File/PackChunks.cpp:104` admits only Islands on the server; eager Scene/Model payload loading at `:458` is client-only. Model-derived simulation clearance needs an explicit producer and shared metadata consumption contract.
- `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:406`, `AccumulateDrawnBounds`, already measures drawn indexed geometry with mesh/skin transforms. Reuse is available; raw vertex extrema or an assumed normalized cube do not establish the actual drawn bounds after normalization and pre-scale. The glTF and Assimp decoder normalization paths and main-only Scene export must be considered.
- Fleet destinations already enter `ComputeNavigation` in
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp:331`. Random-spawn
point branches instead call `ComputeFleeOrReturnDestination` and steer directly in Fighters.cpp:625,
Battleships.cpp:618 and Omnis.cpp:569 under their respective collection directories. Actual
return/chase point goals therefore need coverage too. Fighter/Battleship flee uses a threat position
with negative turn rate; that does not automatically define an escape destination.

Evidence was checked against baseline `382fa066095c8edeab39c4e67a5690846724e4f3`. Line numbers may move with the active fix; the named symbols own the evidence.

## Decisions before an executable Plan

1. Choose the bounds owner and server metadata route. Scene-header ownership and model-header ownership remain alternatives; neither is selected here. Prefer reusing the existing drawn-bounds producer and packed-header mechanisms where their contracts fit.
2. Define the bounds envelope: default pose versus animation/banking/tilt coverage; account for pre-scale, offset, fixed alignment and the actual renderer transform order. Omni's order differs from Fighter/Battleship, so nonuniform scale must not be silently commuted with heading.
3. Choose shared per-plane graphs versus model-specific graphs. Shared maxima can exclude passages a smaller ship could otherwise use; model-specific graphs change graph selection and identity. Resolve the route-access tradeoff before implementation.
4. Define how navigation covers existing terrain contact and push margins, and whether terrain-contact sizing itself changes. Model-derived navigation alone must not admit a route the existing contact footprint cannot traverse. Do not replace this requirement with new fixed radii.
5. Define exactly which point goals count as destinations. Cover every controller's actual low/medium destination, including random-spawn return/chase goals. Decide flee semantics explicitly if replacing inverse threat steering is desired; do not invent an endpoint or escape distance.

## Implementation boundary and verification to preserve

The eventual Plan covers the chosen bounds producer/storage/reader path, game model-to-controller
mapping, clearance and elevation inputs to the existing navigation bake, and dependent destination
query callers. Preserve lifecycle steering locks, destination selection and deterministic RNG
behavior. No new pathfinding algorithm, unrelated hitboxes, asset tuning, or saved-format
compatibility reader is requested. Bump owning packed/navigation/save identities when their
contracts change; retain strict admission.

Execution is Tier 3 if it changes CRC-visible steering, packed layout/trust boundaries, or integration across DataPacker, File and game simulation. Use the normal plan review and approval workflow after resolving the choices. Verify through `/compile` and `/agent-harness`, without unit tests:

- Compare persisted bounds with transformed drawn geometry for all used models, covering static/skinned producers, pre-scale, nonzero offset and nonuniform scale; demonstrate that a pre-scale change changes derived clearance.
- Show waypoint steering and sustained progress around terrain for every permitted low/medium type/controller with an actual point destination; include the direct random-spawn branches, moving targets and lower-plane transitions. Preserve the separately agreed flee semantics and no-goal behavior.
- Exercise a narrow passage and a safe route under the selected graph policy; show that navigation covers the approved model/contact envelope. Verify server startup consumes required immutable metadata without unnecessary mesh/animation/GPU payloads, main-only export publishes it, and stale/corrupt required metadata is rejected.
- Rebuild current packs and affected consumers, then compare repeated final replay tick CRCs and client/server results. The supplied preparation records no harness command sequence reproducing these deferred gaps; construct and record the smallest controlled scenarios before implementing them.

## Related records

No existing executable Plan owns these gaps. [FlagshipPinnedAgainstTerrainBlocksFighterLaunch.md](FlagshipPinnedAgainstTerrainBlocksFighterLaunch.md) records a separate pinning symptom and unproven hypotheses; it does not establish this saved stall's cause. [ModelShapedHitboxes.md](../../Features/Frame/ModelShapedHitboxes.md) explicitly excludes terrain collision and navigation clearance. Keep those boundaries when promoting this record to a decision-complete Plan.
