# Refactor Candidates

Design shapes in the code that a refactor would improve, kept for a later session to review in batch. At the Change Workflow Verify and land step, main copies these rows here word for word, dropping only session-local IDs — a row's leading `/over-engineering-review` `<ID>`, and a `makes unnecessary:` or `candidate:` tail that names only candidate IDs:

- `/over-engineering-review` `Design suggestions` rows the user did not accept.
- `size observation` and `refactor observation` rows from the `/repo-code-review` and `/glsl-review` `Residuals`, skipping a size observation whose last field is `no qualifying reduction`.
- At most one row from main's own context, on the `refactor observation` form in `.agents/skills/repo-code-review/SKILL.md` `## Handoff`.

Prefix each row with `<YYYY-MM-DD> <session branch> — `. Add new rows directly under `## Entries`, so the newest rows are at the top and main reads only the top of the file. A session reviewing the log removes each row it handles.

## Entries

- 2026-11-01 claude/8bce67c8-8b83-4559-a8bd-9856a639aa8b — Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipHitboxes.cpp:28 — InitializeShipHitboxes could read each controller's model CRC from the scene chunk named in that type's Render model table (SceneHeader::modelCrc), instead of keeping its own parallel model CRC table; the hitbox model and the drawn model would then share one source and could not disagree — makes unnecessary: Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsRender.cpp:132, Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersRender.cpp:132, Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/OmnisRender.cpp:149
- 2026-10-31 claude/4651544d-53aa-4a04-b074-f08eb3cea56e — refactor observation — Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp:678 — a random-spawn Omni scans its enemies with line-of-sight tests twice per tick, once in FindAttackTarget for steering and once in AcquireTarget (:698) for fire and look — one per-row targeting pass whose result feeds both steering and the blaster and look picks would remove the repeated scan and sight tests
- 2026-10-31 claude/6474a6e5-0b43-47ad-81ad-b041b5850efe — Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/FleetCoordination.cpp:249 — let `ChooseFleetTactic` return only the plain Attack tactic for both types
  and move the veer-off start and continuation into the Fighter script (`Fighters.cpp`, which already owns
  `pVecVeerOffPoints`); the shared function would then need no type test and no veer-off output parameter.
  This goes beyond the user-chosen Option 3 (plan line 25) and touches Fighter code outside this change,
  so it is for the user to decide only. — makes unnecessary: FleetCoordination.cpp:249, FleetCoordination.cpp:259, Omnis.cpp:625
