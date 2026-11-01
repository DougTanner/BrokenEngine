# Ship Navigation Clearance

## Deferred requirement

The author records the user's requirement that navigation clearance account for each ship's actual
size. During the saved Battleship valley-stall diagnosis, the user permitted broader work to move to
a follow-up so the immediate escape behavior could be fixed first. On 2026-11-01 the user chose the
approach below in place of clearance derived from model bounds. Which ship goals use navigation is
outside this record.

The active valley-stall fix remains responsible for escaping toward the next navigation point behind the ship, including the reached steering/thrust behavior. This follow-up does not defer that acceptance or claim that undersized clearance caused that saved stall.

## Confirmed gaps

- `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.h:31`, `NavigationClearanceMeters` and `NavigationThresholdElevation`, use only `kfOmniRadius` and `kfPushMargin`. Their definitions in `Collections/Omnis/Omnis.h:44` and `:46` yield about 1.83337 m clearance, while `Collections/Battleships/Battleships.h:27` sets the Battleship contact radius to 4.5 m. These collection paths are relative to the same game Frame directory.
This is an existing sizing mismatch, independently of the saved stall.

Evidence was checked at `60a977a7`. Line numbers may move; the named symbols own the evidence.

## Approved clearance approach

The user approved these choices on 2026-11-01. They replace the earlier bounds-owner, bounds-envelope, graph-policy and contact-coverage decisions. Runtime speed matters more than ships passing close to terrain.

- Bake one navigation graph per size class: small (Fighter and Omni) and large (Battleship). Each ship queries its class's graph. Query cost is unchanged; bake time, wire bytes and client navigation memory roughly double, so the large class bakes only the flight planes Battleships use (`kiBattleshipHighestFlightPlane`).
- A class's clearance is its largest member's terrain-contact radius plus `kfPushMargin`, and its threshold elevation is the plane height minus that clearance. These replace the Omni-only `NavigationClearanceMeters` and `NavigationThresholdElevation`. Terrain contact stays a disc of the per-type contact radius and its sizing does not change.
- Clearance does not come from model data. A turning ship sweeps its whole enclosing circle, and disc terrain contact would still block a route sized to the `ModelOutline` hull. With no pack or model data, no pack format changes and nothing needs to move ahead of the startup bake (`Main.cpp`, `WaitForElevationMaps`, which runs before `InitializeShipHitboxes` in `Game.cpp`).
- Rejected: one shared graph inflated for the largest ship (inflation of about 5.23 m on each side closes every channel narrower than about 10.5 m at the threshold contour to Fighters and Omnis); one graph that stores distance to terrain with the query taking a radius (every visibility test becomes a swept disc, and corners baked at the small radius clip a larger ship); planning with the oriented hull (search grows to position and heading, orders of magnitude slower).
- The change is CRC-visible steering for every ship type and bumps `kiNavDataVersion`, so it is Tier 3.

## Implementation boundary and verification to preserve

The eventual Plan covers per-class clearance and elevation inputs to the existing navigation bake,
per-class contours and cell graphs, ship-type-to-class graph selection, and every navigation query
caller that must pass the ship's class. Preserve lifecycle steering locks, destination selection and
deterministic RNG behavior. No new pathfinding algorithm, unrelated hitboxes, asset tuning, or saved-format
compatibility reader is requested. Bump owning packed/navigation/save identities when their
contracts change; retain strict admission.

Execution is Tier 3 if it changes CRC-visible steering, packed layout/trust boundaries, or integration across DataPacker, File and game simulation. Use the normal plan review and approval workflow. Verify through `/compile` and `/agent-harness`, without unit tests:

- For each size class, exercise a passage narrower than the large class's clearance allows but wide enough for the small class's: small ships take it and Battleships route around it. Show that navigation covers each type's contact disc.
- Rebuild current packs and affected consumers, then compare repeated final replay tick CRCs and client/server results. The supplied preparation records no harness command sequence reproducing these deferred gaps; construct and record the smallest controlled scenarios before implementing them.

## Related records

No existing executable Plan owns the clearance gap.
[FlagshipPinnedAgainstTerrainBlocksFighterLaunch.md](FlagshipPinnedAgainstTerrainBlocksFighterLaunch.md) records a separate pinning symptom and unproven hypotheses; it does not establish this saved stall's cause. Keep that boundary when promoting this record to a decision-complete Plan.
