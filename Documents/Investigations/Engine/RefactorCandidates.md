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

- 2026-11-14 claude/5939deae-ae49-4fa3-8c68-a2981f6a14ef —
  Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h:125
  — drop NextRouteWaypoint's `rbFinal` out parameter and the snap-and-compare at
  ShipBehaviours.cpp:542 that fills it, since no caller reads the result
  (Battleships.cpp:508, Omnis.cpp:673 and :697, ShipBehaviours.cpp:564 and :828,
  FleetCoordination.cpp:118) — makes unnecessary:
  Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp:507
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
