# Refactor Candidates

Design shapes in the code that a refactor would improve, kept for a later session to review in batch. At the Change Workflow Verify and land step, main copies these rows here word for word, dropping only session-local IDs — a row's leading `/over-engineering-review` `<ID>`, and a `makes unnecessary:` or `candidate:` tail that names only candidate IDs:

- `/over-engineering-review` `Design suggestions` rows the user did not accept.
- `size observation` and `refactor observation` rows from the `/repo-code-review` and `/glsl-review` `Residuals`, skipping a size observation whose last field is `no qualifying reduction`.
- At most one row from main's own context, on the `refactor observation` form in `.agents/skills/repo-code-review/SKILL.md` `## Handoff`.

Prefix each row with `<YYYY-MM-DD> <session branch> — `. Add new rows directly under `## Entries`, so the newest rows are at the top and main reads only the top of the file. A session reviewing the log removes each row it handles.

## Entries

- 2026-11-05 claude/02d81b1e-9526-44c9-b600-56fc582cd210 — Engine/Source/Profile/ProfileManager.cpp:378 — `GpuRead` treats a query still in flight and a query this recording never writes the same way, so an inactive row keeps its old sample; clearing the `mpGpuTimers` rows when command buffers are re-recorded would fix that at the base, since a row only changes between active and inactive at re-recording
  — makes unnecessary: the paired empty timestamps the inactive water foam recording keeps (`Engine/Source/Graphics/Managers/AGENTS.md` `### CommandBufferManager`)
- 2026-11-05 claude/4a3377b2-9193-4035-830d-1b0d217e7190 — refactor observation Engine/Source/Frame/NavQuery.cpp:544 —
  TryVisibleNeighbors walks the grid before TryNeighbor's length prune — test tentative length against pGCost first in
  sealed queries.

- 2026-11-05 claude/f8358cc9-467b-430e-9258-b1818603694a — Projects/ModelViewer/Source/ModelViewerApplication.cpp:1306 — add `ImGuiSliderFlags_NoInput` to the "Inset (m)" slider so no typed text reaches it; drag values stay within 0 to 2 m and the loaded value is already validated finite, at the cost of Ctrl+click exact entry — makes unnecessary: ModelViewerApplication.cpp:1306 `std::isfinite(fOutlineInset)` and its comment at :1304

