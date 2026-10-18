# BrokenEngineSandbox Harness Launch

[Back to AgentHarness hub](../AgentHarness.md)

Launch through the shared
[launch recipe](../../../../.agents/skills/agent-harness/references/launch.md),
with the values in the hub's `## Launch values` table and machine-readable
launch configuration. This file holds only the BrokenEngineSandbox launch notes.

## Client persisted state

The client's own persisted state, which a server `reset` never clears, is
`ClientState.bin`: the client writes it once during normal orderly shutdown
under `<absolute app-data root>\Broken Engine Sandbox`; focus/zoom changes made
during play remain in memory and may be lost on exception, crash, or hard kill.

## Durable caveats

- Client weapon-mode requests received during paused or other zero-tick updates and internally queued flagship navigation updates persist until the first advancing update. Client `kClientFleetNavigationDelay` requests apply immediately. To verify load-requeued flagship updates, use `load {"pauseAfterLoad":true}` and inspect `pendingFlagshipUpdateCount` before unpausing.
