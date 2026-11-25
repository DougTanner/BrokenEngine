# Compile-Time Invariant Enforcement

## Overview

Broken Engine already enforces many layout, serialization, and identity rules
at compile time (286 `static_assert`s, 101 `[[nodiscard]]` annotations,
`common::Flags<T>`, `engine::Id<T>`, `engine::GlobalId`, `engine::Guid128<T>`,
`engine::GridCoord`, `engine::RenderBasis`, `engine::SimulationTime`,
single-sourced CPU/GLSL headers in `ShaderLayoutsBase.h`, and 57 layout
assertions in `Common/DataFile.h`).

This investigation catalogs the remaining high-value invariants that still live
in `AGENTS.md` prose or runtime `ASSERT`s and evaluates which ones should move
into the C++23 compiler, grouped by ROI and scope.

## Tier A — High-ROI, Zero-ABI-Cost Wins (Ready to Split into Plans)

### 1. Single-source `Frame*::Values()` for `Crcs()` and `LogDifferences()`

- **Current prose rule**: `Engine/Source/Frame/AGENTS.md` notes that
  `Crcs()` and `LogDifferences()` list frame-wide values other than
  `kbClientOnlyValue` ones by hand and must change together with `Values()`.
- **Evidence**:
  - `Engine/Source/Frame/FrameBase.h` already defines `FrameValuesWrite` and
    `FrameValuesRead<SHARED>` over `FrameColumnList<FRAME_COLUMN(...)>` and
    filters client-only fields via `kbClientOnlyValue<COLUMN::kpMember>`.
  - `Engine/Source/Frame/FrameBase.cpp` (`FrameInterpolateBase::Crcs`,
    `LogDifferences`, `FramePostRenderBase::Crcs`, `LogDifferences`) and
    `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp` (`FrameInterpolate`,
    `FramePostRender`) duplicate the same member lists manually.
- **Compiler mechanism**: Add `FrameValuesCrc` and `FrameValuesLogDifferences`
  helpers beside `FrameValuesWrite`/`FrameValuesRead` in `FrameBase.h`,
  folding each non-`kbClientOnlyValue` column in `Values()` order (dispatching
  to `.Crc()` when present, else `common::Crc`, and reusing `COLUMN::kName` for
  `common::LogDifference`). Exact CRC fold order is preserved bit-for-bit.
- **Effect**: Adding a shared frame-wide value to `Values()` automatically
  enrolls it in serialization, CRC, and desync difference logging; forgetting
  `Crcs()` or `LogDifferences()` becomes impossible.

### 2. Compile-time `Collections()` dependency ordering

- **Current prose rule**: `Engine/Source/Frame/AGENTS.md` and
  `Engine/Source/Frame/FrameBase.h` state that `Explosions` must precede
  `SmokeTrails` in `Collections()` and explicitly note that existing
  `static_assert`s do not verify order. `Projects/BrokenEngineSandbox/Source/`
  `Frame/FrameCollections.h` states that ship collections follow `ShipType`
  order and `Battleships` precedes client-only `Wrecks`.
- **Compiler mechanism**: Reuse the existing `consteval CollectionTupleIndex<T>`
  helper in `Engine/Source/Frame/Collections/CollectionLifecycle.h` to add
  ordering `static_assert`s directly in `FrameBase.h` and `FrameCollections.h`.
- **Effect**: Reordering dependent collections fails compilation immediately
  instead of introducing subtle one-frame lag or `ShipType` index mismatches.

### 3. Catch signature-drifted optional collection hooks

- **Current prose rule**: `Engine/Source/Frame/Collections/AGENTS.md` warns
  that optional hooks in `Engine/Source/Frame/FrameUtils.h` (`Render`,
  `BeginRender`, `RequestTextures`, `EndRender`, `PreCollision`,
  `PostCollision`, `AreaDamage`, `Transfer`, `Destroy`, `Spawn`,
  `AllocateAndCopy`, `PostRead`) skip silently if absent or uncallable.
- **Risk**: Refactoring a phase signature (for example adding a
  `const CellStaticData&` parameter) or mistyping a parameter type on a new
  collection silently drops that collection from the phase with no compiler
  error.
- **Compiler mechanism**: For every non-overloaded optional hook (all of the
  above except `Spawn`, which is overloaded with `Spawn(Frame&, SpawnInfo)`),
  check whether the member name exists (`requires { &TS::PreCollision; }`) and
  `static_assert` that it matches the expected call expression when present.
  For `Spawn`, either keep the existing check or rename the phase hook to a
  distinct non-overloaded name (`SpawnPhase`) if desired.
- **Effect**: Signature drift or bad parameter types on opt-in hooks become
  immediate compile errors.

## Tier B — Scoped Strong-Type Refactors (Needs Priority Decision)

### 4. Strong types for swappable `int64_t` indices and raw collision masks

Several hot-path APIs accept adjacent raw `int64_t` parameters of different
domains or raw `uint16_t` bitmasks:

- **Network subscriptions** (`Engine/Source/Network/Client/`
  `ClientSubscriptions.h`): `IsStaleRetainedEpoch`, `ClassifyFullState`,
  `ClassifyCoordinateUpdate`, and `ClassifySubscribeAccept` all take adjacent
  `(int64_t iSlotIndex, int64_t iEpoch)` arguments. Wrapping `CoordSlotIndex`
  and `CoordSlotEpoch` (or `uint8_t`/`uint32_t` tagged structs) prevents
  silent transposition across client and server subscription code.
- **Collision layers and categories** (`Engine/Source/Frame/Collision.h`,
  `Engine/Source/Frame/AreaDamage.h`, `Projects/BrokenEngineSandbox/Source/`
  `Frame/HealthDamage.h`):
  - `Collision::GetCollisions(int64_t iLayerIndex, int64_t iIndex)` and
    `Collision::GetOutcome(int64_t iLayerIndex, int64_t iIndex)` take adjacent
    raw `int64_t` parameters; a tagged `CollisionLayerIndex` returned by
    `Collision::AddLayer` prevents swapping layer index and row index.
  - `CollisionLayer::uiCategory`, `CollisionLayer::uiCollidesWith`, and
    `AreaDamage::Get(..., uint16_t uiCategoryMask, ...)` use raw `uint16_t`
    masks while the rest of the engine uses `common::Flags<EnumType>`.
- **Trade-off**: Zero runtime overhead, moderate call-site churn in one
  subsystem per type. Best split into one small Plan per subsystem if desired.

### 5. Compile-time `SharedMembers()` / `SharedCrcMembers()` subset checks

- **Current state**: `Engine/Source/Frame/Collections/Collection.h` checks
  `IsMemberTupleSubset(SharedCrcMembers(), SharedMembers())` and
  `IsMemberTupleSubset(SharedMembers(), Members())` via runtime `ASSERT`s
  comparing member addresses (`CollectionMemory.h`), because `Members()`
  returns `std::tie(...)`. The trap of adding a `#if defined(BT_CLIENT)` column
  without declaring `SharedMembers()` is audited externally by
  `.agents/scripts/Invoke-CollectionLayoutCheck.ps1`.
- **Option 1**: Keep `std::tie(...)` and rely on `Invoke-CollectionLayoutCheck`
  plus runtime `ASSERT`s (no refactor churn).
- **Option 2**: Migrate collection member accessors to pointer-to-member lists
  (`FrameColumnList`-style), enabling `static_assert` subset, uniqueness, and
  client/server parity checks at compile time.
- **Trade-off**: Option 2 touches every engine and game collection header and
  `CollectionMemory.h`; defer unless collection layout churn increases.

## Explicitly Excluded (Low ROI or Conflicts with Engine Design)

- **Strong `XMVECTOR` wrappers for cell-local vs camera-space positions or
  W-lane invariants**: `RenderBasis::Rebase` and `ValidateVector<IS_POSITION>`
  operate on raw `XMVECTOR` / `FXMVECTOR`. On Windows x64, wrapping `XMVECTOR`
  in a struct risks breaking XMM register passing unless carefully structured
  as an HVA with `__vectorcall`, and forces unwrapping at every DirectXMath
  intrinsic. Keep raw `XMVECTOR` + `RenderBasis` + boundary `ASSERT`s.
- **Eliminating `gp*` manager globals for explicit DI parameters**:
  `.agents/references/cpp-conventions.md` and `Engine/Source/AGENTS.md`
  explicitly mandate reaching game-instantiated `*Base` objects and singleton
  managers through `gp*` globals rather than passing references/pointers. Do
  not fight this established repository contract.

## Recommended Next Steps

1. Promote **Tier A** (items 1, 2, and 3) into one or two small, decision-
   complete `Documents/Plans/Engine/` Plans (`FrameValuesSingleSourceCrc` and
   `CollectionOrderingAndHookChecks`).
2. Decide whether **Tier B item 4** (`CoordSlotIndex`/`CoordSlotEpoch` and
   `CollisionLayerIndex`) is worth its call-site churn before authoring Plans.
3. Leave **Tier B item 5** and the excluded items alone for now.
