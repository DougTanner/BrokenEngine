# BrokenEngineSandbox Harness Verification

[Back to AgentHarness hub](../AgentHarness.md)

## Authoritative verification

Set up server and client state with the recipe below, then verify and release per the skill's Authoritative verification evidence principles and lifecycle checklist.

1. Set up server state with `reset`, then `inject_payload` entries at a coord from `status.activeCoords`; confirm through `query_omnis`/`query_frame`.
2. Launch/connect the client and require `status.clientCount` to increase.
3. Use `describe_ui` before label-addressed `click`, `hover`, or `set_slider`; use `key`/`mouse` for raw input.

### Chosen omni placement

To place omnis at exact positions instead of the single default spawn point,
send one `inject_payload` batch holding two `SpawnOmni` entries on the same
active coord whose `vecPosition` values lie about 600 m apart, for example
`"vecPosition":[-300,0]` and `"vecPosition":[300,0]` (a cell is 900 m square,
so a position must stay inside +/-450 or the batch fails). No response reports the tick that
applied the batch, so poll `query_omnis {"coord":[x,y]}` on that coord until
both minted `globalIds` from the injection response appear in `omnis` (`total`
rises by two), then send `pause {"paused":true}` and read the exact positions
with a final `query_omnis`.

### Forced reconciliation re-simulation

At 1/1 timescale the client runs `targetBehindTicks` behind the latest server
tick, so a server injection reaches the client before the client simulates that
tick and nothing is re-simulated. To force a rollback and re-simulation, the
injection must apply on a tick the client has already simulated: slowing the
timescale shrinks the lag, and pausing until the client's `tailTick` passes the
server tick guarantees it. Command contracts:
[server](commands-server.md#server-commands),
[client](commands-client.md#client-commands), and the engine-shared
[`get_logs`/`set_log_level`](../../../../.agents/skills/agent-harness/references/command-reference.md).

1. Client `set_log_level {"category":"Network","level":"Debug"}`.
2. Server `timescale {"faster":false}` once, then poll client `query_profile` until `clock.targetBehindTicks` is at most 2 (about 11 s at 1/2).
3. Server `pause {"paused":true}`; after about 2 s, require the client `describe_scene {"includeUnits":false}` `cellStates` row whose `coord` is `[0,0]`, the cell step 4 injects into, to report `tailTick` above the server `status.tick`. [0,0] must also be in the server's `status.activeCoords`.
4. Server `inject_payload {"entries":[{"coord":[0,0],"type":"SpawnOmni"}]}`; require `deferred:true`.
5. Server `pause {"paused":false}`.
6. Client `get_logs {"pattern":"resimulation completed","category":"Network"}`; require a line with `Replayed: true` and `DesyncTick: -1`.
7. Restore 1/1 with server `timescale {"faster":true}` before release.

The client suppresses that line when a CRC mismatch on the same coord falls within 32 ticks of the last logged one, so space repeated injections on one coord more than 32 ticks apart.

## Durable caveats

The durable caveats are kept with the focused material that exercises them:
[launch](launch.md#durable-caveats), [replay](replay.md#durable-caveats),
[server commands](commands-server.md#durable-caveats), and
[client commands](commands-client.md#durable-caveats).
