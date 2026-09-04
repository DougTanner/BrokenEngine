<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-03T13:19:22.204Z","dependsOn":[]} -->
# Fix: /agent-harness describe_ui — control values are not readable as text, forcing screenshots

## Context

The `/agent-harness` verification guidance at
`.agents/skills/agent-harness/references/worker.md:568-570` tells a run to close a
criterion with `describe_ui`, scene/server queries, and `get_logs` rather than
pixels, and to "reach for a capture only when the criterion is genuinely about
what was rendered". For any criterion about a UI control's current value that
guidance cannot be followed, because the describe result carries no value.

Observed symptom in the session recorded below: a harness run had to confirm that
four slider drives had landed on whole numbers. `describe_ui` returned only
label/window/rect/disabled/checked/inputable/hovered/visible per item, so the run
instead took native-resolution screenshots, wrote an untracked ad-hoc crop script
(`Temp/Crop-SliderRow.ps1`, since discarded — referenced only as the workaround
that was performed), and loaded five images into the worker's context to read four
numbers off the rendered slider rows.

Current state in the tree:

- `BuildDescribeUi` (`Engine/Source/Agent/AgentCommandsClientGeneric.cpp:667-711`)
  emits `uiState`, `tweaksVisible`, `gameFlags`, `framebuffer`, `mouse`,
  `windows`, and `items`; each item row is built at lines 695-705 with `label`,
  `window`, `rect`, `disabled`, `checked`, `inputable`, `hovered`, `visible` and
  no value.
- `engine::AgentUiItem` (`Engine/Source/Agent/AgentUiRegistry.h:10-18`) stores
  `uiId`, `pcLabel[64]`, `pcWindow[32]`, `f4Rect`, `iStatusFlags`, `bDisabled`.
  It is filled by the two imgui test-engine hooks
  (`AgentUiRegistry.h:50-51`), neither of which receives a widget value.
- `checked` at `AgentCommandsClientGeneric.cpp:701` is derived from
  `ImGuiItemStatusFlags_Checked`, so a checkbox already reports its state; a
  slider, drag, plus/minus, or radio row reports nothing.
- The harness command reference states the gap outright:
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md:17`
  ends with "Values are not exposed."
- The player-facing controls whose values are missing all run through the engine
  wrapper helpers `WrapperToggle`, `WrapperSlider`, and `WrapperPlusMinus`
  (`Engine/Source/Ui/MenuUtils.cpp:46-66` and `106-127`), each of which already
  holds the live value it is about to draw.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the session
`/next-plan-review` must reach — while the `Landing ref` line names a ref whose
tree actually contains this Plan:

- Client: claude
- Conversation session ID: 8c5796af-a367-402c-94bb-f2ab0bb0f407
- Worktree/branch UUID: d2db039b-b414-4862-b6b6-4f2df98bee44
- Session branch: claude/d2db039b-b414-4862-b6b6-4f2df98bee44
- Worktree: .claude\worktrees\BrokenEngine\d2db039b-b414-4862-b6b6-4f2df98bee44
- Landing ref: claude/d2db039b-b414-4862-b6b6-4f2df98bee44
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/DescribeUiControlValues.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID above.

## Design

First root-cause the friction from the current tree and this Plan's `## Context`;
the cited lines above are expected to be sufficient. Only when the transcript is
genuinely needed, in a new session run `/next-plan-review <landing ref>` in
bounded friction mode, supplying the recorded client and conversation session ID.

Recommended route, because it reuses the snapshot the describe result already
reads and keeps the change inside one subsystem: carry a short formatted value
text on the completed-frame item record and emit it from the describe builder.

- Recommended: add a small fixed-size value buffer to `engine::AgentUiItem`
  (`AgentUiRegistry.h:10-18`) plus one client-only registry entry point that
  records a formatted value for the item just submitted, call that entry point
  from the engine wrapper helpers in `MenuUtils.cpp` (`WrapperSlider`,
  `WrapperToggle`, `WrapperPlusMinus`, and `RadioRow` if its selected option is
  wanted), and emit the recorded text as one extra optional item field in
  `BuildDescribeUi` (`AgentCommandsClientGeneric.cpp:695-705`) — omitted for
  items that recorded none, so existing consumers see an unchanged shape.
  Rationale: the value is already in hand at draw time inside those helpers, the
  registry is the existing publish path `describe_ui` reads, and no new command
  or transport surface is added.
- Alternative the fix session may prefer if root-causing shows the wrapper
  helpers cannot reach the client-only registry cleanly: a separate value-query
  command instead of an extended describe result. This costs a new command and a
  second round trip per check, so prefer it only with a concrete reason.

Two tree constraints bind either route. `MenuUtils.cpp`'s wrapper helpers compile
into both the client and server builds while `AgentUiRegistry` is whole-file
`BT_CLIENT` (`AgentUiRegistry.h:3` and `93`), so any call from those helpers into
the registry is `BT_CLIENT`-guarded at the narrowest practical scope. The registry
and synthetic-input paths must stay allocation-free in steady state
(`Engine/Source/Agent/AGENTS.md`, `## Constraints`), so the value text lives in a
fixed member buffer formatted in place, never in a heap string.

Then update the two documentation sites that currently describe the gap:
`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md:17`
(replace "Values are not exposed." with the field that now exists) and the
verification-evidence bullet at
`.agents/skills/agent-harness/references/worker.md:568-570` so it names reading a
control value as text instead of capturing pixels. If root-causing shows the fix
lies outside the `## In scope` boundary below, surface it for re-planning instead
of expanding scope.

## Critical files

- `Engine/Source/Agent/AgentUiRegistry.h`
- `Engine/Source/Agent/AgentUiRegistry.cpp`
- `Engine/Source/Agent/AgentCommandsClientGeneric.cpp`
- `Engine/Source/Ui/MenuUtils.cpp`
- `Engine/Source/Ui/MenuUtils.h`
- `Engine/Source/Agent/AGENTS.md`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md`
- `.agents/skills/agent-harness/references/worker.md`

## In scope

- Root-cause investigation as `## Design` states
- The value record on `engine::AgentUiItem` (`AgentUiRegistry.h:10-18`) and its
  fill/clear/publish handling in `AgentUiRegistry.cpp`, including one client-only
  entry point the wrapper helpers call
- The value-recording calls inside `WrapperSlider`, `WrapperToggle`,
  `WrapperPlusMinus`, and `RadioRow` in `Engine/Source/Ui/MenuUtils.cpp:46-127`,
  with their declarations in `MenuUtils.h` if a signature changes
- The item-row emission in `BuildDescribeUi`
  (`AgentCommandsClientGeneric.cpp:695-705`)
- Documentation for the new field: the `describe_ui` bullet at
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md:17`,
  the verification-evidence bullet at
  `.agents/skills/agent-harness/references/worker.md:568-570`, and the
  `describe_ui` sentence in `Engine/Source/Agent/AGENTS.md` if the contract it
  states changes

## Out of scope

- The landed change the session that observed this friction produced
- Any change to `click`, `hover`, `set_slider`, `key`, `mouse`, `describe_scene`,
  `query_profile`, or the agent command transport
- The game HUD's own controls and any game-side agent command file
- Screenshot, `dump_render_target`, and any image-cropping tooling; the workaround
  script is not to be tracked
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2 (scoped behavior of one subsystem — the client-only agent UI
snapshot and its describe result). It touches no determinism/CRC, wire/protocol,
serialization or `.pack` layout, save/replay compatibility, threading, or trust
boundary; escalate to Tier 3 if the chosen route reaches any of those or spans
independently owned subsystems. Invariants to hold: registry and synthetic-input
steady-state paths stay allocation-free; client implementations stay whole-file or
narrowest-scope `BT_CLIENT` and the server build still links; the registry's
double-buffered publish-after-`ImGui::Render()` ordering is unchanged, so readers
still see only the last completed frame; existing `describe_ui` fields keep their
current names and meanings. Never embed transcript paths or home paths.

## Acceptance criteria

- A live `/agent-harness` client run reads back a driven slider's value and a
  checkbox's state from the `describe_ui` result as text, with no screenshot and
  no image loaded into context
- Client and server both build clean
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md` no
  longer states that values are not exposed, and the
  `.agents/skills/agent-harness/references/worker.md` verification bullet matches
  the shipped field
- /validate-skill passes for any changed SKILL.md; plan validate exits 0

## Notes

Checkbox state is already reachable today through the `checked` field
(`AgentCommandsClientGeneric.cpp:701`); the missing values are the numeric and
selected-option controls. Include the checkbox in the value record only if it
falls out of the same mechanism at no extra cost — do not build a second path for
it.
