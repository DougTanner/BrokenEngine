<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:59:53.410Z","dependsOn":[]} -->
# Give HeightLerpWrapperQuartet a Resolve Overload

## Context

`HeightLerpWrapperQuartet` (`Engine/Source/Ui/HeightLerpWrapperQuartet.h:8-14`)
is four `Wrapper` members — `startHeight`, `endHeight`, `low`, `high` — and
nothing else. The same header declares
`LerpAtHeight(fFraming, fStartHeight, fEndHeight, fLow, fHigh)` (`:16-21`).
Every consumer of a quartet resolves it by expanding the four members into
that call by hand, in the same order. There are six such sites:

- `Engine/Source/Audio/StaticVoices.cpp:595`, `:596`, `:597`, `:598`
  (`gListenerDistanceStart`, `gListenerDistanceEnd`, `gListenerCurve`,
  `gListenerAudibleFloor`);
- `Engine/Source/Graphics/Render/LightingUniforms.cpp:197`
  (`gSpreadDistanceEnd`);
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp:549`
  (`gTerrainDetailNormalsMultiplier`).

The five-float form has four other callers that pass no quartet
(`WaterUniforms.cpp:64`, `LightingUniforms.cpp:238-240`), so it stays. No
caller exists in BrokenEngineSandbox or ModelViewer.
`Engine/Source/Ui/AGENTS.md` `## Shared Types` describes the groups as
resolving camera-height-conditioned values, yet the group type has no resolve
operation, so each consumer must know the member names and their order.

Originating gap: a deep analysis of `Engine/Source/Ui` — an
architecture-review finding, independently verified. Unmet criterion: a quartet
consumer does not need the four member names and their order.

## Design

The author's recommendation: add one overload of `LerpAtHeight` in
`HeightLerpWrapperQuartet.h` taking the framing and a
`const HeightLerpWrapperQuartet&`, which returns the five-float form called
with the quartet's four `mfCurrent` values, and replace the six expansions
with it. Rationale: six identical current expansions meet the root "helpers
only for current duplication" test, and every quartet consumer wants exactly
this resolve. The overload is a plain `inline` function.

Each site computes the same value from the same arguments as before.

## Critical files

- `Engine/Source/Ui/HeightLerpWrapperQuartet.h`
- `Engine/Source/Audio/StaticVoices.cpp`
- `Engine/Source/Graphics/Render/LightingUniforms.cpp`
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`

## In scope

- One new `LerpAtHeight` overload in `HeightLerpWrapperQuartet.h`.
- The six call sites listed in `## Context`.

## Out of scope

- The five-float `LerpAtHeight` and its four non-quartet callers.
- The quartet's members, the six quartet objects, and their Tweaks sliders.
- A member function on the struct, or any other restructuring of it.

## Acceptance criteria

- The diff shows the added overload and six call sites using it, each passing
  the same framing expression and the same quartet as before.
- The BrokenEngineSandbox client and server and ModelViewer build.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- Change Workflow tier: Tier 2 (the author's classification). Trigger: a new
  public function in a shared engine header, which Tier 1's "no public
  signature" excludes. The change is behavior-preserving; the Audio and Render
  call sites are consumers of the UI-owned helper, not an integration between
  those subsystems. All six sites are client presentation values outside the
  CRC, so there is no determinism, wire, serialization, replay, threading,
  allocation, or shader exposure.
- Low priority: a readability and DRY change with no behavior change.
