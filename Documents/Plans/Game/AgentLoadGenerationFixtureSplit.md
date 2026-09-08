<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-08T03:20:32.877Z","dependsOn":[]} -->
# Split the full-state fixture from client agent commands

## Context

`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp` measures
10,932 bt-token-v1 tokens against the 10,000-token C++ file threshold. Its
baseline full-state fixture group at lines 15-349 measures 3,563 tokens and is
a cohesive extraction boundary: two state-building helpers, the matching-tick
exercise, and `CommandClientFullStateFixture` with its existing arm, inspect,
clear, gap, and matching-tick actions.

The fixture behavior is complete and intentionally narrow. This follow-up owns
only its source-file placement; changing its behavior or expanding its
test surface would defeat the reason to defer the split from the active change.

## Design

Author's recommendation: move the complete full-state fixture group at lines
15-349 into a client-only `AgentCommandsClientFullStateFixture.cpp`. Declare
`CommandClientFullStateFixture` in a narrow
`AgentCommandsClientFullStateFixture.h` included by the existing dispatch
translation unit and the extracted implementation.

The extraction removes 3,563 tokens from the 10,932-token source, leaving about
7,369 tokens before the small include and declaration boundary overhead. The
new implementation starts from 3,563 tokens before its guards, includes, and
namespace wrapper. Both sides therefore retain at least 2,500 tokens of margin
below the threshold. Keep the new translation unit out of the shared game PCH
and add it only to the client Visual Studio project and filter.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp:15-349` and its client command dispatch — full-state fixture group to extract and the existing call site to retain.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClientFullStateFixture.h` — narrow client-only declaration for the extracted command handler.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClientFullStateFixture.cpp` — extracted full-state fixture state, helpers, exercises, and action handling.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj` and `.filters` — client-only source/header membership.

## In scope

- Move the complete baseline `client_full_state_fixture` state queries,
  matching-tick and gap exercises, result construction, and action handler into
  the dedicated client-only translation unit.
- Keep the existing action schemas, validation, state transitions, result
  fields, and failure text unchanged across the extraction.
- Add deterministic client project/filter membership for the new files.
- Keep both resulting implementation files within the 10,000-token C++ file
  threshold.

## Out of scope

- Changes to load generation, packet layouts, subscription classification,
  ACK/resend behavior, or reset ordering.
- New fixture actions, packet injection, configurable delays, multi-load
  choreography, result fields, or command parameters.
- Refactoring other client agent fixtures or changing server project membership.
- Unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 1. Trigger: a local behavior-preserving
translation-unit split plus Visual Studio project/filter membership, with no
public signature or runtime invariant change.

- Every `client_full_state_fixture` action keeps its exact schema, state
  transitions, result fields, and failure messages.
- The extracted code remains client-only and uses the same production full-state
  inspection and reconciliation paths.
- No wire, save, replay, deterministic Frame, CRC, threading, or allocation
  contract changes.

## Acceptance criteria

- The repository's deterministic C++ size metric reports
  `AgentCommandsClient.cpp` and the new full-state fixture implementation at or
  below 10,000 bt-token-v1 tokens each.
- `/update-vcxproj` validates that the new implementation and header belong only
  to `BrokenEngineSandbox.vcxproj` and its filter tree.
- `/compile` builds the BrokenEngineSandbox client `Debug|x64` successfully.
- `/agent-harness` exercises the existing `arm_stall`, `inspect`, `clear`,
  `exercise_gap`, and `exercise_matching_tick` paths with their documented
  results unchanged.

## Coordination

No directional prerequisite or reciprocal Plan coordination is required. The
split preserves the baseline full-state fixture contract already present when
this Plan is selected.

## Notes

This is an ownership-only reduction. Update only source-ownership references
that become stale when the full-state fixture moves; keep command schemas and
architecture unchanged.
