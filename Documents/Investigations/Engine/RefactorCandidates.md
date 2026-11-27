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

- 2026-11-26 agy/b4fa1a59-32a9-406a-bc97-2a13634c6bdc —
  Projects/ModelViewer/Source/ModelViewerApplication.cpp — 17,307/10,000
  bt-token-v1 — island selection and flight-body updates form a cohesive
  responsibility.
- 2026-11-26 agy/b4fa1a59-32a9-406a-bc97-2a13634c6bdc —
  Engine/Source/Graphics/Render/GlobalUniforms.cpp — 10,101/10,000 bt-token-v1
  — shadow uniform publication forms a cohesive block under
  PopulateShadowParameters.
- 2026-11-25 agy/67048fc4-9412-489e-bec4-ad39620046af — Size observation:
  Engine/Source/Graphics/Render/GlobalUniforms.cpp:242 — 10,119/10,000
  bt-token-v1 — its five consecutive shadow-population functions form a
  cohesive candidate for a separate shadow-uniform implementation file.
- 2026-11-25 agy/67048fc4-9412-489e-bec4-ad39620046af — Size observation:
  Engine/Source/Graphics/Render/GlobalUniforms.cpp — 10,456/10,000 bt-token-v1
  — the smoke swirl, wind, and curl time offsets adjacent to base-height
  publication form a cohesive block for SmokeUniforms.cpp.
- 2026-11-25 agy/67048fc4-9412-489e-bec4-ad39620046af — Size observation —
  Engine/Data/Shaders/ShaderGlobalLayout.h measures 5,238 bt-token-v1, the only
  reviewed file above 5,000.
- 2026-11-25 agy/1dfed8ef-db5a-4dd6-8c28-98737a8d519e —
  Projects/BrokenEngineSandbox/Source/Input/GameKeybinds.cpp:78 — Have both
  loaders share the existing sequence of header, exact-length, EOF,
  key-validity, and duplicate checks. Each loader can keep its own versioned
  file type and publish its table only after the shared read succeeds. This
  removes the second implementation of those checks.
- 2026-11-25 agy/3ec6e101-2641-4d39-887d-7da8d04df722 —
  Projects/BrokenEngineSandbox/Source/Game.cpp:157 — Share the selected ship's
  coordinate lookup across camera shake, tracking, and reconciliation. Let each
  caller supply its existing fallback coordinate. — makes unnecessary: the
  repeated ownership checks at
  Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp:80 and
  Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp:36
- 2026-11-25 agy/3ec6e101-2641-4d39-887d-7da8d04df722 — Size observation:
  Engine/Source/Agent/AgentCommandsClientGeneric.cpp — 11,797/10,000 bt-token-v1
  — key parsing and scripted UI input commands form a cohesive group.
- 2026-11-25 agy/d7526070-354e-4684-946f-69451661d537 —
  Engine/Source/Frame/Collections/HexShields/HexShieldsRender.cpp:29 — Size the
  shield buffer from the same renderable cells the draw visits, counting the
  camera cell once. That makes the reserved capacity an upper bound on emitted
  shields — makes unnecessary:
  Engine/Source/Frame/Collections/HexShields/HexShieldsRender.cpp:57
- 2026-11-25 agy/d7526070-354e-4684-946f-69451661d537 —
  Engine/Source/Graphics/Render/LightingUniforms.cpp:336 — Compute the
  zero-or-one refresh count once and use it for both the spread instance count
  and the compute dispatch Z count — makes unnecessary:
  Engine/Source/Graphics/Render/LightingUniforms.cpp:351
- 2026-11-25 agy/d7526070-354e-4684-946f-69451661d537 — Size observation at
  Engine/Data/Shaders/ShaderGlobalLayout.h — 5,405 bt-token-v1, above the
  review skill’s approximate 5,000-token threshold.
- 2026-11-25 agy/3fbdabf7-211b-4353-93ea-b970b8e32e36 — Size observation —
  `Tools/WorktreeCli/PlanScheduler.cpp` measures 12,164/10,000 `bt-token-v1` —
  claim-record validation, lookup, and healing form a cohesive follow-up split.
- 2026-11-24 agy/7d3e43fa-1ef7-4ffe-9344-2e2e7dc7393b —
  Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp:436
  — Place terrain bounce immediately after pusher response inside the existing
  live-ship branch — makes unnecessary: Battleships.cpp:500.
- 2026-11-24 agy/7d3e43fa-1ef7-4ffe-9344-2e2e7dc7393b —
  Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp:666
  — Place terrain bounce immediately after knockback inside the existing
  dying-ship branch. Keep the live Fighter’s terrain rise after the X correction
  — makes unnecessary: Fighters.cpp:669.
- 2026-11-24 agy/7d3e43fa-1ef7-4ffe-9344-2e2e7dc7393b — Size observation:
  `Engine/Data/Shaders/ShaderGlobalLayout.h` is 5557 `bt-token-v1` (over
  ~5,000); splitting is follow-up work.
- 2026-11-24 agy/c8b8acf3-a87f-45e0-b77c-5c12d7bd857c — Engine/Source/Network/Client/ReconcileReplayTick.cpp:90 — Check remaining capacity in the full-replay loop before each tick, so an injected full state can reset the budget. Both catch-up loops already enforce their budgets, which would let the shared tick function return void. — makes unnecessary: the success assertion at Engine/Source/Network/Client/ReconcileReplayTick.cpp:273
- 2026-11-24 agy/9242e034-2cbe-469a-98a8-faf3a29768e1 — Size observation — Engine/Data/Shaders/ShaderGlobalLayout.h measures 5,467 `bt-token-v1`.
- 2026-11-23 agy/0dd9242c-5f71-4111-812c-71056a74cb82 — Size observation — Measure-Tokens.ps1 reports 5,448 bt-token-v1 for Engine/Data/Shaders/ShaderGlobalLayout.h, above the skill’s approximate 5,000-token threshold.
