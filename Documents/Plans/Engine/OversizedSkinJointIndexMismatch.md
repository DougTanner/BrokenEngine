<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-29T15:55:52.197Z","dependsOn":["Documents/Plans/Engine/WorkbufferGrowthHandleSafety.md"]} -->
# Make skeleton joint and node counts fully dynamic

## Context

Three constants cap how big a model's skeleton may be, and none of them has a
recorded reason:

- `common::kiMaxJointsPerMesh` = 128 (`Common/DataFile.h:230`) bounds the
  per-material joint-matrix write loop
  (`Engine/Source/Graphics/AnimationData.cpp:441`), clamps the per-mesh joint
  count the shader reads (`Engine/Source/Graphics/AnimationData.cpp:410`), and
  drives the exporter's clamp and warning
  (`DataPacker/Source/ExportJobs/ExportScene.cpp:642-647`).
- `common::Skeleton::kiMaxSkinJoints` = 256 (`Common/DataFile.h:190`) bounds the
  exporter's skin joint count
  (`DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader.cpp:78`) and the
  pack-load validation (`Engine/Source/Graphics/AnimationData.cpp:61`).
- `common::Skeleton::kiMaxNodes` = 256 (`Common/DataFile.h:189`) bounds the node
  count at export (`DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader.cpp:70`)
  and at load (`Engine/Source/Graphics/AnimationData.cpp:60`), and sizes the two
  animation scratch reservations
  (`Engine/Source/Graphics/AnimationData.cpp:215-216,334-335`).

The first two disagree, which is the originally reported defect: a skin may be
accepted with up to 256 joints, but only 128 joint matrices are ever written,
while the joint-matrix region is allocated from the skeleton's full joint count
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersRender.cpp:282`;
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsRender.cpp:147`).
Slots 128..N-1 therefore exist and are indexed by the shader
(`Engine/Data/Shaders/Model/ModelSkinned.vert:22-25`) but never written, holding
identity matrices while the initial fill survives
(`Engine/Source/Graphics/Managers/BufferManager.cpp:392-416`) and leftover values
afterwards (`Engine/Source/Graphics/Managers/BufferManager.cpp:557-577`). The
exporter's warning claims "Skinning will use first 128 joints only"
(`DataPacker/Source/ExportJobs/ExportScene.cpp:644`), which is not what the
runtime guarantees.

Nothing downstream requires any of the three limits. The shader declares no joint
capacity — `jointMatrices[]` is an unsized `scalar` storage-buffer array
(`Engine/Data/Shaders/Model/ModelCommon.h:74-77`). Both render paths already
allocate from the model's real skin joint count
(`PlayersRender.cpp:282`, `SpaceshipsRender.cpp:147`), the per-material offset
stride matches it (`Engine/Source/Graphics/AnimationData.cpp:226`), and
`BufferManager::AllocateJointMatrices`
(`Engine/Source/Graphics/Managers/BufferManager.cpp:498-507`) grows without a
ceiling. Every pre-computed animation array is already runtime-sized to the
header counts (`Engine/Source/Graphics/AnimationData.cpp:186`), and each chunk
section is bounded by the chunk's true byte extent through the `BoundAdvance`
helper (`Engine/Source/Graphics/AnimationData.cpp:78-97`).

Pre-existing at session baseline `45d74910b4c9c2f0b80f06dc3c44c8c746f2ea36`. Not
reachable with current assets: the repository has four `.gltf` files, no `.glb`,
and none declares `"skins"`.

## Design

The user directed the outcome: skeleton sizing is fully dynamic with no
artificial limit, and a file declaring more bones than the runtime can support
trips an `ASSERT`. All three constants are deleted, every buffer is sized from
the counts the chunk declares, and a deleted constant's validation duty is
replaced only where real data or a real structural relationship supplies the
bound. These are recorded as decided, not recommended.

**The per-mesh cap.** `kiMaxJointsPerMesh` and its four uses go: the definition,
the exporter's over-limit test and warning, the exporter clamp (which collapses
to a plain `static_cast<uint16_t>`), the runtime clamp at
`AnimationData.cpp:410` (which collapses to a widening cast —
`common::MeshData::uiJointCount` is `uint32_t` at `Common/DataFile.h:217`, so
nothing narrows), and the `&& i < common::kiMaxJointsPerMesh` term on the write
loop. Every matrix the now-unbounded loop writes lands inside the region already
reserved for that material, so the slots the cap left unwritten are the slots the
loop now fills — the reported defect removed at its root.

**The stored per-material joint count.** `MaterialInfo::uiJointCount` is
`uint8_t` (`Common/DataFile.h:204`), which would wrap to 0 above 255 and silently
disable skinning. User decision: the count is a `uint16_t`. `uiJointCount`
becomes `uint16_t` and `uiPad[3]` becomes `uiPad[2]`, so `sizeof(MaterialInfo)`
stays 72 and the assertion at `Common/DataFile.h:208` is untouched. Two
`BT_OFFSETOF` layout locks are added beside it in the form already used in this
header (`Common/DataFile.h:122,131,246`): `uiJointCount` at offset 4 and
`f4x4RelativeTransform` at offset 8. Because the size is unchanged, the `sizeof`
folds in `ExportScene::kiVersion` (`DataPacker/Source/ExportJobs/ExportScene.h:23`)
and `ExportModel::kiVersion` (`DataPacker/Source/ExportJobs/ExportModel.h:14`) do
not fire, so all three raw version terms must be bumped explicitly, as the
surrounding comments and `Common/AGENTS.md` `## Shared Data Contracts` require.
The local `uint8_t uiSkinJointCount` in `BuildMaterialInfos`
(`DataPacker/Source/ExportJobs/ExportScene.cpp:638`) widens with the field, or
the widened field is re-narrowed at its assignment.

**The skin-joint limit.** `kiMaxSkinJoints` is deleted; `sizeof(Skeleton)` stays
4 and its assertion (`Common/DataFile.h:196`) is untouched, since it is a
`static constexpr` member. The bound becomes the model's own node count at both
ends (user decision): the pack loader tests
`mHeader.skeleton.uiSkinJointCount > mHeader.skeleton.uiNodeCount` and the
exporter's `ASSERT` becomes
`ASSERT(rSkin.joints.size() <= skeletonData.skeleton.uiNodeCount);`. A glTF
skin's joints are node indices, so a valid skin can never declare more joints
than the skeleton has nodes, and every entry is already checked against
`uiNodeCount` one loop later (`Engine/Source/Graphics/AnimationData.cpp:119-125`).
The bound stays dynamic and still closes the allocation amplification the render
paths would otherwise inherit. Accepted cost: a glTF listing the same node twice
in one skin, which tinygltf does not reject, now fails export instead of
producing a chunk the engine refuses to load.

**The node limit.** `kiMaxNodes` is deleted and the pack-load node-count term is
removed with no replacement. What bounds the node count afterwards is already
present: the `uint16_t` width of `Skeleton::uiNodeCount` (`Common/DataFile.h:192`),
the bytes actually present in the chunk through `BoundAdvance`
(`Engine/Source/Graphics/AnimationData.cpp:78-97`), and the exporter's `ASSERT`
on the `size_t`→`uint16_t` narrowing, which becomes
`ASSERT(rModel.nodes.size() <= std::numeric_limits<uint16_t>::max());` —
`std::numeric_limits` is the established form here
(`Engine/Source/Frame/FrameRegistry.cpp:140`). The two scratch reservations keep
their current shape and positions and are each sized from
`mHeader.skeleton.uiNodeCount` in place: the world-matrix reservation in
`EvaluateAnimation` (`Engine/Source/Graphics/AnimationData.cpp:215-216`) and the
TRS reservation in `EvaluateWorldMatrices`
(`Engine/Source/Graphics/AnimationData.cpp:334-335`). No new capacity constant,
no derived node limit, and no pack-load node ceiling.

**Growth is the designed signal.** A skeleton large enough to exceed a thread's
workbuffer makes `Workbuffer::Grow` (`Common/Workbuffer.cpp:78-95`) fire: it
`DEBUG_BREAK()`s, logs `kError` while a frame is open, resizes to twice the need,
and continues, and the per-thread buffer stays grown for every later call on that
thread. That is the intended, accepted behavior here — one break and one
in-place growth per thread per oversized model. It is safe because
`Documents/Plans/Engine/WorkbufferGrowthHandleSafety.md`, this Plan's dependency,
moves the workbuffer onto backing storage that grows in place without changing
its address, names this exact nested pair in its `## Context`, and rewrites the
`Common/AGENTS.md` `## Allocation-Free Scratch` growth sentence accordingly. That
is why this Plan leaves both reservations and `EvaluateWorldMatrices`' signature
alone.

**The corrupt-chunk `ASSERT`.** User addition: no mesh is expected to saturate
the limit, but a corrupt file must still be caught, so an `ASSERT` stays. It
goes in the existing per-material validation loop in
`AnimationData::Load` (`Engine/Source/Graphics/AnimationData.cpp:126-132`):
`ASSERT(mpMaterialInfos[i].uiJointCount <= mHeader.skeleton.uiSkinJointCount);`.
`ASSERT` (`Common/ErrorUtils.h:20`) logs, breaks, and throws in every
configuration, so it rejects a corrupt chunk in Release too — which is also why
the DataPacker needs no rejection of its own: the skeleton loader's `ASSERT`
already runs over the same canonicalized `skins[0]` earlier in the same
`PreExport` call, and a failed job discards the staged manifest and pack
(`DataPacker/Source/Main.cpp:538-554`).

**Regeneration.** The three version bumps dirty every cached scene and model
chunk, so `PreExport` runs again and the four tracked `.MODEL` intermediates are
rewritten. Their bytes come out identical: no repository `.gltf` declares a skin,
so every `MaterialInfo::uiJointCount` is 0 and the old `{0}{0,0,0}` and new
`{0,0}{0,0}` patterns are the same four zero bytes. Local packed data must be
regenerated through an authorized Local generation build before the client will
load, per `Common/AGENTS.md` `## Shared Data Contracts`.

**Verification is inspection-only** (user decision): no skinned test fixture, no
new tracked asset, and no harness check that observes joints 128..N-1 animating.

## Critical files

- `Common/DataFile.h:189,190,204-205,208,230,430` — the three constants, the
  retyped field pair and its layout assertions, and `DataHeader::kiVersion`.
- `Engine/Source/Graphics/AnimationData.cpp:60-61,126-132,215-216,334-335,409-410,441` —
  pack-load validation, the added `ASSERT`, the two scratch reservations, the
  collapsed clamp, and the write-loop bound.
- `DataPacker/Source/ExportJobs/ExportScene.cpp:638-648` — `BuildMaterialInfos`:
  the local count type, the over-limit warning, and the clamp.
- `DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader.cpp:69-78` — the two
  narrowing `ASSERT`s that must be rebound, not deleted.
- `DataPacker/Source/ExportJobs/ExportScene.h:17`,
  `DataPacker/Source/ExportJobs/ExportModel.h:14` — the two raw version terms
  whose `sizeof` folds cannot fire for a size-preserving retype.

## In scope

- `Common/DataFile.h:189` — delete `Skeleton::kiMaxNodes`.
- `Common/DataFile.h:190` — delete `Skeleton::kiMaxSkinJoints`.
  `sizeof(Skeleton)` and its assertion at `:196` are unaffected by either
  deletion.
- `Common/DataFile.h:204-205` — retype `uiJointCount` to `uint16_t`, shrink
  `uiPad` to `[2]`, and restate the trailing comment if it reads as a byte field.
- `Common/DataFile.h:208` — add `BT_OFFSETOF` assertions for `uiJointCount` at
  offset 4 and `f4x4RelativeTransform` at offset 8, in the message form used at
  `Common/DataFile.h:122,131,246`.
- `Common/DataFile.h:230` — delete `kiMaxJointsPerMesh`;
  `kiInitialJointMatrixCapacity` on the next line stays.
- `Common/DataFile.h:430` — bump `DataHeader::kiVersion`'s raw `52` term.
- `DataPacker/Source/ExportJobs/ExportScene.h:17` — bump `ExportScene::kiVersion`'s
  raw `62` term.
- `DataPacker/Source/ExportJobs/ExportModel.h:14` — bump `ExportModel::kiVersion`'s
  raw `2` term.
- `DataPacker/Source/ExportJobs/ExportScene.cpp:638` — retype the local
  `uint8_t uiSkinJointCount` to `uint16_t`.
- `DataPacker/Source/ExportJobs/ExportScene.cpp:642-647` — delete the over-limit
  test and its `kWarning` LOG; collapse the `std::min` to
  `static_cast<uint16_t>(uiJointCount)` and restate the comment against the
  skeleton loader's `ASSERT` instead of the cap.
- `DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader.cpp:70` — rebind the
  node `ASSERT` to `ASSERT(rModel.nodes.size() <= std::numeric_limits<uint16_t>::max());`.
- `DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader.cpp:78` — rebind the
  skin `ASSERT` to `ASSERT(rSkin.joints.size() <= skeletonData.skeleton.uiNodeCount);`.
  Neither `ASSERT` may be deleted: `:69` and `:77` each narrow a `size_t` to
  `uint16_t` with a plain `static_cast`, so the field type enforces nothing.
- `Engine/Source/Graphics/AnimationData.cpp:60-61` — delete the node-count term
  with no replacement and replace the skin-joint term with
  `mHeader.skeleton.uiSkinJointCount > mHeader.skeleton.uiNodeCount`; update the
  comment block at `:56-59`, which names structural maxima for both counts, to
  name what bounds the node count instead (field width, chunk byte extent through
  `BoundAdvance`, exporter `ASSERT`).
- `Engine/Source/Graphics/AnimationData.cpp:126-132` — inside the existing
  per-material validation loop, add
  `ASSERT(mpMaterialInfos[i].uiJointCount <= mHeader.skeleton.uiSkinJointCount);`
  with a short comment naming corrupt-chunk detection.
- `Engine/Source/Graphics/AnimationData.cpp:215-216` — delete the local
  `kiMaxNodes` alias and size the world-matrix reservation from
  `mHeader.skeleton.uiNodeCount`; the reservation stays where it is.
- `Engine/Source/Graphics/AnimationData.cpp:334-335` — delete the `kiMaxNodes`
  alias and derive `kiVecSize`/`kiTotalSize` from `mHeader.skeleton.uiNodeCount`
  as runtime values; the reservation stays where it is.
- `Engine/Source/Graphics/AnimationData.cpp:409-410` — collapse the `std::min` to
  a plain widening cast of `rMaterialInfo.uiJointCount` and restate the comment,
  which names the cap.
- `Engine/Source/Graphics/AnimationData.cpp:441` — drop the
  `&& i < common::kiMaxJointsPerMesh` term from the write-loop bound.
- `Common/AGENTS.md` `## Shared Data Contracts` — rewrite the closing sentence
  requiring a `.pack` field with a fixed structural maximum to be bounded against
  a matching `DataFile.h` `kiMax*` constant: after this change the node and
  skin-joint counts have no such constant, so the rule becomes that an untrusted
  count is bounded by the count/capacity helpers against the bytes present plus
  the data-derived checks its own boundary applies.
- The four tracked `.MODEL` intermediates —
  `Engine/Data/Models/DualGeodesicIcosahedron/DualGeodesicIcosahedron.gltf.MODEL`,
  `Projects/BrokenEngineSandbox/Data/Models/Spaceship/scene.gltf.MODEL`,
  `Projects/BrokenEngineSandbox/Data/Models/aim-9_missile/scene.gltf.MODEL`,
  `Projects/BrokenEngineSandbox/Data/Models/spaceship2/scene.gltf.MODEL` —
  rewritten by the DataPacker run the version bumps force, expected
  byte-identical.

## Out of scope

- Merging, moving, or reordering the two scratch reservations, and changing
  `AnimationData::EvaluateWorldMatrices`' signature. Handle safety under growth
  belongs to `Documents/Plans/Engine/WorkbufferGrowthHandleSafety.md`, this
  Plan's dependency.
- Any workbuffer sizing or naming change: `Common/Threading/Multithreading.cpp:15`
  keeps its bare `65536` and the main thread stays 10 MiB
  (`Engine/Source/Main.cpp:100`). No new capacity constant, no runtime capacity
  query, and no pack-load node-count ceiling.
- Remapping, pruning, or renumbering joint indices, and any vertex-attribute
  rewrite.
- Bounding the per-vertex `JOINTS_0` indices against the skin joint count at
  export (`DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:124,157-165`).
  That leftover is real and pre-existing, and is owned by the follow-up Plan
  `Documents/Plans/Engine/SceneVertexJointIndexBounds.md`; it is not a dependency
  of this Plan in either direction.
- A skinned test asset, a game load path for one, and any harness verification.
- The skeleton serialization beyond the `MaterialInfo` retype, the render-path
  joint-matrix allocation, `kiInitialJointMatrixCapacity`, the joint-matrix
  buffer growth policy, and `ModelSkinned.vert`.
- Multi-skin identity selection and mixed-skinning material splitting.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the change spans independently owned
subsystems (Common, Engine, DataPacker) and edits the shared data header
`Common/DataFile.h`, which `.agents/references/risk-tiers.md` names as Tier 3.
Two further independent triggers: it is a serialized data-layout change with
three raw version bumps and local data regeneration, and it alters trust-boundary
bounds at `Engine/Source/Graphics/AnimationData.cpp:60-61`, which that reference
excludes from Tier 2.

Preserve these invariants:

- For every skinned material the runtime writes exactly
  `mHeader.skeleton.uiSkinJointCount` joint matrices at `uiJointMatrixOffset`,
  and the render paths reserve exactly that many per material, so every index
  within `uiSkinJointCount` is written each frame.
- `MaterialInfo::uiJointCount` never wraps for a skin the exporter accepts,
  because the exporter's checked bound is representable in that field, and
  `sizeof(MaterialInfo)` stays 72.
- Trust boundary: every animation-chunk section stays bounded by the chunk's true
  byte extent through `BoundAdvance`, which together with the `uint16_t` width is
  what bounds `uiNodeCount`; `uiSkinJointCount` is validated against
  `uiNodeCount`, every `mpSkinJointToNode` entry against `uiNodeCount`, and every
  `MaterialInfo::uiJointCount` against `uiSkinJointCount`, so no runtime-sized
  destination can be overrun by a corrupt chunk.
- Scratch: both reservations are sized from `mHeader.skeleton.uiNodeCount`; a
  `Workbuffer::Grow` is accepted and signalled by `DEBUG_BREAK()` plus the
  `kError` log, and does not invalidate a live handle because the dependency
  Plan has already landed.
- Format: `DataHeader::kiVersion`, `ExportScene::kiVersion`, and
  `ExportModel::kiVersion` all advance, so no stale manifest, cached chunk, or
  `.PreExport` marker survives.

## Acceptance criteria

- DataPacker, client, and server build `Release|x64` through `/compile` with no
  new warning; the `sizeof(MaterialInfo) == 72` assertion and both new
  `BT_OFFSETOF` assertions hold at compile time.
- A search of tracked source, shaders, scripts, and project files returns zero
  matches for `kiMaxJointsPerMesh`, `kiMaxSkinJoints`, and `kiMaxNodes`, and zero
  matches for the first-128-joints warning wording.
- The write loop's condition is `i < mHeader.skeleton.uiSkinJointCount` alone,
  and `pMeshData->uiJointCount` is a plain widening cast with no `std::min`.
- Pack-load validation carries no node-count ceiling, bounds `uiSkinJointCount`
  by `uiNodeCount`, and the per-material loop carries the new `ASSERT`; both
  scratch reservations are sized from `mHeader.skeleton.uiNodeCount` and neither
  is moved.
- The exporter's joint-count site has no LOG and no `std::min`, only a
  `static_cast<uint16_t>` into a `uint16_t` local; the node `ASSERT` tests
  `std::numeric_limits<uint16_t>::max()` and the skin `ASSERT` tests
  `skeletonData.skeleton.uiNodeCount`.
- A DataPacker run over the current assets completes with no new warning or
  failure and re-exports every scene and model chunk rather than taking the
  cached skip path.
- `git status` reports all four tracked `.MODEL` files unchanged after that run;
  a reported diff is a failure to investigate, not a pass.
- After local packed data regeneration through an authorized Local generation
  build, the client loads the regenerated pack with no `AnimationData::Load`
  failure and no `ASSERT` from the new per-material check.

## Notes

Origin: the original export-rejection design was replaced at the user's direction
during its `/next-plan` run.
