# Expose the per-tick simulation CRC through `query_frame`

## Context

The simulation's shared CRC is computed every tick and travels the wire, but no agent-visible surface returns it:

- `common::crc_t sharedCrc` lives on `engine::FramePostRenderBase` (`Engine/Source/Frame/FrameBase.h:179`) and is assigned once per tick from `Frame::Crcs()` (`Projects/BrokenEngineSandbox/Source/Frame/FrameTick.cpp:89`; base contribution at `Engine/Source/Frame/FrameBase.cpp:8-22`).
- The server sends it with each coord update (`Engine/Source/Network/Server/ServerSend.cpp:117`) and the client stores the received value for comparison (`Engine/Source/Network/Client/ClientReceive.cpp:379`).
- Only a *mismatch* is observable. `Engine/Source/File/DifferenceStream.h:470` logs `LogDifferences CRC Client: {} Server: {}` on divergence; nothing logs or returns a matching value. `desync_probe` reads `postRender.sharedCrc` internally (`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp:1445`) but omits it from its JSON result (`:1482-1488`).
- `CommandQueryFrame` (`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp:172-179`) already holds the exact `Frame` whose `postRender` carries the value, and returns only collection counts.

Consequence for verification: an agent can prove client and server agree with each other (absence of desync lines), but cannot prove that a change left the simulation bit-identical to a *previous build*, because client and server can drift together identically. This was hit concretely while preparing acceptance checks for the completed plan that motivated this document, `EngineOwnedFrameConstants` (2026-08-12), which was executed and removed from `Documents/Plans/Frame/` at landing: the intended check "compare per-tick CRC sequences before and after the change" was not expressible and had to be weakened to "zero desync lines plus an island-placement comparison".

## Design

Return the already-computed CRC from the existing server query; add no new command and no new computation.

- In `CommandQueryFrame`, add `rResult["tick"] = rFrame.interpolate.iTick;` and `rResult["sharedCrc"] = rFrame.postRender.sharedCrc;` (`common::crc_t` is `uint64_t`, `Common/Crc.h:6`, which nlohmann serializes as an exact JSON integer). Both are plain reads of the frame `QueryFrame` already resolved, executed at the existing agent-command drain point, so no new frame access path or lifetime concern appears.
- Harness use: a consumer polls `query_frame` itself across a scripted, fixed-seed scenario and records the `(tick, sharedCrc)` pairs it gets back. Automating that per-tick sampling in the harness driver is an explicit non-goal of this feature; the feature is the two added result fields and nothing else.
- Reproducible sampling: a free-running server keeps ticking between polls, so two runs need not capture the same set of ticks. Every returned sample carries the tick it belongs to, and a cross-run comparison aligns samples by tick value, comparing only the ticks present in both captures. A paused, stepped, or replay-driven run can make the two captured tick sets equal; a free-running comparison is by tick-keyed intersection.

Rejected alternative: a per-tick `kVerbose` CRC log line. It would put a formatted log call on the deterministic tick path in allocation-tracked code and would be filtered out of the default log threshold, so it costs more in the hot path and buys nothing the query does not already give.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp` — `CommandQueryFrame`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` — `query_frame` result documentation
- `Engine/Source/Frame/FrameBase.h`, `Projects/BrokenEngineSandbox/Source/Frame/FrameTick.cpp` — reference only, no edits

## In scope

- The two added result fields in `CommandQueryFrame` and their documentation in the harness command reference

## Out of scope

- Any change to what the CRC covers, when it is computed, or how it is networked
- Harness driver automation for sampling or diffing a `(tick, sharedCrc)` sequence — a non-goal of this feature; a consumer polls `query_frame` itself
- Any new agent command, and any client-side CRC query (the server frame is the authority; a client-side surface is only worth adding if a concrete need appears)
- Desync detection, reporting, and recovery policy
- Backward-compatibility shims for the previous `query_frame` result shape

## Risk tier and invariants

Expected Change Workflow Tier 2 (one subsystem's tool behavior). The export must not perturb determinism: it is a read of an already-stored member, adds nothing to the CRC input set, and runs on the agent-command path outside deterministic frame execution. Escalate to Tier 3 if implementation finds it needs to move, recompute, or re-time the CRC rather than read it. No wire format, serialization, `.pack`, or replay-compatibility exposure.

## Acceptance criteria

- `query_frame` on a running server returns `tick` and `sharedCrc`, and the returned CRC equals the value the client reports in a deliberately induced desync log for the same tick.
- Two runs of the same scripted, fixed-seed scenario on the same build agree on `sharedCrc` for every tick present in both captures.
- A deliberate simulation-affecting edit changes the CRC on the shared ticks; a pure code-motion edit does not.
- Client and server build clean.

## Revisit When

Wanted before the next determinism-sensitive refactor that needs a cross-build bit-identity acceptance check — the queued Plans under `Documents/Plans/Frame/` and `Documents/Plans/Network/` that move simulation code between Engine and game are the concrete trigger.

## Notes

Not a duplicate of `Documents/Plans/Network/DesyncDebugFrameResponseCorrelation.md`, which makes a desync debug-frame *response* match its request; that plan neither exposes nor reads `sharedCrc` outside the desync path. Adjacent to `AgentQueryGlobalState.md` in this directory, which proposes new server-global `query_*` arms; this document extends one existing Frame query instead and the two do not overlap.
