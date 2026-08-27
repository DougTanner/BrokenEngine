<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:01.273Z","dependsOn":[]} -->
# Bound synthetic mouse-wheel arithmetic

## Context

The retained survivor `CAI/shard-0010/003` identifies an unchecked fixed-width
wheel calculation. `CommandMouse` narrows the external `notches` integer to
`int32_t` at `Engine/Source/Agent/AgentCommandsClientGeneric.cpp:975-978`.
`AgentScript::iWheelNotches` and
`AgentInput::miSyntheticScrollAccumulator` are fixed-width integers
(`Engine/Source/Agent/AgentInput.h:62,141`), and
`AgentInput::AdvanceFrame` multiplies the notch count by the wheel delta and
adds it at `AgentInput.cpp:379-394`. `RawInputManager::Update` publishes that
lifetime value at `Engine/Source/Input/RawInputManager.cpp:171-181`, where
`Input::BeginPoll` diffs it for camera zoom.

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0010.md:69`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:532`. Frozen/live source
identity matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`; this
routing session has not changed source. A syntactically valid request such as
20,000,000 notches reaches the multiply before any range or product check.

## Design

The author's recommendation is to establish one checked arithmetic boundary
before publishing synthetic wheel state. Parse into a wide temporary, reject a
notch count that cannot fit the fixed per-event product, and perform a checked
wide add against the current lifetime accumulator before committing the new
value. On failure, return the existing command-error envelope without changing
the accumulator. Keep the persistent lifetime accumulator and the normal
one-notch camera diff unchanged.

## Critical files

- `Engine/Source/Agent/AgentCommandsClientGeneric.cpp:941-1002` — external
  mouse command parsing.
- `Engine/Source/Agent/AgentInput.cpp:369-394` — synthetic wheel update.
- `Engine/Source/Agent/AgentInput.h:59-64,137-141` — fixed-width script and
  accumulator state.
- `Engine/Source/Input/RawInputManager.cpp:171-181` and
  `Engine/Source/Input/Input.cpp:111-118` — publication and consumer diff.
- `Engine/Source/Agent/AGENTS.md` and `Engine/Source/Input/AGENTS.md` — agent
  and lifetime-wheel contracts.

## In scope

- Validation and checked arithmetic for `mouse` wheel `notches` before the
  synthetic lifetime value is mutated.
- Error behavior and accumulator preservation for an unrepresentable event.
- Existing publication and camera-consumer semantics for representable values.

## Out of scope

- Camera zoom scaling, wheel direction, ImGui scroll ownership, or hardware
  mouse behavior.
- Changes to the agent frame format or unrelated key/mouse command fields.
- A wider persistent accumulator without a demonstrated need and compatibility
  decision.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Trigger: an opaque
agent command enters fixed-width main-loop input state and can affect client
behavior through unchecked arithmetic.

Tier rationale: the fix is a pre-specified checked-arithmetic guard at one
command-parsing boundary that rejects through the existing error envelope; the
accumulator width, publication path, and camera consumer behavior for
representable values are unchanged.

Preserve these invariants:

- Every accepted wheel event has a representable product and lifetime sum.
- Rejected values leave the accumulator and the next `RawInput` snapshot
  unchanged.
- Representable wheel commands still publish one persistent lifetime update
  and produce the existing camera delta.

## Acceptance criteria

- An oversized or cumulative-overflowing wheel request returns a validation
  failure before changing synthetic scroll state.
- Boundary values that fit the product and accumulator are accepted without
  overflow, and one ordinary notch still produces the existing delta.
- Repeated accepted wheel commands cannot wrap the published lifetime total or
  cause implementation-dependent camera input.
- Client `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0010/003`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:532`. No source fix or build
was performed during routing.
