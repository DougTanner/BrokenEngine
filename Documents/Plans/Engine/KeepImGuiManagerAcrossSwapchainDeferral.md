<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-25T21:21:02.068Z","dependsOn":[]} -->
# Keep the ImGui manager alive across a post-Destroy swapchain deferral

## Context

`Graphics::Destroy` resets `mpImGuiManager` at every swapchain-tier destroy (Engine/Source/Graphics/Graphics.cpp:736,
inside the `meDestroyType >= DestroyType::kSwapchain` block at :706-737). `Graphics::Create` then re-queries the surface
after `Destroy` and, on a zero-area extent (window minimized during `Destroy`'s device wait), sets
`mbSwapchainRecreateDeferred` and returns (:329-342) before it re-creates the manager at :460-463. From that return until
a later `GameBase::HandleDeferredSwapchain` (Engine/Source/GameBase.cpp:871-932) completes `Create`, `gpImGuiManager` is
null while the main loop keeps running.

Main-loop code outside the render path dereferences `gpImGuiManager` without a check in that window:

- Engine/Source/Main.cpp:525, the shutdown `SaveTweaksSettings` after the loop exits: `GetTweaksScreen()`
  (Engine/Source/Ui/Screens/UiScreens.cpp:92-95) dereferences `spTweaksScreen`, which `DestroyUiScreens` reset when the
  manager's destructor ran (Engine/Source/Graphics/Managers/ImGuiManager.cpp:263). Reached when the app exits while
  minimized after a deferral.
- Engine/Source/GameBase.cpp:159 and :163, the pause toggle in `GameBase::ProcessInput`, which runs every iteration
  (Main.cpp:439) before `GameBase::Render`'s deferral skip (GameBase.cpp:946).
- Engine/Source/GameBase.cpp:260 and :277 in `GameBase::ClientUpdate` (Main.cpp:449), also before the skip; a server
  timespeed message (`Client::ServerTimespeedUpdate`, Engine/Source/Network/Client/Client.cpp:821) sets
  `mbTimeScaleChanged` with no local input.
- Engine/Source/Profile/ProfileManager.cpp:151 in `ProfileManager::ToggleProfileText`, called from
  `GameBase::ProcessInput` (GameBase.cpp:109-111).

The only checked reader is Engine/Source/Agent/AgentCommandsClientGeneric.cpp:1039. The defect predates the
RemoveModelViewerDefineUiScreens change: at baseline 5ebd21ba, Main.cpp:525 reached `gpImGuiManager->mpTweaksScreen` in
Engine/Source/Ui/TweaksSettings.cpp:38 the same way, and the baseline `Graphics::Destroy` comment already recorded that
a post-Destroy re-defer leaves the global null. A per-site null check was proposed and rejected as a bandaid; this Plan
removes the shared cause.

Root cause: the manager's lifetime ends at the swapchain-tier `Destroy`, but its replacement can only start after the
deferral point in `Create`, so the absence spans main-loop iterations. Only the manager's framebuffers
(`mImGuiFramebuffers`, built from `gpSwapchainManager->mFramebuffers` image views at ImGuiManager.cpp:371-392) must die
with the old swapchain; its framebuffer-count-sized state (the UI prepass indirect buffer, `mTextureRetirements`, and
the Vulkan backend's `ImageCount`, which imgui_impl_vulkan.cpp asserts against its render-buffer count) is rebuilt by
constructing a new manager once the new swapchain exists.

## Design

Recommended: move the swapchain-tier replacement of the manager from `Destroy` into `Create`, after the deferral, so no
main-loop code can observe the manager absent.

1. Add a public `ImGuiManager::DestroyFramebuffers()` that destroys every handle in `mImGuiFramebuffers` and clears the
   vector; the destructor's framebuffer loop (ImGuiManager.cpp:274-277) then destroys only what remains. It is
   idempotent, because a deferred retry runs `Destroy` again before the extent recovers.
2. In `Graphics::Destroy`'s swapchain-tier block (:706-737), call `gpImGuiManager->DestroyFramebuffers()` when the
   manager exists, in place of the `mpImGuiManager.reset()` at :736, so no ImGui framebuffer outlives the swapchain
   image views `mpSwapchainManager.reset()` (:735) destroys. This parallels the partial destroy of the texture and
   buffer managers at :708-719. Reset `mpImGuiManager` only at the surface tier (the `kSurface` block at :739-753),
   before `mpDeviceManager.reset()` (:751), because the manager's destructor needs the device. `Graphics::~Graphics`
   (:111-115) destroys at `kSurface`, so final teardown is unchanged.
3. In `Graphics::Create`, replace the manager at :460-463 when the swapchain was recreated in this call
   (`bSwapchainRecreated`, :353): reset the old manager, then construct the new one, since `common::Singleton` admits
   one live instance. Keep construction there, before `RecordCommandBuffers` (:465-468), which records
   `mUiPrepassIndirectVkBuffer` (CommandBufferRecordMain.cpp:349). The device is drained when the old destructor runs:
   a call that recreated the swapchain ran `Destroy` with its device wait earlier in the same `Create` call (:322), and
   no frame is submitted while a deferral is pending.

Screen lifetime stays tied to the manager. The old destructor still saves Tweaks settings through `DestroyUiScreens`,
and the new constructor still loads them through `CreateUiScreens`, as at every swapchain recreation today. The
recommendation keeps this because `TweaksScreen::LoadState` re-arms the saved-subtab selection for a new ImGui context
(TweaksScreen.cpp:377), and the slider audit re-arms per screen lifetime (`TweaksScreen/AGENTS.md`). Moving screens to
program lifetime would need another hook from the context's creation into the screens.

Alternatives considered:
- Keep the manager and its ImGui context across swapchain recreation, rebuilding only its swapchain-dependent Vulkan
  state. Not recommended: the backend fixes `ImageCount` at `ImGui_ImplVulkan_Init`, so a change in swapchain image
  count needs a backend shutdown and re-init, which marks every ImGui texture destroyed. Every engine-owned texture
  resource would then have to be re-created mid-session.
- Move the text areas and screens out of the manager to program lifetime. Not recommended: it changes 14
  `UpdateTextArea`/`GetTextArea` call sites and the Tweaks re-arm contract above, and it still leaves the manager
  absent for any future non-render reader.

Exposure: client-only (`BT_CLIENT`) Graphics recreation. No determinism or CRC, wire, serialization or `.pack` format,
`kiVersion`, replay, threading, shader, or project-membership change. Engine/Source/Graphics/Graphics.cpp and
ImGuiManager.cpp also build into ModelViewer, which shares the same `Create`/`Destroy` path.

## Critical files

- Engine/Source/Graphics/Graphics.cpp
- Engine/Source/Graphics/Managers/ImGuiManager.h
- Engine/Source/Graphics/Managers/ImGuiManager.cpp

## In scope

- `ImGuiManager`: new public `DestroyFramebuffers()` member (declaration in ImGuiManager.h, definition in
  ImGuiManager.cpp); the destructor's framebuffer loop stays correct over an emptied vector.
- `Graphics::Destroy`: the swapchain-tier block's `mpImGuiManager.reset()` (:736) replaced by the framebuffer destroy,
  and `mpImGuiManager.reset()` added to the `kSurface` block ahead of `mpDeviceManager.reset()`.
- `Graphics::Create`: the ImGui manager construction site (:460-463) gains the swapchain-recreated replacement.
- Comments those edits make false: the post-Destroy deferral comment in `Graphics::Create` (:324-328), if it describes
  manager absence.
- Engine/Source/Graphics/AGENTS.md `## Frame and Resource Lifecycle` and Engine/Source/Graphics/Managers/AGENTS.md
  `### ImGuiManager`, only where `/update-claude-docs` finds the manager's recreation tier stated.

## Out of scope

- Null checks at any `gpImGuiManager` reader, and changes at Main.cpp, GameBase.cpp, ProfileManager.cpp, UiScreens.cpp,
  TweaksSettings.cpp, or the existing check at AgentCommandsClientGeneric.cpp:1039.
- Screen or text-area lifetime changes, including keeping text-area contents such as "PAUSED" across recreation (lost
  at every recreation today).
- The surface-tier zero-extent case that reaches `CreateSwapchain` with no retry (Graphics.cpp:294-297).
- Device-loss recovery (Main.cpp:460-469) and the exception path in `Create` (:449-454).
- ThirdParty imgui sources.

## Acceptance criteria

- No path out of `Graphics::Create` or `Graphics::Destroy` that returns to the main loop leaves `gpImGuiManager` null
  while `Graphics` exists: the swapchain-tier `Destroy` and both deferral returns in `Create` (:308, :340) keep the
  manager alive. The diff settles this.
- After a swapchain-tier destroy, no ImGui framebuffer handle survives `mpSwapchainManager.reset()`.
- BrokenEngineSandbox client and ModelViewer build with no new warnings.
- A live `/agent-harness` client run that forces swapchain recreation, by toggling fullscreen through the agent
  command, keeps rendering with the UI drawn, with no validation error in the log. The supplied evidence records no
  harness reproduction of the minimized deferral itself. Reproducing it needs a minimize landing inside `Destroy`'s
  device wait, which is timing-dependent.

## Notes

- Change Workflow Tier 2, trigger: one subsystem's runtime behavior (client Graphics manager recreation) with one new
  public member. No surface that Tier 2 excludes is touched.
- No `dependsOn`: the RemoveModelViewerDefineUiScreens change that introduced UiScreens.cpp lands before this Plan
  can be claimed.
