<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-13T13:09:47.473Z","dependsOn":[]} -->
# De-duplicate the describe_scene unit loops

## Context

`CommandDescribeScene` (`Projects/BrokenEngineSandbox/Source/Agent/AgentScene.cpp:114-352`) emits unit rows through three loops that are the same code: players (240-272), spaceships (274-304), and blasters (307-338). Each iterates the post-render rows, rebases the interpolated position through `basis`, culls with `InVisibleArea`, checks the shared `iMaximumUnits` budget (setting `bTruncated` and breaking that type's loop for this cell), projects with `WorldToScreen`, pushes a row holding `type`, `coord`, `local`, and `screen`, and increments `iUnitCount`. Only the collection types and the type-specific fields differ: `globalId`, `armor`, `shield`, `alignment`, and `flags` for players; `health`, `alignment`, and `flags` for spaceships; `alignment` and the three wind-trail fields for blasters.

Origin: a user-directed read-only review of the top 20 functions by complexity mass at `43450112`. The user's rule: a function is never split because it is big; code that belongs together stays together, and only a piece that does not belong moves. The copies are the piece; parameter validation, camera and game state, the island loop, and the counts were judged to belong and stay as is.

## Design

Recommended: a same-file static template, for example `template <typename RowBuilder> static void AppendVisibleUnits(const char* pcType, const XMVECTOR* pVecPositions, int64_t iRowCount, engine::GridCoord coordinate, const engine::RenderBasis& basis, const XMFLOAT4& f4VisibleArea, int64_t iMaximumUnits, int64_t& riUnitCount, bool& rbTruncated, nlohmann::json& rUnits, RowBuilder&& BuildRow)`. It holds the loop body once. `BuildRow(i, vecLocalPosition, vecScreen)` returns a JSON object with only the type-specific fields, without branches; the helper adds `type`, `coord`, `local`, and `screen` to it and pushes it. Each call site keeps its `bInclude*` check, reads its own interpolate and post-render collections, and passes `pVecPositions` with the post-render `iCount`. Exact parameter order and types are the implementer's choice.

Unchanged: call order stays players, spaceships, blasters, because the budget depends on type order (comment at 306). Truncation still breaks only the current type's loop for the current cell. `nlohmann::json` objects are key-ordered, so every row's text is unchanged.

Risk tier: Tier 1 (`.agents/references/risk-tiers.md`). Trigger: a local behavior-preserving de-duplication into a file-static template in one client-only translation unit, with no public signature or invariant exposure. Scene queries read render-visible state only and the agent channel is an exempt developer tool (`Engine/Source/Agent/AGENTS.md` `## Architecture`).

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/AgentScene.cpp`

## In scope

- New file-static template `AppendVisibleUnits` in `AgentScene.cpp`.
- The three unit loops in `CommandDescribeScene` (240-338), replaced by three calls.

## Out of scope

- Parameter validation, camera, UI and game state, fleets, the island loop, counts, and the result assembly in `CommandDescribeScene`.
- `PlayerFlagNames`, `SpaceshipFlagNames`, `Vector2ToJson`, `BuildFleets`.
- Any unit type, field, budget, or truncation change, and `commands-client.md`.

## Acceptance criteria

- Client builds (`/compile`).
- `/agent-harness`: with units on screen, `describe_scene` returns rows of all three types with the current field sets; `{"maxUnits":1}` returns one row and `truncated` true.

## Notes

- A later added unit type becomes one more call site, not one more loop.

## Coordination

- `Documents/Plans/Game/ShipTypeSplit.md` replaces the player and spaceship unit types of `describe_scene` with `fighter`, `omni`, and `battleship`, editing these loops in `AgentScene.cpp`. Whichever lands second rebases onto the other's edits; landed after this Plan, each ship type is one `AppendVisibleUnits` call.
