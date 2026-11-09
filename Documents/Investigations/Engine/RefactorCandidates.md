# Refactor Candidates

Design shapes in the code that a refactor would improve, kept for a later session to review in batch. At the Change Workflow Verify and land step, main copies these rows here word for word, dropping only session-local IDs — a row's leading `/over-engineering-review` `<ID>`, and a `makes unnecessary:` or `candidate:` tail that names only candidate IDs:

- `/over-engineering-review` `Design suggestions` rows the user did not accept.
- `size observation` and `refactor observation` rows from the `/repo-code-review` and `/glsl-review` `Residuals`, skipping a size observation whose last field is `no qualifying reduction`.
- At most one row from main's own context, on the `refactor observation` form in `.agents/skills/repo-code-review/SKILL.md` `## Handoff`.

Prefix each row with `<YYYY-MM-DD> <session branch> — `. Add new rows directly under `## Entries`, so the newest rows are at the top and main reads only the top of the file. A session reviewing the log removes each row it handles.

## Entries

- 2026-11-09 claude/fd1e46f2-b608-4a00-82ea-ede366954800 — size observation — Engine/Data/Shaders/ShaderGlobalLayout.h measures 5281 bt-token-v1 (over ~5000); it was large before this change, and splitting it is follow-up work outside this review
- 2026-11-08 claude/996e9f9a-6fe0-4dea-bd36-5b66496381a0 — Engine/Source/Ui/WaterWrappersBase.cpp:111 —
  give gWaterColorNoiseFrequency, gWaterColorNoiseMultiplierOne and gWaterColorNoiseMultiplierTwo a minimum above
  zero (the multipliers' existing 0.1 step works as the floor). Every frequency passed to PopulatePatchLattice would
  then be positive.
  — makes unnecessary: Engine/Source/Graphics/Render/GlobalUniforms.cpp:542 (the `fFrequency > 0.0` branch and its
  "0 for zero frequency" case in the Render.h:200 contract)
- 2026-11-08 claude/996e9f9a-6fe0-4dea-bd36-5b66496381a0 — size observation: Engine/Data/Shaders/ShaderFunctions.h is 6161 bt-token-v1, over ~5,000; splitting it into a *Common.h is follow-up work
- 2026-11-08 claude/996e9f9a-6fe0-4dea-bd36-5b66496381a0 — size observation: Engine/Data/Shaders/ShaderGlobalLayout.h is 5216 bt-token-v1, over the ~5000 threshold; splitting it is follow-up work.
- 2026-11-07 claude/35f31600-f877-4044-86d4-1d7c6ccb9721 — refactor observation: Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp:466 — the AI direction is named both `Ai…` (`pVecAiDirections`, `AimAiDirection`, `ShipFlags::kNoAiDirection`) and `ArtificialIntelligence…` (`rVecArtificialIntelligenceDirection`, `ComputeArtificialIntelligenceSteering`); now that `Documents/C++StyleGuide.txt` rule 56 approves `Ai`, rename the long forms so one concept has one name
- 2026-11-07 claude/7a8d4a30-d920-433a-abad-a08b724a3378 — refactor observation Projects/ModelViewer/Source/ModelViewerApplication.cpp:65 — `SelectAdjacentModel` only forwards `miSelected` plus a step to `SelectModel`, a pass-through style guide rule 49 forbids; `miSelected` may stay private because `SelectModel` changes it together with playback and metadata state, so let `SelectModel` take the step (or Main.cpp compute the index through a public read) and drop the wrapper.
- 2026-11-06 claude/2ea16005-f59e-4086-a845-0c0236be3a46 — size observation: `Engine/Data/Shaders/Model/Model.frag` is 5,177 `bt-token-v1`, over the ~5,000 report line; splitting it via `ShaderFunctions.h` or `*Common.h` is follow-up work.
- 2026-11-06 claude/bce2711e-800d-4a71-886c-78431dc6cb86 — DataPacker/Source/ExportJobs/ExportJob.cpp:139 —
  Compute every job's content key in one place, inside `ExportJob::CheckDirty` under the existing fingerprint catch,
  through a virtual key-input function, instead of in the Scene and Shader constructors. A failed read of a shader
  source, a shader header, or a Blender source or script then fails only that asset, where it kills the run today.
  `ExportScene::CheckDirty` must then derive the key before it reads the Blender marker, and `ExportShader` must build
  its dependency sidecar path after the key exists.
  — makes unnecessary: DataPacker/Source/BlenderExport.cpp:168-184, DataPacker/Source/ExportJobs/ExportJob.cpp:149
- 2026-11-06 claude/bce2711e-800d-4a71-886c-78431dc6cb86 — DataPacker/Source/ExportJobs/ExportJob.cpp:114 —
  Fold `mCrc` (the path CRC) into the content key in `SetContentKey`, so a case-only rename gets its own cache entry
  instead of resolving to the old one on the case-insensitive volume. Trees sharing a relative path and content still
  share one entry, and the verified body CRC then proves the header holds the job's own path CRC. Existing entry names
  change, so old entries are orphaned and those jobs re-export once.
  — makes unnecessary: DataPacker/Source/ExportJobs/ExportJob.cpp:234-238
- 2026-11-06 claude/7da75c20-196f-414a-9394-05ed10b2d13e — refactor observation Engine/Source/Frame/NavQuery.cpp:702 — A* scratch scales with graph vertices, yet it runs on a dispatch worker whose workbuffer starts at 65,536 bytes (Common/Threading/Multithreading.cpp:12), so a graph of about 3,600 vertices grew it mid-frame on a temporary sealed-cell build — size the worker buffer from the largest navigation graph.
- 2026-11-06 claude/574f4faa-873a-4ce7-8a18-9457ba135b3a — refactor observation: Tools/AgentHarness/AgentHarness.cpp:136 — pre-existing; every `RefreshHeartbeatIfDue` caller passes a deadline, so the `nullopt` default and the `:150` branch are unreachable — make the deadline a required parameter and drop that branch

- 2026-11-05 claude/02d81b1e-9526-44c9-b600-56fc582cd210 — Engine/Source/Profile/ProfileManager.cpp:378 — `GpuRead` treats a query still in flight and a query this recording never writes the same way, so an inactive row keeps its old sample; clearing the `mpGpuTimers` rows when command buffers are re-recorded would fix that at the base, since a row only changes between active and inactive at re-recording
  — makes unnecessary: the paired empty timestamps the inactive water foam recording keeps (`Engine/Source/Graphics/Managers/AGENTS.md` `### CommandBufferManager`)
- 2026-11-05 claude/4a3377b2-9193-4035-830d-1b0d217e7190 — refactor observation Engine/Source/Frame/NavQuery.cpp:544 —
  TryVisibleNeighbors walks the grid before TryNeighbor's length prune — test tentative length against pGCost first in
  sealed queries.

- 2026-11-05 claude/f8358cc9-467b-430e-9258-b1818603694a — Projects/ModelViewer/Source/ModelViewerApplication.cpp:1306 — add `ImGuiSliderFlags_NoInput` to the "Inset (m)" slider so no typed text reaches it; drag values stay within 0 to 2 m and the loaded value is already validated finite, at the cost of Ctrl+click exact entry — makes unnecessary: ModelViewerApplication.cpp:1306 `std::isfinite(fOutlineInset)` and its comment at :1304

