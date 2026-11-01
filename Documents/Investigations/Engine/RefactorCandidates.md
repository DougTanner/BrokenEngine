# Refactor Candidates

Design shapes in the code that a refactor would improve, kept for a later session to review in batch. At the Change Workflow Verify and land step, main copies these rows here word for word, dropping only session-local IDs — a row's leading `/over-engineering-review` `<ID>`, and a `makes unnecessary:` or `candidate:` tail that names only candidate IDs:

- `/over-engineering-review` `Design suggestions` rows the user did not accept.
- `size observation` and `refactor observation` rows from the `/repo-code-review` and `/glsl-review` `Residuals`, skipping a size observation whose last field is `no qualifying reduction`.
- At most one row from main's own context, on the `refactor observation` form in `.agents/skills/repo-code-review/SKILL.md` `## Handoff`.

Prefix each row with `<YYYY-MM-DD> <session branch> — `. Add new rows directly under `## Entries`, so the newest rows are at the top and main reads only the top of the file. A session reviewing the log removes each row it handles.

## Entries

- 2026-11-01 claude/c5401001-5b07-4667-9379-a63c2a27c40a — Engine/Source/Graphics/Render/LightingUniforms.cpp:48 —
  remove the per-layer held flag and let the per-layer reset flag alone mean "no held area". The held flag is
  cleared at only one site (line 70), and that same branch sets gbLightingTemporalReset, which sets every layer's
  spbTemporalResets. CreateLightingTextures also sets gbLightingTemporalReset before the first frame. The two
  flags are cleared and set together (TemporalAreaLatch::Update clears the reset at line 101 and line 104 sets
  held), so "!held" never holds without "reset" also holding. The old single-layer code had the same redundancy.
  Plan item 5 names a "held flag" per layer, so removing it needs the user to change the plan. — makes
  unnecessary: LightingUniforms.cpp:48, :70, :83 (the !spbHeldVisibleAreas[i] term), :104

- 2026-11-01 claude/2c5731e6-262d-48d2-b0f4-773cd3d679ee — refactor observation —
  Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp:463 — RampBattleshipTurnRate
  and SteerBattleshipHeading are Battleship-local, while Fighters keep the signed-max cone branch plus
  SteerHeadingTowards pair that gave Battleships two alternating turn rates (Fighters.cpp:587) — one shared
  ramped soft-clamp turn in ShipBehaviours, parameterized by type constants, would let Fighters use it and keep
  the two collections parallel
- 2026-11-01 claude/8bce67c8-8b83-4559-a8bd-9856a639aa8b —
  Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipHitboxes.cpp:28 — InitializeShipHitboxes could
  read each controller's model CRC from the scene chunk named in that type's Render model table
  (SceneHeader::modelCrc), instead of keeping its own parallel model CRC table; the hitbox model and the drawn
  model would then share one source and could not disagree — makes unnecessary:
  Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsRender.cpp:132,
  Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersRender.cpp:132,
  Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/OmnisRender.cpp:149
- 2026-10-31 claude/4651544d-53aa-4a04-b074-f08eb3cea56e — refactor observation — Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp:678 — a random-spawn Omni scans its enemies with line-of-sight tests twice per tick, once in FindAttackTarget for steering and once in AcquireTarget (:698) for fire and look — one per-row targeting pass whose result feeds both steering and the blaster and look picks would remove the repeated scan and sight tests
- 2026-10-31 claude/6474a6e5-0b43-47ad-81ad-b041b5850efe — Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/FleetCoordination.cpp:249 — let `ChooseFleetTactic` return only the plain Attack tactic for both types
  and move the veer-off start and continuation into the Fighter script (`Fighters.cpp`, which already owns
  `pVecVeerOffPoints`); the shared function would then need no type test and no veer-off output parameter.
  This goes beyond the user-chosen Option 3 (plan line 25) and touches Fighter code outside this change,
  so it is for the user to decide only. — makes unnecessary: FleetCoordination.cpp:249, FleetCoordination.cpp:259, Omnis.cpp:625
