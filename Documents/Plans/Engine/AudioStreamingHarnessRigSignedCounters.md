<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T13:12:56.193Z","dependsOn":[]} -->
# Store the audio streaming harness rig's sequence and history counters as int64_t

## Context

`Documents/C++StyleGuide.txt` rule 13 (lines 101-106) defaults our own integers to `int64_t` and keeps unsigned types for bitwise work. The `audio_streaming_harness_rig` agent command's record sequence, reservation bounds, retry count, and per-partition history head/dropped counters are unsigned, while their consumers already hold or cast them to `int64_t`. Origin: `Documents/Investigations/ChangeWorkflow/OwnIntegerTypeSweepDeferredFixes.md` (entries `AudioStreamingFixture` F17, F31, F45, F46, F47, F65, F66, F68, F69, F70), deleted when the Plans are created. The sweep left them unsigned citing full-range unsigned arithmetic and the 2^32 history wrap.

Verified current state:

- `Engine/Source/Agent/Commands/AudioStreamingHarnessRig.h:88` `uint64_t uiSequence`; `:118-120` snapshot `uiReservationStart`, `uiReservationBoundary`, `uiRetryCount`; `:258-259` `std::atomic<uint32_t> uiHead`, `uiDropped`; `:268-270` `std::atomic<uint64_t> muiNextSequence`, `muiRetryCount`, `uint64_t muiReservationStart`.
- Consumers already treat them as signed: `AudioStreamingHarnessRig.cpp:479` loads `uiHead.fetch_add` into `int64_t iIndex`; `:503` stores `static_cast<int64_t>(uiSequence)` into the `std::atomic<int64_t> iPublishedSequence`; `:570-571` copy head and dropped into the snapshot's `int64_t iMainHead`/`iMainDropped` (`AudioStreamingHarnessRig.h:127,130`); `Projects/BrokenEngineSandbox/Source/Agent/Commands/AgentCommandsAudioStreaming.cpp:569` casts `uiReservationBoundary` into `int64_t iStartSequence`, and `:334` casts `iAfterSequence` back to `uint64_t` to compare.
- The full-range concern is unreachable: the sequence starts at 0 and gains one per recorded history entry (`AudioStreamingHarnessRig.cpp:485`), entries are capped at 32 + 24 + 24 per scenario (`AudioStreamingHarnessRig.h` `History<32>`, `History<24>` x2) with excess counted as dropped, and the retry count gains one per `AudioStreamingHarnessRig::CountRetry` call during an active scenario (`:816`); none approaches `INT64_MAX`. The head and dropped counters reset each scenario (`:277-278`), and each gains one per record attempt, far below 2^32 or 2^63.

## Design

The author recommends converting each value to `int64_t` with the rule 3 `i`/`mi` prefix, keeping memory orders and logic unchanged:

| Current | Recommended |
|---|---|
| `AudioStreamingHarnessRigRecord::uiSequence` | `int64_t iSequence` |
| `AudioStreamingHarnessRigSnapshot::uiReservationStart`, `uiReservationBoundary`, `uiRetryCount` | `int64_t iReservationStart`, `iReservationBoundary`, `iRetryCount` |
| `History::uiHead`, `uiDropped` | `std::atomic<int64_t> iHead`, `iDropped` |
| `muiNextSequence`, `muiRetryCount` | `std::atomic<int64_t> miNextSequence`, `miRetryCount` |
| `muiReservationStart` | `int64_t miReservationStart` |

Then drop the casts this makes redundant: `AudioStreamingHarnessRig.cpp:503` (`static_cast<int64_t>(uiSequence)`), `:646` (`static_cast<int64_t>(boundary - start)`), `:649` (`static_cast<uint64_t>(i)`), and `AgentCommandsAudioStreaming.cpp:334` and `:569`. `std::atomic<int64_t>` is lock-free on x64 like the current types, so the atomics' synchronization is unchanged.

Rationale: every consumer already reads these as signed, so the conversion removes conversions rather than adding them, and no reachable value changes. nlohmann JSON writes a non-negative `int64_t` with the same text as the equal `uint64_t`, so the command's JSON output (`AgentCommandsAudioStreaming.cpp:227-229,283`) is byte-identical.

Kept unsigned (rule 13's bitwise unsigned bullet): `muiScenarioGate`/`uiScenarioGate` (`AudioStreamingHarnessRig.h:115,266`; parity tested with `& 1` at `AudioStreamingHarnessRig.cpp:135,307,467`) and `muiHoldToken` (`:276`) with its packed hold generation (`AudioStreamingHarnessRig.cpp:52`).

Risk tier: Tier 2 — `.agents/references/risk-tiers.md`; trigger: struct member types and names change across `Engine/Source/Agent/Commands` and the sandbox agent command, which excludes Tier 1's no-public-signature condition. No wire, CRC, serialization, replay, or synchronization surface changes; the agent command channel is an exempt developer tool (`Engine/Source/Agent/AGENTS.md` `## Architecture`).

## Critical files

- `Engine/Source/Agent/Commands/AudioStreamingHarnessRig.h`
- `Engine/Source/Agent/Commands/AudioStreamingHarnessRig.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/AgentCommandsAudioStreaming.cpp`

## In scope

- The members in the Design table and every read or write of them: `AudioStreamingHarnessRig.cpp:277-278,288-289,479-485,489,503,530-532,570-571,597,601,644-649,816`; `AgentCommandsAudioStreaming.cpp:207,227-229,283,334,388,569,789`.

## Out of scope

- `uiScenarioGate`/`muiScenarioGate`, `muiHoldToken`, `PackHoldToken`, `NextHoldGeneration` and the hold-generation shift.
- Every other member of the harness rig, the snapshot, and the command, including the `4'294'967'295i64` pool-index sentinels.
- The JSON field names and the command's action set.

## Acceptance criteria

- No member in the Design table keeps an unsigned type, and the five casts named in `## Design` are gone.
- The `audio_streaming_harness_rig` actions return the same JSON fields with the same values as before the change.

## Notes

- Verification: compile the client (`/compile`), then `/agent-harness` runs `audio_streaming_harness_rig` `start`, `inspect`, `coexistence`, and `saturate` and confirms `kGapFree`/coherence results and the reported `reservationStart`, `reservationBoundary`, `retryCount`, and record `sequence` values behave as before.
- Entries the investigation listed for this harness rig that keep their type (hold generation packing) are covered by the bitwise unsigned bullet of rule 13 in `Documents/C++StyleGuide.txt`.
