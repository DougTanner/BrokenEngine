<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:26.492Z","dependsOn":[]} -->
# Bound sparse Frames profile rendering

## Context

The retained survivor `CAI/shard-0038/001` identifies an unbounded diagnostic
formatter. `FormatFramesMap` expands min/max coordinates and visits every cell
in the dense rectangle, scanning `mActiveCoords` for each one at
`Engine/Source/Profile/ProfileScreens.cpp:246-283`. `FormatFramesScreen` runs it
on the selected profile text update (`ProfileScreens.cpp:395-409`), and
`ImGuiManager::UpdateTextArea` asserts the fixed 4,096-character limit at
`Engine/Source/Graphics/Managers/ImGuiManager.cpp:540-548`. A valid client
coordinate move can therefore hang on a huge gap or throw after generating a
too-large view.

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0038.md:50`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:975`. Frozen/live source
identity matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source
was changed during routing. `set_client_grid_coord` accepts finite coordinates
through ±1,000,000, so the sparse-grid span is a supported input state.

## Design

The author's recommendation is to render a bounded sparse listing of the
active-coordinate set rather than iterate the coordinate rectangle. Preserve
the client coordinate and each active coordinate as labelled entries, use the
existing fixed workbuffer/text-area path, and emit a bounded truncation marker
if the active set itself reaches the text capacity. Coordinate distance must
not affect iteration count or output size.

## Critical files

- `Engine/Source/Profile/ProfileScreens.cpp:246-283,395-409` — Frames map
  formatter and selected-screen caller.
- `Engine/Source/Graphics/Managers/ImGuiManager.h:36-45` and
  `ImGuiManager.cpp:540-548` — fixed text-area contract.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp:474-500,557-560`
  and `Projects/BrokenEngineSandbox/Source/Game.cpp:176-197` — supported
  coordinate/active-set path.
- `Engine/Source/Profile/AGENTS.md` — diagnostic overlay ownership and bounds.

## In scope

- `FormatFramesMap`'s coordinate traversal, lookup, and bounded text emission.
- The selected Frames-screen publication path needed to keep valid sparse state
  observable without assertion or unbounded work.
- Existing Frames tick/profile screens and text-area ownership.

## Out of scope

- Client coordinate range, subscription stickiness, active-set policy, or frame
  ring lifetime.
- Profile timer semantics, network history, and UI automation commands.
- Raising the fixed text-area capacity or changing sparse-grid simulation.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: this is scoped profile diagnostic
render behavior; it does not alter simulation, wire, serialization, or CRC
state.

Preserve these invariants:

- Valid active-coordinate input always produces bounded profile work and text.
- Coordinate span does not turn a sparse set into dense iteration or force a
  top-level exception.
- The Frames overlay still identifies the client cell and active coordinates,
  with explicit truncation when the fixed publication capacity is reached.

## Acceptance criteria

- A valid active set containing distant coordinates completes one Frames update
  without a long rectangle traversal or text-area assertion.
- The published text remains below `TextArea::kiMaxChars` and identifies all
  entries that fit, with a clear bounded indication for omitted entries.
- A local 3x3 active ring retains the current diagnostic information and layout
  meaning.
- Client and server `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0038/001`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:975`. No source fix or build
was performed during routing.
