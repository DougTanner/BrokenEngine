# Refactor Candidates

Design shapes in the code that a refactor would improve, kept for a later
session to review in batch. At the Change Workflow Verify and land step, main
copies these rows here word for word, dropping only session-local IDs — a row's
leading `/over-engineering-review` `<ID>`, and a `makes unnecessary:` or
`candidate:` tail that names only candidate IDs:

- `/over-engineering-review` `Design suggestions` rows the user did not accept.
- `size observation` and `refactor observation` rows from the
  `/repo-code-review` and `/glsl-review` `Residuals`, skipping a size
  observation whose last field is `no qualifying reduction`.
- At most one row from main's own context, on the `refactor observation` form in
  `.agents/skills/repo-code-review/SKILL.md` `## Handoff`.

Prefix each row with `<YYYY-MM-DD> <session branch> — `. Add new rows directly
under `## Entries`, so the newest rows are at the top and main reads only the
top of the file. A session reviewing the log removes each row it handles.

## Entries

- 2026-11-20 claude/bdfc78d3-acd8-4d0c-9af9-3f1bc3399d83 — refactor
  observation — Engine/Source/Agent/Commands/ReplayHarnessRigs.cpp:243 — the
  writer-input pause arms a count of writer inputs while
  `GameBase::miPauseAtTick` (Engine/Source/GameBase.cpp:505) arms a target
  tick, two pause-at-target mechanisms in the server tick loop — key the
  writer-input pause on a target tick so both share one check, after a harness
  run confirms the replay matrix still passes when a target tick has only
  terminal writers and for a pending recording start
- 2026-11-19 claude/dc240152-a8d5-4109-98cc-1668cbc3fa7c —
  Engine/Source/Profile/ProfileManager.h:402 — fill the ring slots with a
  sequence value no real submission can have (submission sequences start at 1),
  both at default construction and in Clear(), so that an empty slot never
  matches sequence 0 — makes unnecessary:
  Engine/Source/Profile/GpuStallTimeline.cpp:44, :60, and the
  iNewestSequence != 0 term at :81
- 2026-11-19 claude/2c24c0a5-8dd2-4c11-9afc-991d104208bc — size observation —
  Engine/Data/Shaders/ShaderFunctions.h is 6,181 bt-token-v1, over ~5,000
- 2026-11-19 claude/2c24c0a5-8dd2-4c11-9afc-991d104208bc — size observation —
  Engine/Data/Shaders/ShaderGlobalLayout.h is 5,396 bt-token-v1, over ~5,000
- 2026-11-19 claude/21ac1d45-ae10-43b8-9886-f9e8114750bf —
  Engine/Source/Frame/TimeStep.cpp:82 — have TimeStep::SetTimeScale return
  without setting mbTimeScaleChanged when the multiply and divide already
  match, so no caller has to check the ratio first — makes unnecessary:
  Engine/Source/Network/Client/Client.cpp:832 (and the pre-existing check at
  Engine/Source/Network/Server/ServerSessionRuntime.cpp:51)
- 2026-11-19 claude/21ac1d45-ae10-43b8-9886-f9e8114750bf —
  Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipQueries.cpp:177
  — have the grid-save read reject a non-dying ship row (Fighters, Battleships,
  Omnis) whose registry id is 0, so every row ForEachEnemyShip visits holds a
  nonzero id and a zero target can never match — makes unnecessary:
  FightersCombat.cpp:104, ShipQueries.cpp:63, ShipQueries.cpp:177 (the
  `uuid.iValue != 0 &&` prefix of the kept-target test)
- 2026-11-19 claude/583d6a45-ee2d-4df1-b610-487ad6f08df3 —
  Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersRender.cpp:241
  — the `kbDebugRender` tails of the Fighters, Omnis, and Battleships `Render`
  are near-copies of one hitbox-plus-navigation row loop differing only in
  ShipType, radius, and Omni combat aim — fold them into one ship-layer debug
  draw that takes the radius from `kShipHitboxTypes` by ship type
- 2026-11-19 claude/eb09a76a-7dd6-4c45-b102-0c8002fba1a8 —
  Engine/Source/Graphics/Managers/DynamicPipelines.h:52 — make the two span
  overloads take paired per-pipeline data whose sizes match by type (a span of
  {crc, count}/{crc, present} pairs, or a template over two std::array<_, N> of
  the same N), so a size mismatch cannot compile — makes unnecessary:
  DynamicPipelines.cpp:82, DynamicPipelines.cpp:97
- 2026-11-19 claude/eb09a76a-7dd6-4c45-b102-0c8002fba1a8 —
  Engine/Source/Frame/CellStaticData.cpp:157 — make
  IslandTerrain::BuildElevationGrid (Engine/Source/Frame/IslandTerrain.cpp:406)
  leave the grid empty when there are no placements; MakeCellElevationSampler
  already treats an empty grid as sea floor, so its callers would not each need
  their own empty-islands check — makes unnecessary:
  Engine/Source/Frame/CellStaticData.cpp:157,
  Engine/Source/Network/Client/ClientSessionRuntime.cpp:228, the islands half
  of Engine/Source/Frame/FrameBase.cpp:272
- 2026-11-15 claude/8e00ea59-d178-42f1-831f-3608122b6db6 —
  Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:115 — move kTransfer (and
  kDestroy) out of the persisted pLifecycleFlags column into a column that is
  never saved, since CompactCollections clears them within the tick, and
  persist only kDying. A saved stream could then never hold the Transfer flag.
  This changes the Collection layout and save format and needs every
  collection's kiVersion bumped; the plan's ## Out of scope excludes it — makes
  unnecessary: ValidateNoTransferRows (Frame.cpp:115-131) and its call at
  Frame.cpp:806
- 2026-11-15 claude/42225028-bbfa-4078-98f5-84becc296ea6 —
  Engine/Source/Graphics/GraphicsUtils.cpp:44 — now that surface loss throws
  instead of raising `meDestroyType` to `kSurface`, nothing sets a tier above
  `kSwapchain` while a frame runs, so every
  `std::max(DestroyType::kSwapchain, meDestroyType)` (also Graphics.cpp:395,
  :573, :586 and SwapchainManager.cpp:608, :643) is a plain assignment —
  assign `DestroyType::kSwapchain` directly, or decide whether the tier
  ordering is still needed above `kSwapchain`
- 2026-11-15 claude/d58c37a3-698d-45c4-8a10-a85cb03c0154 —
  Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp:161
  — `MissilesInterpolate::ClientInitialize` calls `SynchronizeMissile`
  through a hand declaration in `MissilesUpdate.cpp`, although every render
  copy runs `MissilesInterpolate::Update`, which syncs the area light again
  before drawing (the reason Blasters dropped its `ClientInitialize` light
  sync) — check whether the trail and sound parts need the first-frame call;
  if not, drop it and make `SynchronizeMissile` file-static
- 2026-11-15 claude/a2c7596c-10a1-435c-b437-17d460d78832 —
  Engine/Source/Graphics/Objects/Pipeline.h:156 — `mExternalVkDescriptorSetLayout`
  is always assigned the global Set 0 layout in `Pipeline::Create` and read
  only in `PipelineCreator::CreateMultiSetPipelineLayout`, so it is a copy of
  `mTextureDescriptors.mGlobalVkDescriptorSetLayout` — read the global layout
  directly and delete the member
- 2026-11-15 claude/d58c37a3-698d-45c4-8a10-a85cb03c0154 —
  Engine/Data/Shaders/Shadow/Shadow.comp:38 — the terrain shadow march's end
  index is published per direction by the CPU (iShadowElevationSize, written
  in GlobalUniforms.cpp PopulateShadowSunExtension), so each direction's stop
  can be set wrong independently — have the shader stop when the unsigned
  index leaves the elevation width and drop the layout field (needs a shader
  repack)
- 2026-11-15 claude/d58c37a3-698d-45c4-8a10-a85cb03c0154 —
  Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp:24 — the
  `class RenderTargetTextures` defines members in a second .cpp, breaking
  C++StyleGuide rule 67 — move the lighting-texture functions into a stateless
  static-function struct, as WorldLightingShadowPipelines now is, or merge them
  into RenderTargetTextures.cpp if it stays under the /reduce-file threshold
- 2026-11-15 claude/041e27c6-7b5f-4879-840f-1df103133526 —
  Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp:103 —
  the game builds its active-work span from ReconcileDispatcher's public mWorks
  plus the dispatch result's iActiveCount — have ReconcileDispatchResult carry
  the span over the active works and make mWorks private
- 2026-11-15 claude/4028f52d-a875-45f8-b548-c1851fe1804e — Size observation —
  ShaderGlobalLayout.h is 5276 bt-token-v1 tokens, above the ~5,000 threshold;
  splitting it is follow-up work outside this change
- 2026-11-14 claude/179efbf3-916e-4701-9a2c-e96ffd751f75 —
  Engine/Source/Graphics/Managers/DeviceManager.cpp:400 — drop the
  std::filesystem::exists pre-check and its filesystem_error catch and let the
  OpenFile is_open test be the only absent-or-unopenable rejection; the return
  value is identical and the only difference is that a first launch with no
  cache logs the kDebug "Discarded unreadable pipeline cache" line; needs a
  user decision because the Plan fixes exists in its present form — makes
  unnecessary: Engine/Source/Graphics/Managers/DeviceManager.cpp:400-411
- 2026-11-14 claude/179efbf3-916e-4701-9a2c-e96ffd751f75 —
  Engine/Source/Graphics/Managers/DeviceManager.cpp:434 — let the read's
  gcount/stream test at :443 answer a failed seek to the beginning: a failed
  seekg sets failbit, the read then extracts nothing, and the same log text and
  empty return follow; needs a user decision because the Plan keeps one return
  per existing check — makes unnecessary:
  Engine/Source/Graphics/Managers/DeviceManager.cpp:434-439
- 2026-11-14 claude/79a8b3b8-5c7c-4714-9b08-ff5184e4c1b7 —
  Engine/Source/Network/Client/Client.cpp:126 — treat a short or undecodable
  delayed connection response as the fatal corrupt-server-data case that
  Engine/Source/Network/AGENTS.md `## Corrupt Input Policy` already defines (the
  client trusts its server), by dropping the size test and letting
  NetworkMessages::Read throw, or by routing the peek through the same catch
  Client::Receive uses; this reverses Fixed decision (1), so it needs the user's
  approval — makes unnecessary: Client.cpp:126-129 size test and the rationale
  comment at Client.cpp:125
- 2026-11-14 claude/d2a0bc7b-1e0b-4fb0-b1fc-8d003a2e91f6 —
  Engine/Source/Agent/Commands/AudioStreamingHarnessRig.h:33 — carry
  `AudioChunkReadState` (Engine/Source/File/PackChunks.h:11) directly in the
  rig's snapshot, pool-entry, and record types and in `Record`/`RecordEntry`,
  deleting the mirrored `AudioStreamingHarnessRigQueueState`; the rig header
  already includes `File/FileManager.h`, and an opaque
  `enum class AudioChunkReadState : uint8_t;` declaration suffices if
  `PackChunks.h` must stay out of it — makes unnecessary: the four
  `static_assert`s and `ToHarnessRigState` at
  Engine/Source/Agent/Commands/AudioStreamingHarnessRig.cpp:58-65, and the
  value cast at Engine/Source/File/PackChunks.cpp:700
- 2026-11-14 claude/d2a0bc7b-1e0b-4fb0-b1fc-8d003a2e91f6 — refactor
  observation: Engine/Source/Agent/Commands/AgentCommandsAudioStreaming.cpp:267
  — every synchronous action branch ends with the same snapshot assignment and
  return — let those branches fall through to one tail assignment, with only
  the two deferred branches returning early.
- 2026-11-14 claude/5939deae-ae49-4fa3-8c68-a2981f6a14ef —
  Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h:85
  — drop NextRouteWaypoint's `rbFinal` out parameter and the snap-and-compare at
  ShipBehaviours.cpp:542 that fills it, since no caller reads the result
  (Battleships.cpp:496, Omnis.cpp:658 and :682, ShipBehaviours.cpp:564 and :839,
  FleetCoordination.cpp:119) — makes unnecessary:
  Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp:495
- 2026-11-14 claude/d2a0bc7b-1e0b-4fb0-b1fc-8d003a2e91f6 —
  Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:165 —
  in ApplyOwnedEntityEvent, find the entity once and either write its coordinate
  through the found iterator or add it with the event coordinate, which
  AddClientOwnedEntity already stores; the separate UpdateOwnedEntityCoordinate
  function and its second search then go away
- 2026-11-14 claude/79a8b3b8-5c7c-4714-9b08-ff5184e4c1b7 —
  Engine/Source/CrashReport.cpp:89 — test only the pending buffer after the two
  file-name appends: both paths share one prefix and the pending name is the
  report name plus 8 characters (CrashReport.cpp:110, :113), so a pending path
  that fits proves the report path fits; this changes the plan's stated "if
  either buffer ended empty it empties both" form, so it needs the user's
  approval — makes unnecessary: the `spcUserReportPath[0] == L'\0'` operand at
  CrashReport.cpp:89 and the pending-buffer store at CrashReport.cpp:92
- 2026-11-14 claude/17346847-3ec8-430a-8b7a-bdce55b2d1f2 —
  Engine/Source/Frame/Collections/Puffs/Puffs.h:47 — require an area and an
  intensity scale wrapper for every used keyframe of a `PuffControllerType`
  (all four registrations already supply them), so Update and `AddControlled`
  multiply unconditionally — makes unnecessary:
  Engine/Source/Frame/Collections/Puffs/PuffsUpdate.cpp:28, :32, :70, :71
