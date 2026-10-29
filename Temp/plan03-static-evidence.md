# Plan 03 static acceptance evidence

Reviewed read-only in session worktree `C:\Users\dougt\.claude\worktrees\BrokenEngine\0cb8b7b1-9695-4b87-ab78-8bf06eceacad`, branch `lighting-foundation-work`, HEAD `ee53a2c5986a363b94adc82789c634e1dbe312e3`. Fixed comparison baseline `b56f20fa44d0424c2fdf5003157563684ed747f5`; no moving-main comparison. `git status --porcelain` was empty before this evidence file. No rebuild or runtime operations performed.

## Builds

Row 1 PASS from retained successful build envelopes and current local artifacts.

- `Temp/AgentBuildEnvelopes/compile-20261029T150823266Z-41536.md` ## Build envelopes: DataPacker Release x64 starts 2026-10-29T15:08:36.003Z, elapsed 40385 ms, status success, exitCode 0, msbuild exitCode 0, diagnostics empty, complete retained log.
- `Temp/AgentBuildEnvelopes/compile-20261029T154650516Z-22496.md` ## Build envelopes: Sandbox client Debug x64 starts 15:47:09.484Z, elapsed 56102 ms; Sandbox server Debug x64 starts 15:48:16.222Z, elapsed 10976 ms; ModelViewer Debug x64 starts 15:48:36.320Z, elapsed 22091 ms. All success/exitCode 0, diagnostics empty, complete retained logs. All three explicitly pass `/p:DataBuildMode=Local /p:RunDataPacker=true` and use this worktree's Sandbox Output/Data and generated includes.
- Local shader generation is corroborated by `Temp/AgentBuildLogs/brokenenginesandbox-20261029T154709485Z-28012.log:10` (Running DataPacker), :17 (worktree Output/Data), :18 onwards (shader dependency changes and actual shader tool commands). Current `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Output/Data/Shader.pack` is 672560 bytes, last write 15:47:19.5123013Z; Shader.manifest is 2080 bytes, 15:47:19.5113018Z.
- Retained executable output records: `Temp/AgentBuildLogs/datapacker-20261029T150836004Z-32644.log:50`, `Temp/AgentBuildLogs/brokenenginesandbox-20261029T154709485Z-28012.log:1354`, `Temp/AgentBuildLogs/brokenenginesandboxserver-20261029T154816223Z-32688.log:20`, `Temp/AgentBuildLogs/modelviewer-20261029T154836321Z-29908.log:114`.

Current files verified with Get-Item (all paths relative to this worktree):

| Executable | Bytes | LastWriteTimeUtc on 2026-10-29 |
| --- | ---: | --- |
| DataPacker/Platforms/VisualStudio2026/Output/DataPacker.exe | 4545024 | 15:09:16.2249045Z |
| Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Output/BrokenEngineSandbox.Debug.exe | 21308416 | 15:48:05.440022Z |
| Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Output/BrokenEngineSandboxServer.Debug.exe | 17541120 | 15:48:27.1055252Z |
| Projects/ModelViewer/Platforms/VisualStudio2026/Output/ModelViewer.Debug.exe | 13638144 | 15:48:58.3322124Z |

Source freshness: tracked Common/DataPacker C++/project inputs have newest write 14:40:52.0435831Z
(Main.cpp and InputFingerprint.cpp); newest retained changed C++/shader/project source across
Common/DataPacker/Engine/Projects is 15:41:01.2040778Z (GameBase.cpp and LightingComparison.cpp),
before latest runtime builds. The later 16:00:05Z commit records those already-built edits; clean
status and file times support current retained source provenance. These envelopes are build output
records, not content-addressed source receipts.

## Simulation

Row 9 static portion PASS. Runtime absence of desync/CRC failures remains the separate harness worker's check.

`git diff b56f20fa44d0424c2fdf5003157563684ed747f5 -- Engine/Source/Frame Projects/BrokenEngineSandbox/Source/Frame` contains exactly 12 files: four AGENTS.md files; AreaLightsRender.cpp, PointLightsRender.cpp, HexShieldsRender.cpp; Collection.h; BattleshipsRender.cpp, FightersRender.cpp, MissilesRender.cpp, OmnisRender.cpp. Thus it includes merged changes against the fixed pre-change baseline, rather than only the latest commit.

- Engine render files are guarded BT_CLIENT at line 3. AreaLights removes projected diffuse GPU quads while retaining visible sprites (GraphicsResources :15, Render :45, EndRender :104); PointLights removes diffuse buffers and retains visible sprites (:15, :45, :131); HexShields removes lighting GPU layout fields/pipeline and retains ordinary shields (:15, :45, :103). No collection columns or simulation hooks change.
- `Engine/Source/Frame/Collections/Collection.h:119` declaration and :143/:153 call sites rename RegisterLightingTextureCrc to RegisterCollectionTextureCrc under BT_CLIENT. Traced implementation `Engine/Source/Graphics/Managers/TextureManager.cpp:744` requests texture chunk load and inserts into renderer texture-registration set. It changes no shared row, identity counter, RNG, serialization tuple, or CRC selection.
- All four game render translation units are guarded BT_CLIENT at line 1. Battleships/Fighters
  changes allocate and fill trace-only GPU model slots and skinning reservations, reading
  current/post-render row data; writes target ModelLayout (:163 / :165), render-only static
  counters, local workbuffer indices and GPU skinning output. EndRender publishes visible and
  trace-only model ranges (:222 / :224). Missiles :116 writes GPU ModelLayout slots; :134 publishes
  ranges. Omnis :132/:157 excludes completed dying rows from model publication; :228 publishes the
  remaining range. These predicates affect rendering, not lifecycle state.
- `Engine/Source/Graphics/Lighting/Lighting.cpp:228` AddModels appends range descriptions only to the acquired LightingFramebuffer's modelRanges; :246 EndRender feeds the lighting model preparation. No frame reference is accepted or written.
- `Engine/Source/GameBase.cpp:817` allocates/copies a separate mRenderInterpolates entry and :818 interpolates that render copy; :822 stamps its render basis. Its only fixed-baseline diff is inside client Render (:942 comparison settings, :948 comparison camera). Game frame render fan-out is `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:567` / :571, distinct from tick phases.
- `git diff --quiet <fixed-baseline> -- Engine/Source/Frame/FrameBase.cpp Engine/Source/Frame/FrameBase.h Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp Projects/BrokenEngineSandbox/Source/Frame/Frame.h Projects/BrokenEngineSandbox/Source/Frame/FrameCollections.h`
  returned 0. FrameBase.cpp :235 RunFrameTick and :316 CRC stamping are unchanged. CRC walkers :12
  and :72 use ServerCollections. Collection.h :298 SharedCollectionCrc still selects
  SharedCrcMembers/SharedMembers with the server parity assertion; its body is unchanged. No frame
  structs, collection layout declarations, shared-state tuples, simulation hooks, or CRC inputs
  changed in the scoped frame diff.

No findings in the assigned static scope; no source edits made.
