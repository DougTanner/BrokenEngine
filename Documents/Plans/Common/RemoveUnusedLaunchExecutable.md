<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-20T17:37:19.972Z","dependsOn":[]} -->
# Remove the unused common::LaunchExecutable helper

## Context
`common::LaunchExecutable()` is a fire-and-forget `CreateProcessW` wrapper that
starts an executable with no command-line arguments and immediately closes both
returned handles:

- `Common/WindowsUtils.h:51` — declaration, preceded by its own doc comment
  block starting at the `// Launches an external process without waiting for it
  to complete (fire-and-forget)` line.
- `Common/WindowsUtils.cpp:223` — definition.

Its only repository consumer was the auto-launch-server block of the old game
`MainMenuScreen`, which the standard-menu-screens-to-engine change deleted with
explicit user approval. A repository-wide search for `LaunchExecutable` now
returns only the declaration, the definition, and historical prose in
`Documents/Plans/Ui/StandardMenuScreensToEngine.md`; there is no call site left.

The user explicitly chose to record this removal as a follow-up Plan rather than
delete the helper inside that change, because deleting it was outside that
change's approved boundary.

## Design
Delete the declaration with its doc comment block and the definition. Nothing
else changes: no call sites exist, so no caller migration, no replacement
helper, and no signature change anywhere.

The two sibling helpers in the same file stay exactly as they are, because both
have live consumers:

- `RunExecutable()` — DataPacker runs `glslangValidator` and `ffmpeg` through it.
- `RunExecutableInNewConsole()` — the Gaea bake path needs real console handles.

Neither shares code with `LaunchExecutable()`, so removing it cannot affect
them.

## Critical files
- `Common/WindowsUtils.h` — declaration plus its doc comment block.
- `Common/WindowsUtils.cpp` — definition.

## In scope
- Delete the `LaunchExecutable` doc comment block and declaration from
  `Common/WindowsUtils.h`.
- Delete the `LaunchExecutable` function body from `Common/WindowsUtils.cpp`.

## Out of scope
- `RunExecutable` and `RunExecutableInNewConsole` — declarations, definitions,
  doc comments, and behavior all stay unchanged.
- Any other function in `Common/WindowsUtils.{h,cpp}`.
- Adding a replacement process-launch helper anywhere.
- Editing the historical `common::LaunchExecutable` mentions in
  `Documents/Plans/Ui/StandardMenuScreensToEngine.md`.

## Risk tier and invariants
Expected Change Workflow Tier 1: mechanical dead-code removal with no public
signature that any caller uses, no invariant exposure, and no determinism/CRC,
serialization, wire, replay, threading, allocation, shader, or build-coordination
surface. Escalate only if a call site is discovered during implementation, which
would mean the helper is not dead after all.

## Acceptance criteria
- A repository-wide search for `LaunchExecutable` finds no occurrence in any
  `.h` or `.cpp` file.
- `RunExecutable` and `RunExecutableInNewConsole` are byte-identical to their
  pre-change form.
- The client, server, and DataPacker projects compile.

## Notes
Recorded as an out-of-scope residual of the standard-menu-screens-to-engine
change, whose deletion of the game `MainMenuScreen` auto-launch-server block
removed the last caller.
