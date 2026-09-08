<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-09T00:12:46.208Z","dependsOn":["Documents/Plans/Engine/ExtractServerReplayAgentFixtures.md"]} -->
# Normalize isolated agent fixtures and prove command ownership

## Context

Several existing fixtures already keep their invocation state inside Agent command translation units, but their placement and the repository-wide ownership inventory were deferred from the approved agent-fixture work. The collection-layout and registry implementations remain in the broad game dispatcher source (`Projects/BrokenEngineSandbox/Source/Agent/AgentCommands.cpp:16-655`), server packet and pre-handshake fixtures remain in `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerFaultFixtures.cpp`, and the crash-report fixture remains in the shared engine dispatcher (`Engine/Source/Agent/AgentCommandsShared.cpp:190-230`). The real crash-report implementation contains a DxDiag completion guard at `Engine/Source/CrashReport.cpp:155-160`; that production behavior is not fixture payload and must remain intact.

This is the final behavior-preserving normalization and inventory stage. It runs only after the remaining server/replay extraction so the inventory can prove the finished repository state.

## Design

Author's recommendation: move each already isolated fixture family into a focused header/source pair beneath its owning `Source/Agent/Commands/` directory. Keep collection-layout and registry fixtures as separate game command modules; retain the existing server packet/pre-handshake family as a focused game Commands module; and move only the shared crash fixture handler into an engine Commands module. Existing top-level shared/game dispatchers include narrow declarations and retain the same dispatch order.

Do not add production attachment or global state for these fixtures. Their current command or stack lifetime, scope guards, synchronous calls, and isolated fatal-process boundary remain sufficient. Normalize includes and project/filter membership only where required by the moves.

Finish with a static inventory of every fixture-named definition and reference. All fixture payload definitions, enums, DTOs, snapshots, result wrappers, histories, control state, weak/shared command state, and behavior definitions must live under engine or game `Source/Agent/Commands/`. Each remaining outside-Commands match must be assigned to the approved allowlist: the command server's audio-fixture owner declaration, a streaming voice's non-owning command-owned control reference, a necessary narrow debug timing/lifecycle hook, or an existing top-level dispatcher declaration/call. An unassigned match fails acceptance.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommands.cpp` and focused game modules under `Projects/BrokenEngineSandbox/Source/Agent/Commands/` — extract collection-layout and registry fixture implementations while retaining dispatch.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerFaultFixtures.{h,cpp}` and focused game Commands files — normalize server packet and pre-handshake fixture placement without changing fault behavior.
- `Engine/Source/Agent/AgentCommandsShared.cpp` and focused modules under `Engine/Source/Agent/Commands/` — extract only the crash-report fixture handler while retaining shared dispatch.
- `Engine/Source/CrashReport.cpp:155-160` — preserve the production DxDiag completion guard.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox*.vcxproj*` plus affected Agent documentation — project membership and ownership text.

## In scope

- Move the existing collection-layout-capacity, registry, server packet, pre-handshake, and crash-report fixture implementations into focused engine/game Commands modules.
- Retain only narrow declarations and calls in top-level dispatchers, preserving dispatch order and client/server/debug guards.
- Update includes, Visual Studio project/filter membership, and Agent ownership documentation required by the file moves.
- Produce and classify the final repository-wide outside-Commands fixture inventory against the exact four-row allowlist.

## Out of scope

- New fixture behavior, commands, actions, schemas, results, configurable cases, managers, registries, frameworks, abstractions, compatibility paths, or unit tests.
- Changes to collection layout, registry APIs, network packet handling, pre-handshake transport, crash generation/reporting, DxDiag synchronization, ordinary transport, or process-fatal semantics.
- Moving ordinary production crash-report logic into Agent Commands or changing `Engine/Source/CrashReport.cpp`.
- Server/replay extraction; this Plan depends on that work being complete.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the final normalization crosses independently owned engine/game Agent and server fault-path modules and verifies repository-wide ownership while preserving process-fatal behavior.

- Existing command names, accepted/rejected inputs, response fields, debug/client/server gates, dispatch order, and failure text remain identical.
- Packet/pre-handshake fixtures preserve violation and drop accounting, disconnect thresholds, complete state restoration, and continued ordinary traffic.
- The crash fixture retains its no-response process boundary and the real crash-report path, including the DxDiag completion guard.
- No deterministic Frame/CRC, collection layout, registry identity, wire, save, replay, serialization, phase, or threading contract changes.
- No production fixture state or new attachment is introduced.

## Acceptance criteria

- `collection_layout_capacity_fixture` and `registry_fixture` return `passed:true` with every documented Boolean, capacity, and build-specific condition on client and server Debug processes using `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-both.md:9-10`.
- Server packet and pre-handshake scenarios preserve exact response fields, violation/drop accounting, disconnect thresholds, complete preserved-state restoration, and continued ordinary traffic using `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:8-32`, with fresh handshaken sessions where documented.
- Invalid crash fixture gates/schemas fail normally; valid `{}` produces no response, game exit code `0`, harness exit `1`, and a crash report using `.agents/skills/agent-harness/references/command-reference.md:13`; source inspection confirms the DxDiag guard at `Engine/Source/CrashReport.cpp:155-160` is unchanged.
- Normal-valid-traffic baselines from `commands-server.md:19,29,32` produce no new violation, corrupt-drop, or handler-threw log and remain connected; isolated collection/registry commands do not alter that baseline.
- Final static inventory finds every fixture payload/type/state/behavior definition under engine/game `Source/Agent/Commands/` and assigns every outside match to exactly one approved allowlist row; any unassigned payload, forwarding API, or fixture-owned production member fails.
- `/compile` passes `BrokenEngineSandbox` and `BrokenEngineSandboxServer` in `Debug|x64` and `Release|x64`; the full Tier 3 static, code, comment, coherence, style, project-membership, documentation, adversarial, and acceptance routes pass without unit tests.

## Coordination

No nondirectional reciprocal Plan coordination is required. The metadata prerequisite ensures the server/replay extraction is complete before this final inventory and normalization begins.

## Notes

This Plan preserves the approved Stage 4 boundary as a standalone deferred work unit. The final inventory is evidence for command ownership, not permission to refactor unrelated production code.
