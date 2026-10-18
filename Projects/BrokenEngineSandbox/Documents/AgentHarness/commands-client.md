# BrokenEngineSandbox Client Commands

[Back to AgentHarness hub](../AgentHarness.md)

### Client commands

The engine-owned client commands are in the
[command reference](../../../../.agents/skills/agent-harness/references/command-reference.md#client-commands).
BrokenEngineSandbox takes over one of them, adding the parameters and fields
below.

- `describe_scene`: takes over the engine
  [`describe_scene`](../../../../.agents/skills/agent-harness/references/command-reference.md#client-commands),
  adding the parameters
  `{"includeUnits"?:true,"maxUnits"?:200,"unitTypes"?:["omni","spaceship","blaster"]}`.
  `unitTypes` is an array of those three strings; absent means every type, an
  empty array emits no unit rows, and any other value or unknown string is a
  parameter error. It applies only when `includeUnits` is true. A negative
  `maxUnits` is a parameter error. Beside the engine fields it returns
  `fleets[{index,focused,members?}]`,
  `units[{type,globalId?,coord,local,screen,armor?,shield?,health?,alignment,flags?,windTrailIntensity?,windTrailWidth?,windTrailLengthMultiplier?}]`,
  ring-wide `counts{omnis,spaceships,missiles,blasters}` summed over every
  subscribed cell that has a snapshot, and `truncated`. Each unit's `local` is
  in its own `coord` cell, compared with the camera as the engine entry's
  `camera.basisCoord` rule states. Blaster rows carry the three `windTrail*`
  fields and no `flags`. Unit positions come from the committed snapshot and
  trail rendered pixels. `units` are visible-area-filtered while counts span
  subscribed cells that have a snapshot; edge projection may lie slightly
  outside the framebuffer. Only omnis have global IDs, and only the focused
  fleet lists members. Each cell emits its blaster rows after its omni and
  spaceship rows, while the `maxUnits` cap is shared across all subscribed
  cells.
