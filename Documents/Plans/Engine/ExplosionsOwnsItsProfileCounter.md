<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T14:22:50.529Z","dependsOn":[]} -->
# Let Explosions count its own rows for the profile counter

## Context

Found by the architecture review phase of a deep analysis of
`Engine/Source/Frame/Collections` (2026-11-14). Line numbers are from commit
`ae681f03` plus the change this Plan landed with; locate by symbol if they
moved. `C/` abbreviates `Engine/Source/Frame/Collections/`.

`ExplosionsInterpolate::siTotalCount` (`C/Explosions/Explosions.h:112`) is a
public `static inline` that no file in the Explosions directory writes. It is
reset, accumulated, and published from
`Engine/Source/Graphics/Render/MainUniforms.cpp`: `:220` (reset), `:274`
(add the cell's `explosions.iCount`), `:229` (publish to
`kCpuCounterExplosions`).

The same file already fans out `ForEachBeginRender`,
`ForEachInterpolateRender`, and `ForEachEndRender` over `InterpolateTypes` at
`:221`, `:275`, `:231`, and those skip a type with no hook
(`Engine/Source/Frame/FrameUtils.h:175-220`). Explosions is in that tuple
(`Engine/Source/Frame/FrameBase.h:142`) and declares no render hook.
`C/Pushers/Pushers.cpp:53-71` does the identical count-only job through the
three hooks with a file-static counter.

Unmet criterion: a collection's profile counter is reset, accumulated, and
published by that collection's own render hooks
(`C/AGENTS.md` `## Runtime Rules`: "Rendering may publish counters only where
that collection actually owns them").

## Design

Recommended: add count-only `BeginRender`, `Render`, and `EndRender` to
`ExplosionsInterpolate`, mirroring `Pushers.cpp:53-71` with a file-static
counter. Delete the three statements in `MainUniforms.cpp` (and the
`if constexpr` block that is then empty) and delete
`ExplosionsInterpolate::siTotalCount`.

The reset, accumulate, and publish points sit beside the fan-out calls on both
the normal and the no-camera path (`MainUniforms.cpp:247-248,262`), so the
hooks produce the same values on every path.

The change adds more lines than it removes; its value is ownership and
keeping the sibling pattern parallel. A profile counter only: no CRC exposure.

## Critical files

- `Engine/Source/Frame/Collections/Explosions/Explosions.h`
- `Engine/Source/Frame/Collections/Explosions/Explosions.cpp` (or the
  Explosions source file the executing session finds the closest match for
  render hooks)
- `Engine/Source/Graphics/Render/MainUniforms.cpp`

## In scope

- Three count-only render hooks on `ExplosionsInterpolate`.
- `ExplosionsInterpolate::siTotalCount`.
- The three `siTotalCount` statements in `MainUniforms.cpp`.

## Out of scope

- The other collections' counters and hooks.
- The render fan-out helpers in `FrameUtils.h`.
- `kCpuCounterExplosions` and the profile tables.

## Risk tier

Tier 2 — scoped behavior. Trigger: a public static member is removed and
three public hooks are added, which Tier 1 excludes. No determinism/CRC,
serialization, wire, or threading exposure.

## Acceptance criteria

- `MainUniforms.cpp` names no Explosions symbol for counting.
- `/compile` builds the BrokenEngineSandbox client and server.
- Through `/agent-harness`, the Explosions profile counter reads non-zero
  while explosions are alive and zero when none are. The supplied evidence
  records no harness setup.
- The private game's C++ source was not available to the authoring session.
  The executing session confirms no private-game use of
  `ExplosionsInterpolate::siTotalCount` by building the private game where its
  source is available, and reports it as unconfirmed otherwise.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
