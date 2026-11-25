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
