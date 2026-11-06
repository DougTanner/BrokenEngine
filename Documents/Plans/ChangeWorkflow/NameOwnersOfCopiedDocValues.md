<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-05T13:48:44.521Z","dependsOn":[]} -->
# Name owning identifiers in place of copied documentation values

## Context

Documentation that copies a value the code owns goes stale silently, and
`/progressive-disclosure-review` reports it only on session-changed prose. That
leaves existing lines. One has already gone wrong: the comment at
`Engine/Data/Shaders/ShaderGlobalLayout.h:268` states a range that contradicts
`gWaterBreakBlendCurve` at `Engine/Source/Ui/WaterWrappersBase.cpp:122`.

A pattern check over `AGENTS.md`, `.agents/**/*.md`, and
`Documents/Architecture/*.md` was measured and rejected, because 26% of its hits
were not violations. The table in `## In scope` is the hand-judged result
instead. Each of 58 candidate lines was read, and a line appears only when an
owning symbol can be named.

## Design

Replace each listed copied value with its owning identifier, keeping the
sentence's meaning and any rationale it carries. Where a value is derived, name
the identifiers it derives from. Where a parenthetical only restates the value,
delete it. Cite the owning file only when the reader cannot find the identifier
otherwise, and link nothing new.

This is prose only. No identifier, script parameter, or value changes.

## Critical files

- `Documents/Architecture/Network.md`
- `Engine/Source/Agent/AGENTS.md`
- `Engine/Data/Shaders/ShaderGlobalLayout.h`

## In scope

Line numbers are from this Plan's creation; each row's text names the value to
find if a line has moved.

| File | Lines | Value | Owner |
|---|---|---|---|
| `.agents/skills/agent-harness/references/command-reference.md` | 45-46 | `cell_coordinate_probe` area height and width | `kfCellHalfHeight`, `kfCellHalfWidth` (`Engine/Source/Frame/GridCoord.h`) |
| same | 351 | update interval at two time scales | `TimeStep::SimulationToWall(kTickNanoseconds)` |
| `.agents/skills/agent-harness/references/worker.md` | 571 | `--timeout-ms` default | `kiDefaultResponseTimeoutMilliseconds` (`Tools/AgentHarness/AgentHarness.cpp`) |
| same | 841 | `maxWidth` and `quality` defaults | `ScreenshotRequest::iMaxWidth`, `ScreenshotRequest::iQuality` (`Engine/Source/Graphics/Graphics.h`) |
| `.agents/skills/compile/references/worker.md` | 291 | build lock wait | `kiBuildLockWaitSeconds` (`Tools/WorktreeCli/BuildCommand.cpp`) |
| `.agents/skills/deploy-server/references/azure-facts.md` | 74 | budget figure | `amount` of `budget` (`assets/server.bicep`) |
| `.agents/skills/finalize-changes/references/scripts.md` | 234, 242 | index-lock budget | `-IndexLockWaitSeconds` default (`Invoke-FinalizeLanding.ps1`) |
| `.agents/skills/next-plan/references/claim-results.md` | 108 | listing size default | `-Top` default (`Get-NextPlanList.ps1`) |
| `.agents/skills/sweep/SKILL.md` | 101 | parallelism default | `-Throttle` default (`Invoke-Sweep.ps1`) |
| `DataPacker/Source/ExportJobs/AGENTS.md` | 268 | export sample rate | `audiorepair::kiAudioExportSampleRate` |
| `Documents/AGENTS.md` | 17 | tick rate | `kiTickRate` |
| `Documents/Architecture/Network.md` | 10 | ACK bitfield tick span; the timestamp's byte size has no owner and stays | `kiNetworkBufferSize` |
| same | 111-113 | jitter safety margin and its tick equivalents | `kiJitterSafetyMicroseconds`, `kiTickRate` |
| same | 122-123 | streak length and its duration | `common::Smoothed::kiCapacity`, `kiTickRate` |
| same | 230, 232 | stall gap and grace length | `kBudgetStallGrace` (`Engine/Source/Network/Server/Server.cpp`) |
| same | 256 | updates to disconnect and their duration | `kiRateViolationDisconnectCount` |
| same | 272 | slow-motion update window | `TimeStep::SimulationToWall(kTickNanoseconds)` |
| same | 317 | the `kClientDestroyFleetMemberRequest` Min and Max cells | `GameMessages::DestroyFleetMemberRequestMessage::kiSize` plus the type byte the table's heading counts |
| `Documents/Plans/AGENTS.md` | 32 | claim lifetime | `kiClaimLifetimeTicks` (`Tools/WorktreeCli/PlanScheduler.cpp`) |
| `Engine/Data/Shaders/ShaderGlobalLayout.h` | 268 | the range in the comment; delete it and let `gWaterBreakBlendCurve` name the wrapper that owns it | `gWaterBreakBlendCurve` |
| `Engine/Source/Agent/AGENTS.md` | 71 | bind retry window | `kiMaximumBindAttempts`, `kBindRetryInterval` |
| same | 79, 85, 86 | listener and socket wait | `kListenerRetryInterval` |
| same | 88 | response flush deadline | `kResponseFlushTimeout` |
| `Engine/Source/Audio/AGENTS.md` | 7, 28 | mastering rate | `kiMasteringSampleRate` |
| `Engine/Source/Frame/AGENTS.md` | 36 | cell height and apothem | `kfCellApothem` |
| `Engine/Source/Frame/Collections/Pushers/AGENTS.md` | 40 | coincidence distance and correction | `kfPusherCoincidentDistance`, `kfPusherCoincidentCorrection` |
| `Engine/Source/Network/Client/AGENTS.md` | 59 | subscription timeout, both uses | `kSubscriptionTransitionTimeout` |
| `Tools/WorktreeCli/AGENTS.md` | 76 | scheduler guard wait | `kiSchedulerGuardWaitSeconds` |

## Out of scope

- Any line not in the table. The other candidates hold host limits, vendor or
  tool facts, measured estimates, values the document itself owns, or a literal
  with no identifier.
- Other rows of the `Documents/Architecture/Network.md` contract table.
- Any change to code, a script parameter, or a value.
- Static-check rows, patterns, or scanners: the pattern check was rejected.
- Naming a value at its literal: a literal with no identifier gets no new
  constant under this Plan.

## Risk triggers and invariants

- Tier 1. Trigger: documentation and one comment, with no public signature or
  invariant exposure.
- `ShaderGlobalLayout.h` is a dual-language header, so its comment edit owes the
  C++ reviews and a client, server, and ModelViewer build.
- The `.agents/` and `AGENTS.md` edits take the Change Workflow instruction-prose
  review. Those prose edits hold no Frame, CRC, wire, or `.pack` exposure.

## Acceptance criteria

- Each table row's value is gone from its line and the owner it names appears
  there, and each named identifier exists in the tree.
- A sentence keeps its rationale: the jitter margin's reason for sitting off the
  tick boundary survives in `Documents/Architecture/Network.md`.
- `/compile` builds the BrokenEngineSandbox client and server, and ModelViewer.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
