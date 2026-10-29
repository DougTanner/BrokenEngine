## Fixes Applied

All paths are repository-relative under the session worktree; line numbers are post-edit.

- Engine/Source/Graphics/Lighting/Lighting.cpp:19, :46 — Rule 2 — nested `.geometry = {.instances =` / `{.triangles =` opened on the declaration line; each nested multi-line initializer now opens its brace on the next line (LightingSceneGeometry, LightingSolidGeometry); no value change.
- Engine/Source/Graphics/Lighting/LightingModels.cpp:286 — Rule 2 — same nested `.geometry = {.triangles =` reflow in State::GetBuildGeometry.
- Engine/Source/Graphics/Lighting/Terrain.cpp:108 — Rule 2 — same nested `.geometry = {.triangles =` reflow in State::GetGeometry.
- Engine/Source/Graphics/Lighting/Lighting.cpp:77 — Rule 3 — Vulkan local `geometry` -> `vkGeometry` (CreateFixtureResources, two uses).
- Engine/Source/Graphics/Lighting/Lighting.cpp:286 — Rule 3 — `geometry` -> `vkGeometry` (EndRender, two uses).
- Engine/Source/Graphics/Lighting/Lighting.cpp:356-358 — Rule 3 — `geometry` -> `vkGeometry`, `range` -> `vkRange` (RecordPreparation fixture loop).
- Engine/Source/Graphics/Lighting/Lighting.cpp:126 — Rule 3 — reference to a Vk type `rAcceleration` -> `rVkAccelerationInstance` (WriteFixtureSolids, all uses).
- Engine/Source/Graphics/Lighting/Terrain.cpp:257 — Rule 3 — `rAccelerationInstance` -> `rVkAccelerationInstance` (Write, all uses).
- Engine/Source/Graphics/Lighting/Terrain.cpp:132 — Rule 3 — Vk pointer `pDraws` -> `pVkDraws` (Prepare, all uses).
- Engine/Source/Graphics/Lighting/Terrain.cpp:140 — Rule 3 — `rLimits` -> `rVkLimits` (Prepare, two uses).
- Engine/Source/Graphics/Lighting/Terrain.cpp:276 — Rule 3 — `barrier` -> `vkBarrier` (RecordPreparation, two uses).
- Engine/Source/Graphics/Lighting/Terrain.cpp:294 — Rule 3 — `geometry` -> `vkGeometry`, `range` -> `vkRange` (RecordPreparation build loop).
- Engine/Source/Graphics/Managers/InstanceManager.cpp:489 — Rule 3 — lambda parameter `rExtension` -> `rVkExtension` (two uses).
- Engine/Source/Graphics/Lighting/LightingState.h:52 — Rule 3 — `static constexpr VkExtent3D kExtent` -> `kVkExtent` (Comparison; kiTileColumns/kiTileCount initializers in LightingState.h, five uses in LightingComparison.cpp, three in LightingCapture.cpp).
- Engine/Source/Graphics/Lighting/LightingComparison.cpp:107 — Rule 3 — reference `requested` -> `rRequested` (ApplyComparisonSettings, all uses).
- Engine/Source/Graphics/Lighting/LightingComparison.cpp:186 — Rule 56 — `rFrame` (abbreviates Framebuffer) -> `rFramebuffer` (PrepareComparisonPipelines, all uses).
- Engine/Source/Graphics/Lighting/LightingCapture.cpp:38 — Rule 8 — counter `iTile` -> `i` (RecordCapture tile loop).
- Engine/Source/Graphics/Lighting/Terrain.cpp:282 — Rule 8 — loop variable `iBase` -> `i` (RecordPreparation Dispatch lambda, all uses).
- Engine/Source/Graphics/Managers/TextureDescriptors.cpp:32 — Rule 39 — unnamed lambda parameter `int64_t` -> `[[maybe_unused]] int64_t iPosition` (the house name in the sibling Texture data lambdas).
- Engine/Source/Graphics/Render/MainUniforms.cpp:242-244 — Rule 18 — top-level `const` removed from the three `static siv::BasicPerlinNoise<float>` locals (moved block in PopulateMainCamera).
- Engine/Source/Graphics/Lighting/Lighting.cpp:269-278 — Rule 62 — one disjunction of three unrelated limit checks guarding one throw split into three `if`s, original order kept.
- Engine/Source/Graphics/Lighting/LightingModels.cpp:226-237 — Rule 62 — one disjunction of three primitive-range checks guarding one throw split into three `if`s, original order kept.
- Engine/Source/Graphics/Lighting/Terrain.cpp:159-166 — Rules 51 and 62 — the split three-operand disjunction (trailing `||`, misaligned continuation) became two `if`s: the two parallel `std::in_range` checks stay together, the device-limit check stands alone.
- Engine/Source/Graphics/Managers/DeviceManager.cpp:38 — Rule 53 — three consecutive `deviceExtensions.push_back` of fixed values -> one initializer-list `insert` at `end()`.
- Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesRender.cpp:102 — Rule 51 — split Boolean continuation had 16 alignment spaces before `||`; now 13 so the operand starts under the first operand, as the rule states.

Rule 47 script (`Invoke-IncludeOrderCheck.ps1 -Fix`, 94 files): status pass, 0 violations. It rewrote Projects/ModelViewer/Source/ModelViewerApplication.cpp, whose include block is pre-existing and untouched by the session; that rewrite was reverted with Edit so only the session's own lines differ from the baseline. No Fixes Applied row for Rule 47.

## Routed Findings

- Engine/Source/Graphics/Lighting/Lighting.h:68 (Lighting.cpp, LightingCapture.cpp,
  LightingComparison.cpp) — `struct Lighting` carries `m`-prefixed members (`mpState`,
  `mRequestedComparisonSettings`) and a `private:` section, which Rule 3 reserves for classes, and
  splits its member definitions across three .cpp files, which Rule 67 forbids for a `class`. Either
  it is a `class` (Rule 67 violated) or the members drop `m` and the access labels go (Rule 3 / Rule
  49). Needs a class-versus-struct decision and a repo-wide rename of `mpState` and
  `mRequestedComparisonSettings` (TweaksScreenLighting.cpp:18 uses the latter) — routes to the
  caller; not auto-fixed because it changes class access and layout.
- Engine/Source/Graphics/Lighting/Lighting.cpp:413 (`Lighting::SetComparison`) — Rule 49: the body is one assignment to the public `mRequestedComparisonSettings`; the caller (TweaksScreenLighting.cpp:68) could assign it directly. Public API change — `/repo-code-review` Rule 49 route.
- Engine/Source/Graphics/Lighting/Lighting.cpp:428 (`Lighting::GetDiagnostics`) — Rule 49: returns `mpState->diagnostics` only; `State` is a private incomplete type, so removal needs the diagnostics moved into a public member — `/repo-code-review` Rule 49 route.
- Engine/Source/Graphics/Lighting/LightingFixture.cpp:112, :143, :148, :153 (`GetAnimation`, `GetAnchor`, `GetCamera`, `GetEvent`) — Rule 49: each returns one private member or one indexed element (`mAnimation`, `mAnchor`, `mCameras.at`, `mEvents.at`); the `private:` section protects no multi-member invariant. Public API change — `/repo-code-review` Rule 49 route.

Rejected scanner rows (not violations): style-rule-54 x72 (locals and struct members, not class members), style-rule-44 x10 (shader-layout destinations that are not aligned types), style-rule-37 x2 (`std::get<N>` on a tuple parameter, not a multi-return), style-rule-62 x2 (LightingFixture.cpp:80, :87: the same `contains` check over two parallel maps), log x6 and debug-break x1 (kept diagnostics; Objects/AGENTS.md records the `DEBUG_BREAK` before the RAM retry).

## Functions/regions touched

- Lighting.cpp: LightingSceneGeometry, LightingSolidGeometry, CreateFixtureResources, WriteFixtureSolids, EndRender, RecordPreparation.
- LightingModels.cpp: LightingModels::State::GetAsset (primitive range guards), LightingModels::State::GetBuildGeometry.
- Terrain.cpp: LightingTerrain::State::GetGeometry, LightingTerrain::Prepare, LightingTerrain::Write, LightingTerrain::RecordPreparation.
- LightingComparison.cpp: Lighting::ApplyComparisonSettings, Lighting::PrepareComparisonPipelines, Lighting::CreateComparisonResources, Lighting::RecordReceivers (kVkExtent).
- LightingCapture.cpp: Lighting::RecordCapture, Lighting::FinishCapture (kVkExtent).
- LightingState.h: Lighting::State::Comparison.
- InstanceManager.cpp: InstanceManager::ValidatePhysicalDeviceCapabilities (extension lambda).
- DeviceManager.cpp: DeviceManager::DeviceManager (device extension list).
- TextureDescriptors.cpp: TextureDescriptors::Create (fallback texture lambda).
- MainUniforms.cpp: PopulateMainCamera (static noise tables).
- MissilesRender.cpp: MissilesInterpolate::Render (visibility expression).
