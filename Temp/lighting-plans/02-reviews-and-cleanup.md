# 02 Reviews and cleanup

Read `Temp/lighting-plans/00-context.md` first. Requires plan 01 done (clean build).

## Objective

Run the Change Workflow Step 6 and Step 7 reviews over the whole lighting foundation change
(`git diff aa47e4a3e170a69b7dad971b94003f7ea30e5716` plus uncommitted work), resolve the findings
main accepts, and rebuild.

## Steps

1. `/over-engineering-review`: fresh `reviewer` finder pass, then fresh `implementer` validator pass
   (skip the validator if no removal candidates). If the validator changed C++, run
   `/code-style-review` over those ranges and `/compile` before step 2.
2. In parallel, fresh reviewers: `/repo-code-review` (C++), `/glsl-review` (shaders and shared
   CPU/GLSL headers `Engine/Data/Shaders/Lighting/LightingCommon.h`, `LightingTerrainCommon.h`),
   `mechanic` `/comment-review` (C++ and GLSL), `/coherence-review` (changed Markdown).
3. Then `reviewer` `/adversarial-review` (Tier 3).
4. Main decides each finding once under the user's YAGNI rule (prototype base; reject added guards
   or machinery for unobserved cases). Each accepted finding: separate `implementer` runs
   `/resolve-findings`; re-review and retest only the affected regions; a fix that fails its retest
   goes to `/external-diagnose-bug` first.
5. Step 7: `implementer` `/update-claude-docs`; fresh `reviewer` `/progressive-disclosure-review`
   (AGENTS.md and `.agents/skills/**` files changed); fresh `reviewer` `/external-skill-creator`
   findings-only validate mode (the `update-vcxproj` and `agent-harness` skill packages changed).
6. `builder` `/compile` the four targets from plan 01; static checks pass.

## Reviewer focus to pass along

- Capture synchronization (`Temp/temp-slice2-handoff.md` `## Reviewer focus`): the composing frame's
  Set 0 lighting-binding write (UPDATE_AFTER_BIND) while that framebuffer's Global work may still be
  pending; receivers and tiles recorded after `RecordEnvironment` keep preparation-before-Main;
  a failed capture leaves gameplay local diffuse at zero.
- `SaveFinishedPng`'s fence wait must run only after the UI submit for the framebuffer
  `FinishCapture` receives (`Graphics.cpp` near line 217), or it deadlocks.
- Preparation is now submitted every frame: Main waits Preparation-finished and preparation waits
  Global-finished exactly once per frame.
- `AgentCommandServer::Drain` counts every deferred drain toward the timeout again.
- Terrain XYZ buffer fully written (every vertex below `uiVertexCount`); color-attachment →
  compute/vertex barrier sufficient after Global.
- `kGpuTimerLightingTerrain` is written only while comparison mode is enabled, inside the
  preparation buffer's `kGpuTimerMain` span.
- Restored `CHECK_VK(vmaCreate*)` lines in Texture.cpp, ImGuiManager.cpp and
  TextureUploadManager.cpp equal `aa47e4a3`; Buffer.cpp still runs `CHECK_VK` after the RAM retry.

## Open items for main to decide during review

- Style routed findings (`Temp/temp-style-review-detail.md` `## Routed Findings`): `struct Lighting`
  has `m` members and a `private:` section across three .cpp files (class vs struct, rules 3/67);
  one-line accessors `SetComparison`, `GetDiagnostics`, `LightingFixture::Get{Animation,Anchor,
  Camera,Event}` (rule 49).
- `LightingFixtureAnimation` path, clip, `iMaterialCount`, `iJointsPerMaterial` are written but
  never read (slice 2 residual) — likely removable.
- `Engine/Data/Shaders/Terrain/TerrainVertex.h` now has one consumer (`Terrain.vert`), while
  `LightingTerrain.comp` repeats its world-XY/UV expressions: either have both include one shared
  function (keeps traced and drawn terrain in sync) or fold the header back into `Terrain.vert`.
  Prefer the shared function if it is a small change.
- Pre-existing, outside scope: `Projects/ModelViewer/Source/ModelViewerApplication.cpp:3-11`
  include grouping (rule 47) — leave it.

## Acceptance

Every accepted finding resolved and retested; rejected findings listed with one-line reasons in
`## Progress`; four targets build; static checks pass.

## Finish

Tick 02 in `00-context.md` `## Progress` with the decided findings summary and build envelope.
