<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T20:06:13.029Z","dependsOn":[]} -->
# Cleanup: Engine — use size_t for the vector counts whose only uses are local (style rule 17)

## Context
`Documents/C++StyleGuide.txt` rule 17 asks for `size_t` for `std::vector`
sizes and indices. The Engine scanner-rule sweep changed a count only when no
other use depended on its type. It left each of the sites below as a residual
because the count also bounds an `int64_t` loop index or is printed with
`%lld`. In each case those paired uses are local and are themselves vector
indices, sizes or print arguments, so changing the count and its paired uses
together is a complete, behavior-preserving fix:
- `Engine/Source/Graphics/Managers/BufferManager.cpp:434` in
  `BufferManager::CreateDynamicBuffer` —
  `int64_t iCommandBufferCount = gpSwapchainManager->mFramebuffers.size();`
  (an implicit conversion). Its only uses are `rBuffers.resize` (`:435`) and
  the bound of the loop at `:436`, whose index is used only in `rBuffers.at(i)`.
- `Engine/Source/Graphics/Managers/ImGuiManager.cpp:323` in
  `ImGuiManager::CreateUiPrepassIndirectBuffer` — used in the byte size
  `iFramebufferCount * sizeof(VkDrawIndirectCommand)` (`:327`, cast to
  `VkDeviceSize`) and as the bound of the loop at `:342`, whose index is used
  only to index `mpUiPrepassIndirectMapped`.
- `Engine/Source/Graphics/Managers/TextureDescriptors.cpp:79` in
  `TextureDescriptors::Create` — used in `mGlobalDescriptorSets.resize` (`:80`)
  and as the bound of the loop at `:81`, whose index is used only in
  `mGlobalDescriptorSets.at(i)` and `std::format("GlobalSet0{}", i)` (`:91-92`).
  The formatted text of a `size_t` and of an `int64_t` holding the same value
  is the same.
- `Engine/Source/Server/ServerDisplay.cpp:587` and `:589` in
  `PaintServerDisplay` — `iClientCount` and `iActiveCells` are the only
  arguments to `std::snprintf` `"Clients: %lld"` (`:617`) and
  `"Active cells: %lld"` (`:616`, through `textLine`).

## Design
The author's recommendation, which keeps every value and every output
unchanged:
1. At each site, declare the count as `size_t`, initialize it straight from
   `.size()` with no cast, and keep its name.
2. In `BufferManager.cpp:436`, `ImGuiManager.cpp:342` and
   `TextureDescriptors.cpp:81`, declare the loop index the count bounds as
   `size_t`.
3. In `ServerDisplay.cpp:616-617`, change `%lld` to `%zu` for the two changed
   arguments.
4. In `ImGuiManager.cpp:327`, keep the `static_cast<VkDeviceSize>` of the byte
   size. The product is now `size_t` and has the same value.

## Critical files
- `Engine/Source/Graphics/Managers/BufferManager.cpp`
- `Engine/Source/Graphics/Managers/ImGuiManager.cpp`
- `Engine/Source/Graphics/Managers/TextureDescriptors.cpp`
- `Engine/Source/Server/ServerDisplay.cpp`

## In scope
- `BufferManager::CreateDynamicBuffer`: `iCommandBufferCount` (`:434`) and the
  loop index at `:436`.
- `ImGuiManager::CreateUiPrepassIndirectBuffer`: `iFramebufferCount` (`:323`)
  and the loop index at `:342`.
- `TextureDescriptors::Create`: `iFramebufferCount` (`:79`) and the loop index
  at `:81`.
- `PaintServerDisplay`: `iClientCount` (`:587`), `iActiveCells` (`:589`) and
  the two format strings at `:616-617`.

## Out of scope
The other rule 17 rows the sweep recorded are left unchanged. A fix for any of
them would change a function signature, a serialized type, a data format, or
signed arithmetic. That is not a local style change, and rule 13 (`int64_t`
default, API types) governs those rows:
- Counts written to a stream as `int64_t`. Changing their type would change the
  save or replay layout: `Engine/Source/Frame/Alignments.cpp:81` and
  `Engine/Source/File/DifferenceStream.h:85-86`.
- `int32_t` navigation counts and indices that match the stored nav data and
  `PolygonRange<int32_t>`: `Engine/Source/Frame/NavBuild.cpp:443-444`,
  `Engine/Source/Frame/NavCellData.cpp` (all rows) and
  `Engine/Source/Frame/NavQuery.cpp:408`, `:649`.
- Counts or indices passed to `int64_t` parameters or members:
  `IslandChainPlacement.cpp:353`, `IslandTerrain.cpp:59`, `GameBase.cpp:488`,
  `BufferManager.cpp:93`, `:303`, `CommandBufferManager.cpp:24`, `:52`,
  `TextureDescriptors.cpp:205`, `ModelPipeline.cpp:16`,
  `PipelineDescriptorWriter.cpp:405`, `ProfileManagerBase.cpp:81`, `:624`,
  `Network/Server/Server.cpp:618`, `StaticVoices.cpp:419`.
- Signed arithmetic that relies on a signed result:
  `GameBase.cpp:456`, `StaticVoices.cpp:226` (`size() - 1`),
  `MainUniforms.cpp:176`, `:179`.
- The Vulkan `uint32_t` `commandBufferCount`: `TextureManager.cpp:311`.
- `Engine/Source/Ui/CurveData.h:100`, `:162`. Changing them means changing the
  class's `int` interface (`GetPointCount`, `GetPoint(int)`, `Secant(int)`)
  and its callers.
- `Common/`, `DataPacker/`, `Projects/` and `Tools/` (sibling sweep Plans).

## Risk tier and invariants
Tier 1 (mechanical): trigger is local behavior-preserving style work with no
public signature or invariant exposure (`.agents/references/risk-tiers.md`).
Every changed variable is client graphics setup or server display text, so
there is no simulation, CRC, serialization, wire or `.pack` exposure. The
swapchain framebuffer count is small and non-negative, so every value is the
same in either type.

## Acceptance criteria
- `/compile` Client and Server Debug and Release builds pass with no new
  warnings.

## Notes
Originating residual: the Engine scanner-rule style sweep (rule 17 rows for
`Engine/Source/Graphics` and `Engine/Source/Server`).
