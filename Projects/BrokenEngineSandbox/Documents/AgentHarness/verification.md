# BrokenEngineSandbox Harness Verification

[Back to AgentHarness hub](../AgentHarness.md)

## Authoritative verification

Set up server and client state with the recipe below, then verify and release per the skill's Authoritative verification evidence principles and lifecycle checklist.

1. Set up server state with `reset`, then `spawn_players` or `inject_status_changes` at a coord from `status.activeCoords`; confirm through `query_players`/`query_frame`.
2. Launch/connect the client and require `status.clientCount` to increase.
3. Use `describe_ui` before label-addressed `click`, `hover`, or `set_slider`; use `key`/`mouse` for raw input.

### Chosen player placement

To place players at exact positions instead of the single default spawn point, send one `inject_status_changes` batch holding two `SpawnPlayer` changes on the same active coord with `pos` values about 600 m apart, for example `[-300,0]` and `[300,0]` (a cell is 900 m square, so an offset must stay inside +/-450). No response reports the tick that applied the batch, so poll `query_players {"coord":[x,y]}` on that coord until both minted `globalIds` from the injection response appear in `players` (`total` rises by two), then send `pause {"paused":true}` and read the exact positions with a final `query_players`.

## Durable caveats

The durable caveats are kept with the focused material that exercises them:
[launch](launch.md#durable-caveats), [replay](replay.md#durable-caveats),
[server commands](commands-server.md#durable-caveats), and
[client commands](commands-client.md#durable-caveats).
