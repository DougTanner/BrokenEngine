# BrokenEngineSandbox Cross-Cell Verification

[Back to AgentHarness hub](../AgentHarness.md)

### Cross-cell subscription verification

Live recipe for `set_client_grid_coord`. Run it in order.

1. `reset` the server, then inject one flagship at the origin: `inject_payload {"entries":[{"coord":[0,0],"type":"SpawnPlayer","bIsFlagship":true}]}`. Do not set `fleetWantedCoordinate` — the client auto-follows its own ship, which would make the coordinate evidence ambiguous.
2. Give the client the assigned player this command requires. A connected client starts with zero players and the injected flagship is unowned AI, so spawn through the ordinary fleet path: client `click {"label":"[+]##Fleet"}`, then client `click {"label":"[+]##Player"}`. Then poll `set_client_grid_coord {"coord":[0,0]}` until it returns `ok:true`; it returns `ok:false` until the server has assigned the spawned player. Do no coordinate work before that.
3. Set the server Network log level to Debug with
   `set_log_level {"category":"Network","level":"Debug"}` so the decisive server
   lines emit past the default `kInfo` runtime threshold. Do not ask the client
   for `Verbose`: this project compiles Network at a `kDebug` floor, so the
   request clamps and reports `effective:"Debug"`, and the `kVerbose` line
   `Client::ServerCoordFullState ...` is compile-eliminated in every
   configuration. Capture ordered log baselines, server `status`, client
   `describe_scene {"includeUnits":false}`, and both ticks, so later checks can
   identify only appended lines.
4. Send `set_client_grid_coord {"coord":[0,1]}` and require `ok:true` with `clientGridCoord:[0,1]`.
5. Decisive subscription evidence is server `status` showing `[0,1]` active plus a newly appended server line `Server::SendCoordFullState Client: <n> Frame: <tick> Slot: <s> Coord: (0,1) AckFloor: <floor>`. `describe_scene.subscribedCoords` lists local frames including the still-unconfirmed own cell, so treat it only as a coordinate-changed signal.
6. Prove client-side adoption by polling client
   `describe_scene {"includeUnits":false}` about every 0.5 s, for at most 10 s,
   until its `cellStates` holds a row with `coord:[0,1]` and
   `confirmedTick >= 0`; if the bound passes first, the step fails. That row
   must also report `snapshotCount > 0` and `lastFullStateTick` equal to the
   `Frame:` tick of a newly appended
   `Server::SendCoordFullState ... Coord: (0,1)` line. If multiple such lines
   were appended, match the line whose `Frame:` equals `lastFullStateTick`. That
   matching pair, not any client log line, proves the client adopted that full
   state.
7. Require the client tick to stay monotonic and never reset across the move.
8. Capture the clean-log check: no newly appended `LogDifferences CRC Client`, `CONFIRMED DESYNC`, or checksum-mismatch line.
9. Send `set_client_grid_coord {"coord":[0,0]}`, then `release_client_grid_coord {}`, to restore a normal session: the release clears the pin so the client auto-follows its own ship again.
