# BrokenEngineSandbox Client Commands

[Back to AgentHarness hub](../AgentHarness.md)

### Client commands

The engine-owned client commands are in the
[command reference](../../../../.agents/skills/agent-harness/references/command-reference.md#client-commands).
BrokenEngineSandbox takes over one of them, adding the parameters and fields
below, and adds `reveal_ui`.

- `describe_scene`: takes over the engine
  [`describe_scene`](../../../../.agents/skills/agent-harness/references/command-reference.md#client-commands),
  adding the parameters
  `{"includeUnits"?:true,"maxUnits"?:200,"unitTypes"?:["fighter","omni","battleship","blaster"]}`.
  `unitTypes` is an array of those four strings; absent means every type, an
  empty array emits no unit rows, and any other value or unknown string is a
  parameter error. It applies only when `includeUnits` is true. A negative
  `maxUnits` is a parameter error. Beside the engine fields it returns
  `fleets[{index,focused,members?[{globalId,type,squadron,alive}]}]`,
  `units[{type,globalId?,coord,local,screen,armor?,shield?,health?,alignment,flags?,windTrailIntensity?,windTrailWidth?,windTrailLengthMultiplier?}]`,
  ring-wide `counts{fighters,omnis,battleships,missiles,blasters}` summed over
  every subscribed cell that has a snapshot, and `truncated`. Each unit's
  `local` is in its own `coord` cell, compared with the camera as the engine
  entry's `camera.basisCoord` rule states. Blaster rows carry the three
  `windTrail*` fields and no `flags`. Unit positions come from the committed
  snapshot and trail rendered pixels. `units` are visible-area-filtered while
  counts span subscribed cells that have a snapshot; edge projection may lie
  slightly outside the framebuffer. Every ship row carries `globalId` (0 for a
  ship that has none), and only the focused fleet lists members, in add order: a
  member's `type` is a `unitTypes` string and its `squadron` is the stored
  per-type number, one less than the number in the HUD's squadron header. Each
  cell emits its fighter, omni, and battleship rows, then its blaster rows,
  while the `maxUnits` cap is shared across all subscribed cells.
- `reveal_ui`: `{"window":"FleetPanel","reveal":true}` asks the HUD to slide
  the fleet panel open, the way its own auto-unhide does, without moving the
  mouse, so its member rows can be screenshotted; `reveal:false` withdraws the
  request. `FleetPanel` is the only `window`. The `FocusedOwnedEntityPanel`
  window opens with it only while there is a focused owned entity. An open map
  still slides both panels off; a request made then takes effect when the map
  closes. A request made while a menu or Tweaks shows takes effect when the HUD
  renders again. The request survives `reset`, server load, and HUD rebuild,
  and lasts until `reveal:false` or client exit. It fails only for a missing or
  other `window` or a missing or non-Boolean `reveal`. Success returns no
  fields and acknowledges only the request: confirm the panel is shown with a
  later `describe_ui` (its rows `visible:true`) or a screenshot, because it
  slides in over several frames.
