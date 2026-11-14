# BrokenEngineSandbox Harness Launch

[Back to AgentHarness hub](../AgentHarness.md)

Launch through the shared
[launch recipe](../../../../.agents/skills/agent-harness/references/launch.md),
with the values in the hub's `## Launch values` table and machine-readable
launch configuration. This file holds only the BrokenEngineSandbox launch notes.

## Client persisted state

The client's own persisted state, which a server `reset` never clears, lives
under `<absolute app-data root>\Broken Engine Sandbox`; a save reproduction
needs two of its files. The client writes `ClientState.bin` once during normal
orderly shutdown; focus/zoom changes made during play remain in memory and may
be lost on exception, crash, or hard kill. `ClientGuid.bin` holds the client's
identity and is written whenever the server accepts its connection. A run
reproducing a baseline save's view in a fresh app-data root copies both files
beside the save: the save keys every fleet and owner record on that GUID, so a
client without it connects as a new client that owns none of them.

## Durable caveats

- Client `kClientFleetNavigation` requests apply immediately; the effect of a
  behaviour or hold change is in
  `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`
  `## Deterministic Tick Contracts`.
