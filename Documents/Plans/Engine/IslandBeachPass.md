<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-06T03:27:40.824Z","dependsOn":[]} -->
# Production beach pass: SandbarsMost on every island

## Context

Delivered island heights carry a flat shelf at sea level: on `01/1x1/0`, 3014.1 m2
of terrain sits within 2 cm of Z 0. A bake-off in an earlier session compared
fourteen height techniques on `01/1x1/0` in game, and the user chose SandbarsMost.
This Plan makes that technique the real beach on all 70 island leaves. DataPacker
edits each leaf's heights while it builds the leaf's chunk, and two named constants
set how much of the shelf becomes sandbars and how deep they sit. The mesh and every
texture ship as delivered.

The bake-off never landed. Its code and its leaves exist in no commit, so every
fact this Plan needs from it is written here, and `BeachPass.{h,cpp}` are new files.
`## Reference implementation` holds the reviewed code.

Citations: every `path:line` reads commit
`dcd383aee847d5153f4f0691b763163962ca855b`, an ancestor of primary. Resolve one with
`git show dcd383ae:<path>`. Primary has moved since, so find the same code by the
named symbol. `## Primary after the baseline` records each later change this Plan
depends on, and where it and a baseline citation differ, that section is current.

Terms:

- Leaf: one island template, `Engine/Data/Islands/<island>/<route>/<n>`. There are
  70, 47.6 M texels in total.
- Source heights, `s`: the floats in a leaf's delivered `Elevation.r32`.
- Distance, `d`: Euclidean distance in meters to the nearest source texel at or above
  the land line, `kfLandMeters` (0.1 m).
- Shelf texel: `|s| <= 0.25`, `s < kfLandMeters`, and `d >= kfGateEndMeters` (22 m).
- Layered: `ApplyDistanceProfile` then `ApplyHeightCurve`. It removes the shelf and
  steepens what remains around Z 0.
- Bar height: `min(s, max(curve(s) - depth, kfHeightFloorMeters))`, where `curve` is
  the height curve and `depth` is `kfBeachBarDepthMeters`.
- SandbarsMost: Layered, then a lerp toward the bar height past a trough, weighted
  by a low-frequency patch field. The reviewed variant used field edge 0.05 and depth
  0.15 m.

## Recorded user choices

This is the authoring agent's record. `.agents/references/authority-order.md` says
when a Plan decision binds.

The user picked the technique and asked for the constants: "Ok, let's go with
\"SandbarsMost\", add constants so that I can test out different values myself later,
for both coverge and depth." The scope stated with it is DataPacker only, the Gaea
graph untouched, hand-written C++, and no new ThirdParty. This Plan reads "go with" as
production for all islands; the user was told so.

The user then settled four questions with "go with your recommendations":

- U1: the pass runs at the chunk stage in `ExportIsland`, on the delivered tapered
  `Elevation.r32` at leaf resolution. Tracked island data does not change, and a
  retest is edit constant, rebuild, launch.
- U2: normals stay as baked for now. One BC5 re-encode happens later, once, after the
  user freezes the constants (`## Planned follow-ups`).
- U3: a leaf with no shelf gets the beach smoothing and no bars.
- U4: the constant is a percentage with a per-leaf solve, and there is no mesh rebuild:
  no vertex Z resample, no re-subdivision. `MeshProcessed.bin` ships as delivered and
  the `kfBeachSubdivision*` constants are not shared.

Author's recommendations, none of them the user's words:

- Default `kfBeachBarCoveragePercent` 91.6: the coverage the reviewed SandbarsMost
  measured. The U4 question presented 92, and the user was then told the default would
  be 91.6, the value of the look they approved.
- Default `kfBeachBarDepthMeters` 0.15: the reviewed depth, see `### 6`.
- No minimum shelf. One small piece is exposed: `01/8x4/3` has 0.2 m2 of shelf, which
  sets the edge for up to 355 m2 of water there. A minimum would add a third constant
  for one leaf. Check 9 puts that leaf in front of the user, and the choice can be
  reconsidered at that review.
- The pass runs before `common::kfIslandScale`, see `## Primary after the baseline`.

## Primary after the baseline

Five commits landed after the baseline, ending at `66b6dd10`. That last commit bumps
`DataHeader::kiVersion` and changes nothing here. The rest change what this Plan
touches:

- `common::kfIslandScale` (`common::kiIslandScalePermille`, 1158 at this writing,
  `Common/DataFile.h`). `ExportIslandData` now multiplies the footprint, mesh XY and
  every elevation sample above 0 by it, in a loop right after the elevation read, which
  is this Plan's insertion site. Samples at or below 0 stay, so the threshold, the hull
  side test and the masks do not move. The hull and peak are derived after that loop.
- `ExportIsland::kiVersion` is now `Version(34 + common::kiIslandScalePermille)`. The
  chunk file carries that sum plus `sizeof(common::ChunkHeader)`, 376, so primary's
  island chunks are `<n>.v1568.chunk`, not the `.v409.chunk` of the baseline. No check
  below assumes either number: `V` means the value of `kiVersion` before this change.
- A save stores no placements. `ReadGridSave` regenerates them, and a change to
  `GenerateIslandChain`'s output or to the island set it picks from takes a bump of
  the game's `Frame::kiVersion` base literal (`Engine/Source/Frame/AGENTS.md`
  `## Terrain and Navigation`, `Engine/Source/File/AGENTS.md` `## Grid Saves`).
- Chains grow over regions, and a cell can end up island-free when filler fails, so
  `### 7` and check 7 no longer assume every cell holds an island. Placement still reads
  hulls and footprints only, never heights.

Author's recommendation on the scale: run the pass first, on the delivered floats in
delivered meters, and let the existing loop scale its output. That keeps the input the
bake-off reviewed, so every measurement in this Plan stays true, and the pass needs no
knowledge of the scale. Run after the loop, the land line, the curve band and the shelf
mask would see scaled land, the solved edges below would all move, and the
meters-per-texel would differ from the reviewed 0.1953 m. The cost is small: the
technique's meter values read about 16% wider on screen, as all island detail already
does. The user reviewed the unscaled look; check 9 is where the scaled look is judged.

## Design

### Approach

`ExportIslandData` applies the beach pass to each leaf's heights in memory, right
after reading the delivered `Elevation.r32` and before anything derives from them. The
two tuning constants live at the top of a new `BeachPass.h`, and
`ExportIsland::GetInputFingerprint` carries them, so editing one and rebuilding
re-exports the 70 island chunks and nothing else. The pass runs at leaf resolution on
the tapered heights, which is exactly the input the bake-off reviewed, on the same
delivered mesh every bake-off screenshot used. Every BC texture, `Elevation.r32`,
`MeshProcessed.bin` and `BakedIsland.json` stays byte-identical, so no tracked island
data changes, no Gaea authorization or raw cache is needed, and the hull, thumbnail
and placements do not move.

No library is in play: the code is tuning arithmetic plus a narrow edge solve, and the
recorded scope rules out new ThirdParty.

### 1. Coverage means the same on every island's shelf

The fitted edge does not travel. Over the 70 leaves, field edge 0.05 gives from 0.0%
to 100% coverage, against 91.6% on `01/1x1/0`: 7.6% on `03/1x1/0`, 18.1% on
`01/3x2/5`, 100% on `01/2x1/1` (table below).

Constant: `kfBeachBarCoveragePercent = 91.6f` (U4). It is the mean bar weight over a
leaf's shelf texels. For each leaf the pass bisects the field edge `E` so that mean
weight equals the constant.

The reviewed SandbarsMost measured 91.6 on `01/1x1/0`, so that is the default. It gives
that leaf the same look, not the same bytes: the solve gives 0.050101, and the reviewed
edge is exactly 0.05 (the 91.6 is itself a rounded measurement). Check 5 bounds the
difference on the default build.

- Solve: mean of `Smoothstep(E + kfPatchSoftness, E - kfPatchSoftness, field)` over the
  shelf texels is monotone in `E`. A fixed 24 bisection steps over `[-1 - soft, 1 + soft]`
  is deterministic and costs nothing: the largest shelf is 661 m2, about 17 k texels.
- A plain quantile is the simpler alternative, and the author recommends against it.
  On `01/1x1/0` it gives 0.041, while bisection gives 0.050, the reviewed edge to three
  places. The soft edge is why they differ.
- A leaf with no shelf texels has nothing to cover, so it gets no bars and comes out
  exactly as Layered (U3). That is 50 of 70 leaves, including `02/1x1/0`. Bars only
  lower source heights, so they cannot appear where the source has no shallows. This is
  a real visible consequence.
- 0% gives Layered everywhere: the edge converges to the bottom of the range, where
  every weight is 0 and `std::lerp` returns its first argument. 100% gives full bars
  over the shelf.
- The percentage holds on the shelf only. The solved edge then applies to all water at
  or above the floor past `kfGateStartMeters`, which can be far more than the shelf:
  `01/8x4/3` has 0.2 m2 of shelf and up to 355 m2 of that water, and `03/1x1/0` has
  16.1 m2 and up to 7712 m2. How much non-shelf water gets bars therefore varies by
  leaf.

The 20 leaves with shelf. `Shelf` is shelf area, `Water` is water at or above the floor
past 12 m, `0.05` is coverage at the reviewed edge, and `E` is the edge solved for
91.6. The other 50 leaves have no shelf texels.

| Leaf | Heightmap | Shelf m2 | Water m2 | 0.05 % | `E` |
| --- | --- | --- | --- | --- | --- |
| `01/1x1/0` | 2032x2032 | 571.0 | 9353 | 91.6 | 0.050 |
| `01/2x1/1` | 2032x1024 | 192.5 | 7199 | 100.0 | -0.189 |
| `01/2x2/0` | 1024x1024 | 42.4 | 2334 | 97.8 | -0.129 |
| `01/2x2/2` | 1024x1024 | 63.2 | 1861 | 100.0 | -0.766 |
| `01/2x2/3` | 1024x1024 | 157.2 | 4618 | 100.0 | -0.167 |
| `01/3x1/0` | 2032x688 | 69.7 | 4175 | 99.9 | -0.054 |
| `01/3x1/1` | 2032x688 | 4.1 | 2486 | 100.0 | -0.095 |
| `01/3x2/0` | 1024x688 | 11.7 | 1849 | 100.0 | -0.136 |
| `01/3x2/3` | 1024x688 | 661.3 | 4027 | 42.4 | 0.321 |
| `01/3x2/4` | 1024x688 | 71.3 | 1791 | 99.7 | -0.042 |
| `01/3x2/5` | 1024x688 | 284.6 | 2787 | 18.1 | 0.329 |
| `01/3x4/1` | 512x688 | 14.6 | 1421 | 91.3 | 0.051 |
| `01/3x4/10` | 512x688 | 7.2 | 1073 | 0.0 | 0.329 |
| `01/3x4/2` | 512x688 | 4.8 | 1060 | 99.9 | -0.010 |
| `01/3x4/7` | 512x688 | 18.8 | 1682 | 53.0 | 0.150 |
| `01/4x2/0` | 1024x512 | 12.6 | 1774 | 100.0 | -0.113 |
| `01/4x4/1` | 512x512 | 25.9 | 1451 | 97.7 | 0.012 |
| `01/8x4/3` | 512x256 | 0.2 | 355 | 100.0 | -0.080 |
| `02/2x1/0` | 2032x1024 | 66.1 | 4744 | 89.5 | 0.058 |
| `03/1x1/0` | 2048x2032 | 16.1 | 7712 | 7.6 | 0.354 |

### 2. Insertion point: chunk stage, leaf resolution, after the taper

**No seam exists.** A leaf is an independent template. `PickCrc` chooses each placement
from a bucket by seeded RNG (`Engine/Source/Frame/IslandChainPlacement.cpp:79-102`), and
links touch by hull at their own rotation
(`Engine/Source/Frame/IslandChainPlacement.cpp:106`,
`Engine/Source/Frame/IslandChainPlacement.cpp:181`). Two leaves of one route are never
joined along their cut line. The taper already treats interior cut lines as open ocean
(`DataPacker/Source/ExportJobs/Island/ProcessBakedRegion.cpp:129-133`). Measured: every
border texel of all 70 leaves sits below the threshold (highest -3.39 m) and no border
texel is land. The pass skips any texel below its floor, so it leaves every leaf's
outer ring byte-identical. That is stronger than seam matching, and check 3 keeps it.

**Leaf or raw resolution.** Leaf is 0.195 m and raw 0.049 m, and every island is 400 m
at 8192 (`Engine/Data/Islands/01/Island.json`). Time does not decide this: the technique
took 0.214 s on 4.1 M texels in the bake-off, so about 3.5 s per 8192 route and about
45 s for the 13 routes, against about 2.5 s for all 70 leaves. Other things decide it:

- The runtime only ever samples the box-filtered leaf
  (`DataPacker/Source/ExportJobs/Island/ProcessBakedRegion.cpp:104-127`), so raw gives
  no visible gain.
- The threshold invariant does not survive that filter. A raw pass keeps each raw texel
  on its side, but lowering some of a 4x4 block can pull its average across
  `common::kfUnderwaterMaskThresholdMeters`, which moves the hull and the masks.
- Raw needs the 17 GB raw cache and an authorized split. Leaf needs neither.
- A raw pass sits before the taper, so `01/1x1/0` would no longer match the heights the
  user approved.

**Parameters.** All are meters and stay valid. Every leaf has the same 0.1953 m/texel,
because the pass takes `baked.fWidthMeters / iElevationWidth`. The smooth radius (0.15 m)
ramps from zero over 2 m, and the rule that non-land texels sit at least one texel from
land holds at that scale. Small leaves are safe: on the 512x256 leaves (100 x 50 m) the
45 m blend covers most of the leaf and the patch wavelengths exceed it, so `PatchField`
is nearly constant, and the coverage solve absorbs that. `03/1x1/0` is 2048x2032;
`DistancesToLand` and `PatchField` take width and height separately.

**Routes and taper.** Per-leaf running gives 1x1, 2x1, 2x2 and all ten route labels
(`DataPacker/Source/ExportJobs/Island/BakeIslandIntermediates.cpp:49-61`) the same
treatment with no route-level logic. Delivered `Elevation.r32` is already tapered
(`DataPacker/Source/ExportJobs/Island/ProcessBakedRegion.cpp:339`), and water only ever
goes lower, so the taper's ceiling holds. Land stays 4.9 m or more from every border,
and the auto-crop is untouched because it ran during the split.

**Exact site.** `ExportIslandData`, right after the read that ends at
`rOut.iHeightmapHeight = iElevationHeight;`
(`DataPacker/Source/ExportJobs/ExportIsland.cpp:433-438`) and before the
`common::kfIslandScale` loop on primary, so the scale, peak
(`DataPacker/Source/ExportJobs/ExportIsland.cpp:441`), hull, masks, thumbnail and
quantization all see edited heights.

### 3. Retest loop

1. Edit `kfBeachBarCoveragePercent` or `kfBeachBarDepthMeters` in
   `DataPacker/Source/ExportJobs/Island/BeachPass.h`.
2. Build. In the primary checkout, a Visual Studio client or server build runs DataPacker
   (`.agents/skills/compile/references/runtime-data-mode.md:64`). In a worktree, a
   `/compile` game build is Local, because DataPacker differs from the stamp, with no
   extra flag.
3. Run the game. Nothing else.

| Question | Answer | Evidence |
| --- | --- | --- |
| Authorization | None. Not `BT_DATAPACKER_ALLOW_GAEA_EXPORT`, not `-AllowGaeaExport` | Published routes return before any Gaea path (`DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:580-583`) |
| Forbid vetoes | Both stay in force and pass. `-ForbidExpensiveExport` is the proof no encode ran | Published leaves skip the encode (`DataPacker/Source/ExportJobs/ExportIsland.cpp:624-629`); guard at `DataPacker/Source/ExportJobs/Texture/Texture.cpp:364` |
| Raw cache | Not needed | The chunk reads delivered files only (`DataPacker/Source/ExportJobs/ExportIsland.cpp:385`) |
| Invalidation | Automatic: both constants join `GetInputFingerprint` | `DataPacker/Source/ExportJobs/ExportIsland.cpp:544-561`, compared at `DataPacker/Source/ExportJobs/ExportJob.cpp:202-204` |
| Version bump per try | None. `ExportIsland::kiVersion` bumps once in this change | `DataPacker/Source/ExportJobs/ExportIsland.h:26`, `DataPacker/Source/ExportJobs/Island/AGENTS.md:87-91` |
| Tracked regeneration per try | None. `git status` stays clean under `Engine/Data` | Check 5 |

The chunk cache file name carries only the job version
(`DataPacker/Source/ExportJobs/ExportJob.cpp:143-149`), so two worktrees with different
constants re-export over each other when switched. The fingerprint keeps that correct;
it costs one chunk export.

**Why not the split stage.** A changed split input trips three things. An ordinary run
throws, including the user's Visual Studio build, unless authorized
(`DataPacker/Source/ExportJobs/Island/PublishedIslandBake.cpp:128-131`,
`DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:604-607`). Each try reruns all 13
route splits from the raw cache. And `Elevation.r32` is a texture input
(`DataPacker/Source/ExportJobs/ExportIsland.cpp:600`), so every try re-encodes four BC
textures for 70 leaves, unmeasured but the largest part of a full export, which can take
hours (`.agents/skills/compile/references/runtime-data-mode.md:116`). It also rewrites
tracked files on every try. That is not a loop a person repeats.

### 4. Derived data

| Output | Result | Why |
| --- | --- | --- |
| Elevation in chunk | Recomputed | The pass |
| Mesh XY and indices | Unchanged | U4; below |
| Peak | Unchanged | The pass changes only texels below the land line (`kfLandMeters`) or inside the curve band (`|z| < 1` m), and shipped leaves peak at 7 m or higher (`DataPacker/Source/ExportJobs/Island/BakeIslandIntermediatesInternal.h:26`) |
| Hull | Unchanged | Same side of the threshold (`DataPacker/Source/ExportJobs/ExportIsland.cpp:72`) |
| Thumbnail | Unchanged | Alpha tests the same threshold; RGB reads BC (`DataPacker/Source/ExportJobs/ExportIsland.cpp:353`) |
| Masks, Color, AO | Unchanged | `MaskByHeightmap` tests only the threshold (`DataPacker/Source/ExportJobs/Texture/Texture.cpp:326`) |
| Normals | Unchanged, baked from source heights | U2; below |

**Mesh (U4).** `MeshProcessed.bin` ships as delivered and `ReadProcessedMesh` is
untouched, so the chunk's mesh XY and indices stay byte-identical; check 4 proves it.
Band selection tests mesh Z
(`DataPacker/Source/ExportJobs/Island/SubdivideBeachBand.cpp:107-112`), which is Gaea's
Mesher Z, not the heightmap, and they already disagree on `01/1x1/0`: p50 0.125 m, with
sign disagreement on 10.2% of near-shore vertices. The new waterline therefore lands on
coarser triangles: the p95 longest XY edge of triangles crossing it rose from 0.78 to 1.10
m. This matters at the waterline because `Terrain.vert` displaces only at vertices. Every
bake-off screenshot already used this mesh, so 1.10 is the look the user approved. The
mesh arena sees no growth.

**Normals (U2).** Gaea's normals are sampled unchanged
(`Engine/Data/Shaders/Terrain/Terrain.frag:65-81`). Measured on `01/1x1/0`, the technique
changes 21082 m2, of which the camera can see 7684 m2 (4449 m2 above water, 12% of its
land). There the baked normal is off by p50 5.5 and p95 22 degrees above water, from
central differences. The user chose SandbarsMost looking at exactly that. Normals depend
on the constants, so a re-encode now would go stale on the next try. The order is: tune
first, then re-encode once.

### 5. The threshold rule and the -2.5 m plateau: keep both

Technically the rule could go, since re-encoding would mask correctly. The author
recommends keeping it, because it pays for itself and removing it buys nothing visible.

- Nothing visible is gained. Water is fully opaque at compressed depth `gWaterTerrainFade`
  (`Engine/Data/Shaders/Water/Water.frag:178`, `Engine/Source/Ui/WaterWrappersBase.cpp:101`),
  which is raw -0.60 m (`Engine/Source/Ui/WaterWrappersBase.cpp:107`). The plateau is under
  1.9 m more water than that. No one can see it.
- Removing it costs a lot. Crossing the threshold changes the hull
  (`DataPacker/Source/ExportJobs/ExportIsland.cpp:72`), which changes `ConvexHullsOverlap`
  results and so every world's placements; plus thumbnail alpha, all four BC masks, and
  the sink rule (`Engine/Data/Shaders/Terrain/Terrain.vert:116-120`). It would also end the
  no-re-encode retest.

`kfHeightFloorMeters` stays derived from the shared constant, so it is not a second
threshold under `DataPacker/Source/ExportJobs/Island/AGENTS.md:63-65`.

### 6. Depth constant and Undersea Compression

Depth is raw meters, and the eye sees compressed depth
(`Engine/Data/Shaders/Terrain/TerrainElevation.frag:33-37`). With the current exponent and
`gWaterTerrainFade`:

| `kfBeachBarDepthMeters` | Compressed depth | Water alpha |
| --- | --- | --- |
| 0.08 | 2.4 cm | 8% |
| 0.15 (default) | 5.3 cm | 18% |
| 0.25 | 9.9 cm | 33% |
| 0.40 | 17.9 cm | 60% |
| 0.60 | 30 cm | 100% |

So 15 cm bars read as nearly dry sand. That is a property of the constant, not a defect.
To make bars read wetter, raise the constant; 0.25 is about a third opaque. Below about
0.05 m flat exposed area returns (22.7 m2 at 0.02 m). No compression change is needed:
the curve is global to all water. A one-line comment at the constant states that it is
raw, not compressed.

### 7. Simulation

| Consumer | Change | Evidence |
| --- | --- | --- |
| Hull, `GenerateIslandChain`, placements | None | `### 4`; `placements.crc` must match |
| Elevation grid | Changes near shore. Not serialized, not CRC'd itself, built from the shared heightmaps on both sides | `Engine/Source/Frame/FrameBase.cpp:266-273` |
| AI steering at 0.2 m | Changes. The contour lies inside the curve band, and the old flat shelf fell under the gradient test, so AI that drifted on it now steers | `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.cpp:8`, `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.cpp:42`, `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.cpp:52` |
| Terrain avoidance, 0.5 m | Changes slightly near shore | `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.cpp:86`, `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.cpp:130` |
| Disc collision | Blocking is unchanged: it blocks at ship height, the 3 m plane, above the 1 m band. A Battleship's start-overlap snap target can change, because `ResolveDiscAgainstTerrain` calls `NavQuerySnapToNavigable` after `kStartOverlap` on the tier nav graph the ship passes in | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp:635`, `Projects/BrokenEngineSandbox/Source/Frame/FlightPlaneTable.h:9`, `Engine/Source/Frame/TerrainCollisionUtils.cpp:369` |
| Terrain floor, `RaiseAboveTerrain` | Changes near shore. Fighters and Omnis rise to sampled terrain plus the 3 m plane, so their floor follows the new heights over terrain above Z 0, by up to 0.25 m (before `common::kfIslandScale`) where the curve lifts it. Battleships skip it, because plane 1 is not allowed for them | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp:148`, `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.h:42` |
| Spawn test, `IsSpawnPositionValid` | Changes near shore for Fighters and Omnis. On the 3 m plane it rejects a position whose sampled terrain exceeds plane height minus twice the radius: 0 m for Fighters and +0.8 m for Omnis, both inside the band. The Battleship's -6 m lies below the floor, where heights do not change | `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:284`, `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h:30`, `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.h:41`, `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.h:29` |
| Nav contours, tier 0 | Near none. The threshold is +1.17 m, above the band. A steep slope whose lower texel sits inside the band can move that one contour vertex by under a texel, and the 1 m simplification (`kfSimplifyEpsilonMeters`) absorbs most of it | `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.h:149`, `Engine/Source/Frame/NavBuild.cpp:303` |
| Nav contours, tiers 1 and 2 | **Change.** Thresholds are -0.53 m and -2.23 m, because of the 2.8 m and 4.5 m radii. Battleship routes reach closer to shore where the seabed dropped. Bars at -0.15 m stay obstacles, and a trough now separates them from the beach, so polygon counts can rise | `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.h:44-45`, `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.h:29` |

`ApplyTerrainPush` needs no row: it runs for Omnis only and starts at +1.17 m, above the
band (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp:745`). The
sampler in `BattleshipsRender.cpp` places client-only wrecks, outside every CRC.

`BuildNavContour` has no vertex cap (`Engine/Source/Frame/NavBuild.cpp:279`), and the
bake-off servers booted with these heights in template contours, though never placed in
a cell. Check 7 therefore compares graph sizes on placed cells.

**Replays.** Replay format is unchanged and replays are an exempt developer tool
(`Engine/Source/File/AGENTS.md:73`); they resimulate against recorded checksums
(`Engine/Source/File/AGENTS.md:70`), so a recording from older data diverges at the first
shore-affected tick, as after any terrain change. Nothing owns a data version for replays,
so no bump applies. Record new ones.

**Saves.** A save stores no placements and no heights, and regenerates each cell's list
from its coordinate. The bump rule covers a change to `GenerateIslandChain`'s output or to
the island set it picks from, and this change alters neither: path CRCs and hulls stay the
same. So `game::Frame::kiVersion` does not bump, and the trust path is untouched.

**Handshake.** Chunk content CRCs change the pack integrity token
(`Engine/Source/File/PackChunks.cpp:394-400`), so client and server must come from one
export (`Engine/Source/Network/Server/Server.cpp:640-642`). That is the existing gate doing
its job.

**Versions.** `ExportIsland::kiVersion` owns this, and only this one
(`DataPacker/Source/ExportJobs/Island/AGENTS.md:87-91`). Not `kiSplitVersion`,
`kiBakeVersion`, `kiTextureVersion`, the publication version, `DataHeader::kiVersion`, or
`game::Frame::kiVersion`.

### 8. Data cost

| Item | Files | Size (blob, packed) | This Plan | Split-stage alternative |
| --- | --- | --- | --- | --- |
| `Elevation.r32` | 70 | 190 MB, 147 MB | Unchanged | Rewritten |
| `MeshProcessed.bin` | 70 | 141 MB, 56 MB | Unchanged | Rewritten |
| Color and Masks BC7 | 140 | 216 MB, 215 MB | Unchanged | Re-encoded, identical only if the mask and encode are |
| AO BC4 | 70 | 159 MB, 159 MB | Unchanged | Same |
| Normals BC5 | 70 | 344 MB, 343 MB | Unchanged | Same |
| `BakedIsland.json` | 13 | under 1 MB | Unchanged | Rewritten |

Landing size: source only. `git sparse-checkout disable` is not a manual step, because
Local generation restores the tree itself
(`.agents/skills/compile/references/runtime-data-mode.md:167`), and nothing under it is
edited. Regeneration steps: none. After landing, the primary's Shared refresh re-exports
the 70 chunks once, because `DataPacker/**` is a data input, and its stamp restores
normally since no tracked input was rewritten.

**Bake-host caveat.** The split-stage route would make one machine's 17 GB raw cache the
only source of the landing. This Plan needs no raw data. It has a smaller caveat instead:
chunk bytes now depend on `std::sin` and `std::pow`, computed where DataPacker runs, so two
machines could differ in the last bit and fail the handshake. DataPacker builds `/fp:strict`
(`DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj:194`,
`DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj:231`), and the hosted-server Plan
already copies the pack from the primary, so client and server share one export
(`Documents/Plans/ChangeWorkflow/DeployAndRunHostedServer.md:165`). A clause in the
Reproducibility bullet at `DataPacker/Source/AGENTS.md:143` records this property.

## Reference implementation

Reviewed bake-off code, for reference. It is the exact arithmetic the user approved. The
first block is verbatim. `### Production shape` states every change from it, and the block
there is what `BeachPass.cpp` adds. Keep the arithmetic unchanged: check 3 pins it to the
reviewed Layered output.

### Verbatim reviewed code

```cpp
struct BeachLeaf
{
	int64_t iWidth = 0;
	int64_t iHeight = 0;
	float fMetersPerTexel = 0.0f;
};

// One half-float step above the threshold: a texel at it could sink, since Terrain.vert compares an inexact LINEAR sample.
constexpr float kfHeightFloorMeters = common::kfUnderwaterMaskThresholdMeters + 1.0f / 512.0f;

// fEdgeZero may exceed fEdgeOne, which reverses the ramp.
static float Smoothstep(float fEdgeZero, float fEdgeOne, float fValue)
{
	float fT = std::clamp((fValue - fEdgeZero) / (fEdgeOne - fEdgeZero), 0.0f, 1.0f);
	return fT * fT * (3.0f - 2.0f * fT);
}

// Odd remap with slope 1 + kfGain at Z=0 and identity from kfBandMeters out; it stays monotone while kfGain is below 4.63.
static void ApplyHeightCurve(std::span<float> elevation, [[maybe_unused]] const BeachLeaf& rLeaf)
{
	static constexpr float kfBandMeters = 1.0f;
	static constexpr float kfGain = 3.0f;

	for (float& rfHeight : elevation)
	{
		float fT = std::abs(rfHeight) / kfBandMeters;
		if (fT >= 1.0f)
		{
			continue;
		}

		float fRemaining = 1.0f - fT;
		float fRemainingSquared = fRemaining * fRemaining;
		rfHeight = std::copysign(kfBandMeters * (fT + kfGain * fT * fRemainingSquared * fRemainingSquared), rfHeight);
	}
}

// Exact Euclidean distance in meters from each texel to the nearest texel at or above fLandMeters (Felzenszwalb-Huttenlocher).
static std::vector<float> DistancesToLand(std::span<const float> elevation, const BeachLeaf& rLeaf, float fLandMeters)
{
	// Finite, because the parabola intersection subtracts two of these.
	static constexpr double kfNoLand = 1.0e20;

	std::vector<double> squaredDistances(elevation.size());
	for (int64_t i = 0; i < std::ssize(squaredDistances); ++i)
	{
		squaredDistances.at(i) = elevation[i] >= fLandMeters ? 0.0 : kfNoLand;
	}

	int64_t iLongestLine = std::max(rLeaf.iWidth, rLeaf.iHeight);
	std::vector<double> line(static_cast<size_t>(iLongestLine));
	std::vector<int64_t> roots(static_cast<size_t>(iLongestLine));
	std::vector<double> boundaries(static_cast<size_t>(iLongestLine) + 1);
	auto TransformLine = [&](int64_t iStart, int64_t iStride, int64_t iCount)
	{
		for (int64_t i = 0; i < iCount; ++i)
		{
			line.at(i) = squaredDistances.at(iStart + i * iStride);
		}

		auto Intersection = [&line](int64_t iRight, int64_t iLeft) -> double
		{
			double fRight = static_cast<double>(iRight);
			double fLeft = static_cast<double>(iLeft);
			return (line.at(iRight) + fRight * fRight - line.at(iLeft) - fLeft * fLeft) / (2.0 * (fRight - fLeft));
		};

		// Lower envelope of the parabolas rooted at each position; roots[k] wins from boundaries[k] to boundaries[k + 1].
		int64_t iEnvelope = 0;
		roots.at(0) = 0;
		boundaries.at(0) = -std::numeric_limits<double>::infinity();
		boundaries.at(1) = std::numeric_limits<double>::infinity();
		for (int64_t i = 1; i < iCount; ++i)
		{
			double fBoundary = Intersection(i, roots.at(iEnvelope));
			while (fBoundary <= boundaries.at(iEnvelope))
			{
				--iEnvelope;
				fBoundary = Intersection(i, roots.at(iEnvelope));
			}
			++iEnvelope;
			roots.at(iEnvelope) = i;
			boundaries.at(iEnvelope) = fBoundary;
			boundaries.at(iEnvelope + 1) = std::numeric_limits<double>::infinity();
		}

		iEnvelope = 0;
		for (int64_t i = 0; i < iCount; ++i)
		{
			while (boundaries.at(iEnvelope + 1) < static_cast<double>(i))
			{
				++iEnvelope;
			}
			int64_t iRoot = roots.at(iEnvelope);
			double fOffset = static_cast<double>(i - iRoot);
			squaredDistances.at(iStart + i * iStride) = fOffset * fOffset + line.at(iRoot);
		}
	};
	for (int64_t i = 0; i < rLeaf.iHeight; ++i)
	{
		TransformLine(i * rLeaf.iWidth, 1, rLeaf.iWidth);
	}
	for (int64_t i = 0; i < rLeaf.iWidth; ++i)
	{
		TransformLine(i, rLeaf.iWidth, rLeaf.iHeight);
	}

	std::vector<float> distances(static_cast<size_t>(std::ssize(squaredDistances)));
	for (int64_t i = 0; i < std::ssize(distances); ++i)
	{
		distances.at(i) = static_cast<float>(std::sqrt(squaredDistances.at(i))) * rLeaf.fMetersPerTexel;
	}
	return distances;
}

// Lowers the coast toward a linear foreshore that joins a Dean profile with equal slope, measured from the land line so
// the flat shelf's noise does not define the shore. It only lowers, so bays keep their deep water and steep coast barely changes.
static void ApplyDistanceProfile(std::span<float> elevation, const BeachLeaf& rLeaf)
{
	static constexpr float kfLandMeters = 0.1f;
	static constexpr float kfForeshoreSlope = 0.15f;
	static constexpr float kfForeshoreSlopeVariation = 0.4f;
	static constexpr float kfSlopePeriodXMeters = 73.0f;
	static constexpr float kfSlopePeriodYMeters = 127.0f;
	static constexpr float kfForeshoreDropMeters = 0.6f;
	static constexpr float kfDeanScale = 0.5f;
	static constexpr float kfSmoothRadiusMeters = 0.15f;
	static constexpr float kfSmoothRampMeters = 2.0f;
	static constexpr float kfBlendStartMeters = 25.0f;
	static constexpr float kfBlendEndMeters = 45.0f;
	static constexpr float kfTwoPi = 2.0f * std::numbers::pi_v<float>;

	std::vector<float> distances = DistancesToLand(elevation, rLeaf, kfLandMeters);
	for (int64_t i = 0; i < rLeaf.iHeight; ++i)
	{
		float fY = (0.5f * static_cast<float>(rLeaf.iHeight) - static_cast<float>(i) - 0.5f) * rLeaf.fMetersPerTexel;
		for (int64_t j = 0; j < rLeaf.iWidth; ++j)
		{
			float& rfHeight = elevation[i * rLeaf.iWidth + j];
			if (rfHeight < kfHeightFloorMeters)
			{
				continue;
			}

			// Land has a distance of zero, where the profile at radius zero would flatten it to kfLandMeters.
			if (rfHeight >= kfLandMeters)
			{
				continue;
			}

			float fDistance = distances.at(i * rLeaf.iWidth + j);
			if (fDistance >= kfBlendEndMeters)
			{
				continue;
			}

			float fX = (static_cast<float>(j) + 0.5f - 0.5f * static_cast<float>(rLeaf.iWidth)) * rLeaf.fMetersPerTexel;
			float fSlope = kfForeshoreSlope * (1.0f + kfForeshoreSlopeVariation * std::sin(kfTwoPi * fX / kfSlopePeriodXMeters + 1.0f) * std::sin(kfTwoPi * fY / kfSlopePeriodYMeters + 2.0f));
			float fJoinMeters = kfForeshoreDropMeters / fSlope;
			float fProfile = kfLandMeters - fSlope * fDistance;
			if (fDistance > fJoinMeters)
			{
				// With offset = xs^(1/3), the Dean curve's slope at distance xs equals fSlope, which makes the join tangent.
				float fDeanOffset = 2.0f * kfDeanScale / (3.0f * fSlope);
				float fDeanOrigin = fDeanOffset * fDeanOffset * fDeanOffset;
				fProfile = kfLandMeters - kfForeshoreDropMeters - kfDeanScale * (std::pow(fDistance - fJoinMeters + fDeanOrigin, 2.0f / 3.0f) - fDeanOffset * fDeanOffset);
			}
			fProfile = std::max(fProfile, kfHeightFloorMeters);

			// Non-land texels sit at least one texel from land, so the radius is never zero.
			float fRadius = kfSmoothRadiusMeters * std::min(fDistance / kfSmoothRampMeters, 1.0f);
			float fOverlap = std::max(fRadius - std::abs(rfHeight - fProfile), 0.0f) / fRadius;
			float fSmoothMinimum = std::min(rfHeight, fProfile) - 0.25f * fRadius * fOverlap * fOverlap;
			float fBlended = fSmoothMinimum + Smoothstep(kfBlendStartMeters, kfBlendEndMeters, fDistance) * (rfHeight - fSmoothMinimum);
			rfHeight = std::max(fBlended, kfHeightFloorMeters);
		}
	}
}

// The profile removes the shelf, and the curve then steepens what remains around Z=0.
static void ApplyLayered(std::span<float> elevation, const BeachLeaf& rLeaf)
{
	ApplyDistanceProfile(elevation, rLeaf);
	ApplyHeightCurve(elevation, rLeaf);
}

// A low-frequency field in -1..1, independent of the coast.
static std::vector<float> PatchField(const BeachLeaf& rLeaf)
{
	struct Wave
	{
		float fDirectionX = 0.0f;
		float fDirectionY = 0.0f;
		float fWavelengthMeters = 0.0f;
		float fPhase = 0.0f;
	};

	static constexpr Wave kpWaves[] =
	{
		{.fDirectionX = 0.6f, .fDirectionY = 0.8f, .fWavelengthMeters = 83.0f, .fPhase = 1.0f},
		{.fDirectionX = -0.8f, .fDirectionY = 0.6f, .fWavelengthMeters = 109.0f, .fPhase = 2.5f},
		{.fDirectionX = 0.96f, .fDirectionY = 0.28f, .fWavelengthMeters = 143.0f, .fPhase = 4.0f},
	};
	static constexpr float kfTwoPi = 2.0f * std::numbers::pi_v<float>;

	std::vector<float> field(static_cast<size_t>(rLeaf.iWidth * rLeaf.iHeight));
	for (int64_t i = 0; i < rLeaf.iHeight; ++i)
	{
		float fY = (0.5f * static_cast<float>(rLeaf.iHeight) - static_cast<float>(i) - 0.5f) * rLeaf.fMetersPerTexel;
		for (int64_t j = 0; j < rLeaf.iWidth; ++j)
		{
			float fX = (static_cast<float>(j) + 0.5f - 0.5f * static_cast<float>(rLeaf.iWidth)) * rLeaf.fMetersPerTexel;
			float fField = 0.0f;
			for (const Wave& rWave : kpWaves)
			{
				fField += std::sin(kfTwoPi * (rWave.fDirectionX * fX + rWave.fDirectionY * fY) / rWave.fWavelengthMeters + rWave.fPhase);
			}
			field.at(i * rLeaf.iWidth + j) = fField / static_cast<float>(std::ssize(kpWaves));
		}
	}
	return field;
}

// Keeps Layered's beach, then past a trough lerps toward the sunken source shelf in patches of low field, where ApplyPatchy
// keeps the shelf too. A source texel below the floor stays exact in both results and every other stays at or above the floor,
// so the lerp holds the threshold invariant.
static void ApplySandbars(std::span<float> elevation, const BeachLeaf& rLeaf, float fPatchFieldEdge, float fSubmergeMeters)
{
	static constexpr float kfLandMeters = 0.1f;
	static constexpr float kfGateStartMeters = 12.0f;
	static constexpr float kfGateEndMeters = 22.0f;
	static constexpr float kfPatchSoftness = 0.15f;

	std::vector<float> source(elevation.begin(), elevation.end());
	std::vector<float> curve = source;
	ApplyHeightCurve(curve, rLeaf);
	std::vector<float> distances = DistancesToLand(source, rLeaf, kfLandMeters);
	std::vector<float> field = PatchField(rLeaf);

	ApplyLayered(elevation, rLeaf);
	for (int64_t i = 0; i < std::ssize(source); ++i)
	{
		float fBar = std::min(source.at(i), std::max(curve.at(i) - fSubmergeMeters, kfHeightFloorMeters));
		float fWeight = Smoothstep(kfGateStartMeters, kfGateEndMeters, distances.at(i)) * Smoothstep(fPatchFieldEdge + kfPatchSoftness, fPatchFieldEdge - kfPatchSoftness, field.at(i));
		elevation[i] = std::lerp(elevation[i], fBar, fWeight);
	}
}

// Fitted to the leaf's shelf, which lies mostly at low field, so an edge at the field's midpoint already covers most of it.
constexpr float kfMostSandbarsFieldEdge = 0.05f;

// Sunk below Z=0, because bars at the waterline would rebuild the flat shelf.
constexpr float kfSandbarSubmergeMeters = 0.15f;

static void ApplySandbarsMost(std::span<float> elevation, const BeachLeaf& rLeaf)
{
	ApplySandbars(elevation, rLeaf, kfMostSandbarsFieldEdge, kfSandbarSubmergeMeters);
}
```

The rule the reviewed code keeps, which belongs in a comment on `ApplyBeachPass`: each
texel stays on its source side of `common::kfUnderwaterMaskThresholdMeters`, because terrain
below it renders sunk and its BC textures are masked to flat values, so crossing either way
breaks the coast, and the hull stays the source leaf's. The pass is single-threaded with
fixed iteration counts so output bytes are reproducible.

### Production shape

Every change from the verbatim code:

- `kfLandMeters`, `kfGateStartMeters`, `kfGateEndMeters` and `kfPatchSoftness` move to file
  scope in `BeachPass.cpp`, because the solve and the blend share them.
  `kfShelfBandMeters = 0.25f` joins them. Every other technique value stays a local
  `constexpr`.
- `ApplyHeightCurve` drops its unused `rLeaf` parameter.
- `ApplyDistanceProfile` takes `std::span<const float> distances` and stops calling
  `DistancesToLand`. One distance transform, shared by the profile and the bars, replaces
  the reviewed two, on the same input bytes, because `elevation` still equals the source
  when the reviewed profile measured it.
- `ApplyLayered`, `ApplySandbars`, `ApplySandbarsMost`, `kfMostSandbarsFieldEdge` and
  `kfSandbarSubmergeMeters` are not ported: `ApplyBeachPass` below replaces them, with the
  solved edge and `kfBeachBarDepthMeters` as the two arguments. Its blend loop is the
  reviewed loop.

```cpp
// The mean weight over the shelf rises monotonically with the edge, so a fixed bisection is deterministic.
static float SolveBarFieldEdge(std::span<const float> shelfField)
{
	static constexpr int64_t kiBisectionCount = 24;

	auto MeanWeight = [&shelfField](float fEdge) -> double
	{
		double fSum = 0.0;
		for (float fField : shelfField)
		{
			fSum += Smoothstep(fEdge + kfPatchSoftness, fEdge - kfPatchSoftness, fField);
		}
		return fSum / static_cast<double>(std::ssize(shelfField));
	};

	double fTargetWeight = static_cast<double>(kfBeachBarCoveragePercent) / 100.0;
	float fLow = -1.0f - kfPatchSoftness;
	float fHigh = 1.0f + kfPatchSoftness;
	for (int64_t i = 0; i < kiBisectionCount; ++i)
	{
		float fMiddle = 0.5f * (fLow + fHigh);
		if (MeanWeight(fMiddle) < fTargetWeight)
		{
			fLow = fMiddle;
		}
		else
		{
			fHigh = fMiddle;
		}
	}
	return 0.5f * (fLow + fHigh);
}

void ApplyBeachPass(std::span<float> elevation, int64_t iWidth, int64_t iHeight, float fMetersPerTexel)
{
	BeachLeaf leaf {.iWidth = iWidth, .iHeight = iHeight, .fMetersPerTexel = fMetersPerTexel};

	std::vector<float> source(elevation.begin(), elevation.end());
	std::vector<float> curve = source;
	ApplyHeightCurve(curve);
	std::vector<float> distances = DistancesToLand(source, leaf, kfLandMeters);
	std::vector<float> field = PatchField(leaf);

	// The profile removes the shelf, and the curve then steepens what remains around Z=0.
	ApplyDistanceProfile(elevation, leaf, distances);
	ApplyHeightCurve(elevation);

	std::vector<float> shelfField;
	for (int64_t i = 0; i < std::ssize(source); ++i)
	{
		float fHeight = source.at(i);
		if (fHeight < kfLandMeters && std::abs(fHeight) <= kfShelfBandMeters && distances.at(i) >= kfGateEndMeters)
		{
			shelfField.push_back(field.at(i));
		}
	}

	// Bars only lower source heights, so a leaf with no shallows has none to keep.
	if (shelfField.empty())
	{
		return;
	}

	float fFieldEdge = SolveBarFieldEdge(shelfField);
	for (int64_t i = 0; i < std::ssize(source); ++i)
	{
		float fBar = std::min(source.at(i), std::max(curve.at(i) - kfBeachBarDepthMeters, kfHeightFloorMeters));
		float fWeight = Smoothstep(kfGateStartMeters, kfGateEndMeters, distances.at(i)) * Smoothstep(fFieldEdge + kfPatchSoftness, fFieldEdge - kfPatchSoftness, field.at(i));
		elevation[i] = std::lerp(elevation[i], fBar, fWeight);
	}
}
```

## Critical files

- `DataPacker/Source/ExportJobs/Island/BeachPass.h`, `BeachPass.cpp`: new.
- `DataPacker/Source/ExportJobs/ExportIsland.cpp`, `ExportIsland.h`: the call, the
  fingerprint, the version.
- `DataPacker/Source/ExportJobs/Island/AGENTS.md`, `DataPacker/Source/AGENTS.md`: docs.
- `DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj` and `.filters`: membership.

## In scope

- New `DataPacker/Source/ExportJobs/Island/BeachPass.h`:
  `inline constexpr float kfBeachBarCoveragePercent = 91.6f`,
  `inline constexpr float kfBeachBarDepthMeters = 0.15f`, each with a one-line comment
  (depth's says raw meters, not compressed), and
  `void ApplyBeachPass(std::span<float> elevation, int64_t iWidth, int64_t iHeight, float fMetersPerTexel)`.
- New `DataPacker/Source/ExportJobs/Island/BeachPass.cpp`: `BeachLeaf`,
  `kfHeightFloorMeters`, `Smoothstep`, `ApplyHeightCurve`, `DistancesToLand`,
  `ApplyDistanceProfile`, `PatchField`, `SolveBarFieldEdge`, `ApplyBeachPass`, as
  `## Reference implementation` states.
- `DataPacker/Source/ExportJobs/ExportIsland.cpp`:
  - `ExportIslandData`: call `ApplyBeachPass` right after
    `rOut.iHeightmapHeight = iElevationHeight;` and before the `common::kfIslandScale`
    loop, passing `baked.fWidthMeters / static_cast<float>(iElevationWidth)` as the
    meters per texel;
  - `ExportIsland::GetInputFingerprint`: add both constants to the `fingerprint` JSON;
  - the `BeachPass.h` include.
- `DataPacker/Source/ExportJobs/ExportIsland.h`: raise the literal inside
  `ExportIsland::kiVersion`'s `Version(...)` argument by one, 34 to 35 on primary at this
  writing, keeping the `common::kiIslandScalePermille` term.
- `DataPacker/Source/ExportJobs/Island/AGENTS.md`: one paragraph in `## Island Outputs`,
  before the `Export multiplies the footprint` paragraph. It states that the chunk stage
  edits heights through `BeachPass` before the island scale, that `Elevation.r32` stays the
  pre-edit canonical input, that the pass keeps each texel on its side of the threshold,
  that the mesh and BC textures are not rebuilt against the edited heights, and that
  `BeachPass.h` owns the tuning constants. `## Versioning` already covers it and stays. No
  number is copied.
- `DataPacker/Source/AGENTS.md`: one clause in the Reproducibility bullet: island chunk
  heights, which `BeachPass` computes with `std::sin` and `std::pow`, also assume one
  authoritative host.
- `DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj` and `.filters`: add
  `BeachPass.cpp` and `BeachPass.h` beside the other `ExportJobs\Island` entries, in the
  `DataPacker\ExportJobs\Island` filter, through `/update-vcxproj`.
- Scratch, gitignored: `Temp/BeachProduction/` report script and outputs.

## Out of scope

- Every tracked file under `Engine/Data`, including all BC textures, `Elevation.r32`,
  `MeshProcessed.bin`, `BakedIsland.json`, `Island.json`, and the Gaea graph.
- `ProcessBakedRegion`, `SubdivideBeachBand`, `GaeaArchetype`, `PublishedIslandBake`,
  `ExportTexture`, and pack or manifest formats.
- `ReadProcessedMesh`, `BakeRoute.cpp`, `BakeIslandIntermediates.h`, and any vertex Z
  resample or re-subdivision of the mesh (U4).
- `kiSplitVersion`, `kiBakeVersion`, `kiTextureVersion`, the publication schema version,
  `DataHeader::kiVersion`, `game::Frame::kiVersion`, `kiProtocolVersion`,
  `engine::kiFrameBaseVersion`.
- Regenerated normals, AO, color or masks (U2); any shader change;
  `gWaterUnderseaCompression`.
- `kfBeachSubdivision*` values and any sharing of them,
  `kfUnderwaterMaskThresholdMeters`, `kfSeaBottomMeters`, `common::kfIslandScale` and its
  loop.
- Placement, navigation, collision, steering, residency, and save code.
- Parameters beyond the two constants as tuning surface; other technique values stay
  local. No config file, UI, or runtime switch.
- Bars on leaves with no shelf (U3).
- Unit tests, new ThirdParty, new harness commands, `-AllowGaeaExport`, a Gaea launch.
- The other bake-off variants: this Plan ports none of them.
- Everything under `## Planned follow-ups`.

## Planned follow-ups

Neither item below is part of this change, and neither blocks landing.

- Normals, planned (U2): once, after the user freezes the constants, one BC5 re-encode. It
  runs from the raw `Normals.exr`, on the authoring path only
  (`DataPacker/Source/ExportJobs/ExportIsland.cpp:501-507`), rewrites 344 MB of tracked
  files, and bumps `kiTextureVersion`. Method: tilt Gaea's normal by the gradient of
  (edited minus source) heights, which is zero outside the band, so no blend mask is
  needed.
- Mesh, deferred (U4): rebuild only if faceting bothers the user. Today's measure is
  the p95 waterline triangle edge, 1.10 m against 0.78 m.

Optional and unscheduled: a later step could move the edited heights into tracked files.
Nothing here depends on it.

## Risk tier and invariants

Tier trigger: Tier 3. The change alters what determinism and CRC inputs compute: terrain
heights drive CRC'd simulation through the elevation grid and AI steering
(`.agents/references/risk-tiers.md:8-11`, `.agents/references/risk-tiers.md:15`,
`Engine/Source/Frame/FrameBase.cpp:266-273`,
`Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.cpp:52`), the pack integrity token
(`Engine/Source/File/PackChunks.cpp:394-400`), and replay compatibility
(`Engine/Source/File/AGENTS.md:70`).

New interfaces: `ApplyBeachPass`, `kfBeachBarCoveragePercent`, `kfBeachBarDepthMeters`.

Invariants:

- Threshold invariant: every texel stays on its source side of
  `common::kfUnderwaterMaskThresholdMeters`, so hull, masks, thumbnail and placements are
  unchanged.
- Every leaf's outer texel ring is byte-identical to its baseline halfs.
- No tracked file under `Engine/Data` changes.
- Each chunk's mesh XY and index bytes equal the delivered `MeshProcessed.bin` export.
- An island chunk's `kiVersion` covers the pass; a changed constant re-exports chunks
  through the input fingerprint and writes no BC output.
- The pass is single-threaded with fixed iteration counts, so one host's output is
  reproducible.
- Texture channel CRCs stay unique per template
  (`Engine/Source/Frame/IslandTerrain.cpp:104-117`).

## Acceptance checks

`Temp/BeachProduction/` is scratch the executor creates. The numbers below that come from
`01/1x1/0` hold while its `Elevation.r32` has SHA-256
`3a8e01df804a41c1ec10c1813decaf45c7c69ade47141cbc52e07133efa3ddbf`. A different hash means
the delivered heights moved after this Plan was written; stop and report that.

### Chunk reading

`V` is the value of `ExportIsland::kiVersion` before this change: the `Version(...)`
argument plus `sizeof(common::ChunkHeader)`. A leaf's chunk is
`%LOCALAPPDATA%/BrokenEngine/DataPackerCache/BrokenEngineSandbox/Islands/<island>/<route>/<n>.v<V>.chunk`,
and `V + 1` after this change. The shared cache also holds older-version files. Read only
the named one, and confirm the int64 at file offset 8 equals the expected version.

Island chunks are not compressed. File offsets, confirmed on the delivered chunks:

| Offset | Field |
| --- | --- |
| 0, 8 | int64 magic, int64 version (`DataPacker/Source/ExportJobs/ExportJob.cpp:227`) |
| 352, 356 | int32 `iHeightmapWidth`, `iHeightmapHeight` |
| 372, 376, 380 | int32 `iMeshVertexCount`, `iMeshIndexCount`, `iValidAreaVertexCount` |
| 384, 388, 390 | float `fPeakHeightMeters`, uint16 `uiPeakTexelX`, `uiPeakTexelY` |
| 400 | Payload start: 16 plus `common::kiChunkDataOffset` (`Common/DataFile.h:514`), 384 |

The payload, in order (`DataPacker/Source/ExportJobs/ExportIsland.cpp:746-761`): R16 height
halfs, float2 mesh positions, uint32 mesh indices, int32 thumbnail width and height, RGBA8
thumbnail, float2 hull vertices.

Baseline halfs are those bytes of the `V` chunk, taken before the first edit. They are the
delivered heights after the island scale and `XMConvertFloatToHalfStream`
(`DataPacker/Source/ExportJobs/ExportIsland.cpp:709`). Compare chunk halfs with them byte
for byte rather than converting floats in the script, because a conversion could round
differently. Threshold side comes from those baseline halfs. The delivered `Elevation.r32`
supplies only the floats the shelf mask and bar height need. Half quantization is below the
check tolerances, and `kfHeightFloorMeters` is itself one half step above the threshold.

The scale touches samples above 0 only, so every comparison below stays valid, and the
pass's output is compared in delivered units by unscaling: `u(z)` is `z / kfIslandScale`
for `z > 0`, and `z` otherwise.

### Report script

`Temp/BeachProduction/Report.ps1` is a scratch PowerShell 7 script with an inline C# helper
through `Add-Type`. It reads each leaf found by `BakedDimensions.json` under
`Engine/Data/Islands`: `w = cropWidth / 4`, `h = cropHeight / 4`,
`metersPerTexel = widthMeters / w`, which is 0.1953125, and `s` from `Elevation.r32`.

Per leaf, in delivered units, with `z = u(half)`:

- `d`: an exact distance transform of `s >= 0.1`, single-precision meters, as
  `DistancesToLand` computes it.
- Area rows, `count * metersPerTexel^2`: `|z| <= 0.02`, `|z| <= 0.05`, `|z| <= 0.25`, and
  `|z| <= 0.02` with `d > 22`.
- `compr2cm`: the same area where `|c(z)| <= 0.02`, with `c(z) = -10 * clamp(z / -10, 0, 1)^1.25`
  for `z < 0`, otherwise `z`.
- `maxChange`: the largest `|z - s|`.
- `coverage`: over shelf texels, take `L = u(Layered half)`, `O = u(output half)`,
  `bar` from `s` with the current `kfBeachBarDepthMeters`, and `gap = bar - L`. Keep texels
  with `|gap| >= 0.05` and report `100 * mean((O - L) / gap)`. A shelf leaf with no such
  texel fails check 3, and the executor reports it.

Expected rows for `01/1x1/0`, from the reviewed floats run through the scale and half
conversion. That same conversion of `s` reproduces primary's chunk heights exactly, by
SHA-256:

| Row | `|Z|<=2cm` | `|Z|<=5cm` | `|Z|<=25cm` | `2cm,d>22` | `compr2cm` | `maxChange` | `coverage` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Baseline | 3014.0 | 3702.5 | 6867.1 | 122.6 | 3592.0 | 0.013 | n/a |
| Coverage 0 (Layered) | 58.4 | 154.2 | 1016.3 | 0.0 | 137.1 | 2.498 | n/a |
| Default | 58.4 | 154.2 | 1180.8 | 0.0 | 137.1 | 2.498 | 91.60 |

Reference figures: a same-pipeline simulation of the solved pass gave recovered coverage of
91.48 to 91.60 at the defaults and 55.36 to 55.65 at check 5's tuned values across all 20
shelf leaves, with `03/1x1/0` the lowest in both.

### Checks

1. Baseline, before the first edit: build Debug client and server with `/compile`, and save
   `Temp/BeachProduction/baseline.json` with, for all 70 leaves from the `V` chunks, the
   SHA-256 of the hull bytes, the thumbnail bytes, and the mesh XY and index bytes, plus the
   peak fields; save each leaf's height halfs under `Temp/BeachProduction/Baseline/`. On that
   build, through `/agent-harness`, run `cell_coordinate_probe` on both endpoints at
   [0,0], [1,0], [0,1], [-1,1], [5,-3], [-7,11], [40,-25], [-300,512], and save
   `placements.count`, `placements.crc` and `elevation.crc` per endpoint to
   `Temp/BeachProduction/world-before.json`. On the server, repeat each with `navigation`
   requesting every graph index below `kiNavigationGraphCount` and an empty `points`, and
   save each graph's vertex count and `polygonOffsets` count. `kNavigationGraphs` entry `i`
   describes graph index `i` and names its tier.
2. `/compile` of Release DataPacker and Debug client and server with
   `-ForbidExpensiveExport` and without `-AllowGaeaExport` succeeds in Local mode with no
   texture encode. `git status --short` lists only `## In scope` files and none under
   `Engine/Data`. The generated island header carries 70 island symbols.
3. Layered reference first: a check 2 build with `kfBeachBarCoveragePercent` set to 0 saves
   all 70 leaves' height halfs under `Temp/BeachProduction/Layered/`. For `01/1x1/0` there,
   replace every half whose sign bit is clear with `0x0000` and hash the result: the SHA-256
   is `2bcdbe1e83a93db080c57e684ac44a2724c239ec8ff7824253e9c76f5498324d`, which is the
   reviewed Layered output at and below sea level and does not depend on the island scale.
   Its report row matches the `Coverage 0` row within 1 m2. With the constant restored and
   rebuilt, `Temp/BeachProduction/Report.ps1` records each `.chunk` file's SHA-256 in
   `Temp/BeachProduction/default-sha256.json` and, comparing chunk halfs byte for byte with
   check 1's baseline halfs, reports zero violations over all 70 leaves for:
   - threshold side, taken from the baseline halfs;
   - texels below `kfHeightFloorMeters` unchanged;
   - outer ring unchanged;
   - every texel outside the band unchanged, the band being `|s| < 1`, or `s < 0.1` with
     `d < 45`, or `d > 12` with `s >= kfHeightFloorMeters`;
   - hull, thumbnail and peak equal `baseline.json`.

   Coverage is within 1 point of `kfBeachBarCoveragePercent` on each of the 20 leaves with
   shelf, and the other 50 equal their Layered reference byte for byte. Every leaf with
   shelf differs from its baseline halfs, so the unedited tree cannot pass: its chunks equal
   them.
4. The same script reports, for all 70 leaves, the mesh XY and index SHA-256 equal to
   `baseline.json`, which proves `MeshProcessed.bin` shipped as delivered.
5. Retest proof, in two builds after check 3's, each the check 2 build with the same flags:
   1. Tuned: with `kfBeachBarCoveragePercent` set to 55.6 and `kfBeachBarDepthMeters` set to
      0.4, the build exports the 70 island chunks and no BC or other type's chunk, leaves
      `git status --short` under `Engine/Data` empty, and check 3's coverage reads 55.6
      within 1 point. The DataPacker log's time for that run is recorded and is under 300 s.
   2. Default: with both constants restored, all 70 `.chunk` SHA-256 values equal
      `default-sha256.json`. For `01/1x1/0`, the report row matches the `Default` row within
      1 m2 and 1 point, and every texel of `u(output)` is within 1 cm of the reviewed
      SandbarsMost recomputed by the script as `lerp(L, bar, w)`, where `L` is
      `u(Layered half)`, `bar` uses depth 0.15, and `w` is
      `Smoothstep(12, 22, d) * Smoothstep(0.05 + 0.15, 0.05 - 0.15, field)`, with `field`
      from `PatchField`'s formula. Measured on the reviewed data, that recomputation sits at
      most 3.9 mm from the reviewed chunk and from the solved output.

   The unedited tree cannot pass: it has neither constant.
6. A second build with no edit exports no island chunk.
7. On check 5's default build, `cell_coordinate_probe` on both endpoints over the same
   coordinates equals `world-before.json` for `placements.count` and `placements.crc`; client
   and server `elevation.crc` agree on every row, and differ from the baseline on every row
   with a nonzero `placements.count`. Server `navigation` probes succeed; tier 0 graph sizes
   stay within 1% of check 1's, and tier 1 and 2 sizes are recorded against it for the user.
8. `/agent-harness` gameplay session on that build, with ships moving near a coast and a
   Battleship routing near a coast, logs no CRC mismatch; a replay recorded there passes the
   harness replay determinism check.
9. User-judged: on the Debug main menu, the `E` browser cycles the 70 islands, and the user
   reviews whole-island screenshots of all 70, plus shelf close-ups of the three 1x1 islands
   and of `01/8x4/3`, the leaf with the least shelf, under `Temp/BeachProduction/`, for bars,
   the near beach, and no black band. Screenshots support the judgment; the black-band
   mechanism is settled by check 3's threshold result. A close-up of `01/1x1/0` is at leaf
   texel (1397,912), the widest shelf, about (74.6, 20.2) m from the island center before
   the scale.

### Verification map

- CRC'd terrain inputs -> checks 7 and 8 -> placements match, client and server agree, no CRC
  mismatch near a coast; the replay check is the independent signal.
- Threshold invariant -> check 3 -> zero side violations against baseline halfs; hull,
  thumbnail and peak equality to `baseline.json` is the independent signal.
- Per-leaf coverage -> check 3 -> within 1 point on every shelf leaf; the Layered reference is
  pinned by its SHA-256.
- Retest loop -> check 5 -> tuned build exports only the 70 chunks; default build equals
  `default-sha256.json`; check 6 is the independent no-edit signal.
- Same look as the reviewed variant -> check 5 and check 9 -> within 1 cm and 1 m2; the user's
  screenshot review is the independent signal.
- Mesh ships as delivered -> check 4 -> SHA-256 equal to `baseline.json`.
- Reproducibility clause and island paragraph -> `/coherence-review` and
  `/progressive-disclosure-review` -> no findings.

## Roles

Reviewers `/plan-audit`, `/plan-simplicity-review`, `/external-grill-plan` (Tier 3);
implementer `/implement-plan`, `/update-vcxproj`, `/update-affected-code`,
`/update-claude-docs`, `/agent-harness`; builder `/compile`; reviewers `/repo-code-review`,
`/comment-review`, `/code-style-review`, `/over-engineering-review`, `/adversarial-review`,
`/coherence-review` for the vcxproj pair, `Island/AGENTS.md` and `DataPacker/Source/AGENTS.md`,
`/progressive-disclosure-review` for both `AGENTS.md` files; `/finalize-changes` on user
direction. No Gaea export and no tracked data generation.
