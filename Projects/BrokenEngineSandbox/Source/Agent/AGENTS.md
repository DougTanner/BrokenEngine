# Agent - Game Command Dispatch

## Overview

Game-owned command handlers expose simulation, scene, save/replay, and diagnostic operations through the engine agent transport (`../../../../Engine/Source/Agent/AGENTS.md`). Engine-owned shared commands dispatch first; the game dispatcher then handles game-owned both-endpoint commands, then — under `BT_CLIENT` — the engine's client-generic handler, before routing remaining commands to its side-specific handlers under `BT_CLIENT` or `BT_SERVER`.

## Contracts

- Treat JSON parameters as hostile external input. Validate required fields, types, ranges, and referenced state; throw to let the engine transport produce a failure response.
- The engine owns client capture, window state, UI/input automation, and the GPU `query_profile` query (`../../../../Engine/Source/Agent/AGENTS.md`). Game client handlers own scene queries, the client full-state fixture, the desync probe, and moving the client's own grid cell. Cell moves require a connected session whose player is already assigned, because before assignment the client subscription policy falls back to origin and discards the requested cell.
- Server handlers own simulation controls, status-change injection, frame and CPU profile queries, and save/replay operations. Server `query_profile` may acknowledge one retained raw-profile event by its exact nonzero sequence; `inject_status_changes` may request the matching activation arm only as an exact `{arm:true}` object. The arm is accepted only in a normal unpaused 1/1 server with recording/replay inactive, and validated changes plus the arm commit as one main-thread transaction; an occupied or overrun event rejects the batch before queue mutation. Save/load `file` values are appdata-relative bare filenames: reject empties, embedded NUL, separators, `..`, `:`, and Windows reserved device basenames.
- Replay commands fail when `kbDebugInput` is disabled because the simulation will not consume those requests.
- Replay transfer fixtures are server-only debug controls. Keep their injected state deterministic; a Blaster fixture must materialize a newly live destination's derived terrain grid before selecting a terrain-clear position for both its spawn and first fixed-tick movement, since Blasters destroy themselves on terrain contact.
- Commands that wait for a later frame, for the queue where messages wait for the renderer to pick them up, or for an input script use the engine deferred-response path. Ensure every deferred operation can resolve or fail without blocking the single in-flight channel.
- Scene queries are client-only and read render-visible state. Server frame queries read deterministic simulation state and must preserve collection/type validation.
- The frame registry keeps no rows, so nothing about it is enumerable or countable through a query. Its state reaches the harness only as the handle columns the collections themselves own, and the both-endpoint `registry_fixture` proves its behavior over its own fixed local data rather than over a frame.

## See Also

- `../../../../Engine/Source/Agent/AGENTS.md`
- Game Save: `../Save/AGENTS.md`
- `../../../../Tools/AgentHarness/AGENTS.md`
