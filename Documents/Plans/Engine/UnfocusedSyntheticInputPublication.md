<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:06.134Z","dependsOn":[]} -->
# Publish cleared synthetic input after unfocused scripts

## Context

The retained survivor `CAI/shard-0033/001` identifies a client input snapshot
that remains stale after an unfocused agent script completes. Agent clients are
started minimized and without focus by `Engine/Source/Main.cpp:326-349`.
`AgentInput::Finish` clears synthetic buttons and position at
`Engine/Source/Agent/AgentInput.cpp:75-92`, before the next
`RawInputManager::Update`. The unfocused early return at
`Engine/Source/Input/RawInputManager.cpp:141-144` then skips both the hardware
copy and overlay, leaving the previously published synthetic state in
`mRawInput`; `Input::BeginPoll` consumes that state for button and mode policy.

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0033.md:46`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:885`. Frozen/live target
identity matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`, and no
source was changed during routing. The shard distinguishes this transition
from the intended steady-state unfocused freeze and from the separate
agent-wheel/timeout findings.

## Design

The author's recommendation is to carry a one-shot cleared-publication signal
from script completion into `RawInputManager::Update`, or equivalently make
the manager publish the unfocused baseline once when the active-script state
changes from active to inactive. That publication must clear synthetic buttons
and position while preserving the persistent scroll lifetime contract. After
the cleanup frame, retain the existing unfocused freeze and focused hardware
publication behavior.

## Critical files

- `Engine/Source/Agent/AgentInput.cpp:75-92,369-410` and
  `Engine/Source/Agent/AgentInput.h:75-107,137-141` — script completion and
  synthetic state.
- `Engine/Source/Input/RawInputManager.cpp:135-181,228-237` — unfocused
  publication and overlay.
- `Engine/Source/Input/Input.cpp:27-50` — consumers of the stale snapshot.
- `Engine/Source/Input/AGENTS.md` and `Engine/Source/Agent/AGENTS.md` —
  unfocused script and two-sink contracts.

## In scope

- The transition from an active synthetic script to the next unfocused
  `RawInput` publication.
- Clearing stale synthetic mouse button/position state while preserving the
  persistent scroll accumulator.
- Existing focused publication and steady-state unfocused freeze behavior.

## Out of scope

- Physical-input suppression policy, ImGui mouse-position pinning, or command
  parsing.
- Camera wheel arithmetic and ImGui scroll ownership.
- A redesign of the RawInput snapshot or game-specific bindings.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: this is scoped client input runtime
behavior and remains outside deterministic Frame/PostRender CRC, wire state,
and serialization.

Preserve these invariants:

- A finished synthetic script is reflected in the next published game-binding
  snapshot even without focus.
- A released synthetic button cannot remain held, and a transient synthetic
  position does not survive into the idle unfocused baseline.
- The lifetime scroll accumulator remains present on every publish and focused
  input behavior is unchanged.

## Acceptance criteria

- On a minimized/no-focus client, a `mouse down` script publishes its press,
  then the next snapshot clears the button after completion.
- A coordinate-bearing move or wheel script restores the documented unfocused
  baseline after completion, while subsequent idle polls remain frozen.
- Focused click/key/mouse scripts retain their existing edge and release
  behavior.
- Client `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0033/001`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:885`. No source fix or build
was performed during routing.
