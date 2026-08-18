<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T18:40:24.443Z","dependsOn":[]} -->
# D11: Establish the build-switch contract

## Context

This is the executable handoff for D11 from the removed settings/build
investigation. The repository user explicitly permits unresolved choices and
multiple options in this Plan; the implementation session must settle the
remaining validation and reference-source decisions from the evidence instead
of treating the preferred shape as already approved.

At the current source baseline, `Projects/BrokenEngineSandbox/Source/Pch.h:5-96`
declares 41 named compile-time constants, including the derived
`kbFramebufferClearColor`; `keNetworkSimulation` at line 108 is the 42nd named
constant. Of the 42 named constants, 41 are independently configured because
`kbFramebufferClearColor` is derived. The block includes the general switches,
client/server `kbSingleInstance`, the Debug/Profile/Release policy tables, and
the log-level constants through `keLogLevelInput`.
The PCH's load-bearing include-order note and aggregation are at lines 98-106:
`ExternalHeaders.h` is first, `Common.h` precedes `Shaders/ShaderLayouts.h`,
and `ShaderLayouts.h` precedes `Engine.h`, with the game Frame/network headers
around that chain. `keNetworkSimulation` remains at line 108, after `Engine.h`,
because its type is `engine::NetworkSimulationLevel`.

The census found that 34 of the 41 independently configured values are read by
Engine, Common, or Tools rather than only by the game. Current witnesses include
`Common/Log/Log.h:204-215` (the Common log table requires the PCH's typed log
constants), `Engine/Source/Main.cpp:162` (`kbAgent`),
`Engine/Source/Graphics/Managers/InstanceManager.cpp:203` (`kbRenderDoc`),
`Engine/Source/Graphics/Objects/PipelineCreator.cpp:411`
(`kbVulkanWireframe`), and the network simulation users in
`Engine/Source/Network/Server/Server.cpp:120-177`,
`Engine/Source/Network/Client/Client.cpp:179-230`, and
`Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.cpp:167-394`.
The switches therefore form a real per-game compile contract in the current
sandbox: the current values and configuration blocks express game/build policy
rather than generic engine defaults. This census is evidence the closed-set
option must answer, not a decision against it; if closed set is selected, the
implementation must explicitly identify which values are fixed engine policy
and which remain game extensions, while preserving all baseline values.

## Design

### Preferred shape and open decisions

The researched recommendation (2026-08-11, not user-approved) is:

1. The game owns a `BuildSwitches.h`, split verbatim from the current
   `Pch.h:5-96` declarations. The load-bearing include-order note below that
   block remains untouched.
2. An engine-side contract header validates the declarations in two deliberate
   phases: a pre-`Common.h` phase for switches needed by Common and earlier
   includes, and a post-`Engine.h` phase for `keNetworkSimulation` after the
   `engine::NetworkSimulationLevel` type is available.
3. A new game uses the sandbox's actual compiled `BuildSwitches.h` as its
   reference source instead of maintaining a second duplicate. Ship a separate
   copy only when standalone distribution requires it.

The implementation must still resolve these points explicitly:

- Whether the contract is required game-supplied values plus validation (the
  preferred but not approved shape), or a closed engine set with a documented
  game extension. If the closed-set option is selected, the implementation must
  explicitly classify which current values/configuration blocks are fixed engine
  policy and which remain game extensions, while preserving all baseline values.
- Exact placement and interface of the two validation phases while preserving
  the PCH's include order and useful diagnostics when a declaration is missing.
  A missing identifier can fail before an assertion and cascade; do not claim
  that an assertion alone provides clean diagnostics.
- Whether the compiled sandbox header is the sole reference or a separately
  shipped copy is required for standalone distribution.

The earlier `#ifndef`-overridable-defaults alternative is rejected directionally:
it turns typed `LogLevel` and `NetworkSimulationLevel` values into raw macro
configuration and introduces macro-shaped policy into an `inline constexpr`
codebase. Reopen it only with explicit user approval and a demonstrated typed
equivalent; do not implement it implicitly.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Pch.h` — current switch declarations,
  log-level range, load-bearing include order, and post-`Engine.h` network switch.
- `Common/Log/Log.h` and `Common/Log/LogTypes.h` — typed log contract and
  category-table consumers.
- `Engine/Source/Network/NetworkSimulation.h` — definition of
  `engine::NetworkSimulationLevel` needed before `keNetworkSimulation`.
- `Engine/Source/Network/Server/Server.cpp`,
  `Engine/Source/Network/Client/Client.cpp`, and
  `Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.cpp` — network
  simulation compile consumers.
- `Engine/Source/Main.cpp`,
  `Engine/Source/Graphics/Managers/InstanceManager.cpp`, and
  `Engine/Source/Graphics/Objects/PipelineCreator.cpp` — representative
  engine consumers of game policy switches.
- The build-switch declaration/contract headers and source selected by the
  resolved shape; keep their exact paths and target affinity explicit before
  implementation. Existing sandbox source and targets are the reference
  inspection surface; no second-game fixture is authorized.
- The client/server Visual Studio project and filter files if the selected
  header/contract needs explicit project membership.

## In scope

- Resolve and record the required-header vs closed-set choice, the two-phase
  validation placement, and the reference-source policy before implementation.
- Extract or otherwise reorganize the current `Pch.h:5-96` declarations into
  the selected build-switch ownership boundary without changing any value,
  per-configuration policy, type, or derived relationship.
- Keep `keNetworkSimulation` in a post-`Engine.h` declaration or equivalent
  post-type validation site; do not move it before its engine-defined type.
- Centralize declaration/type/value-shape checks in the selected engine
  contract, with pre-Common and post-Engine validation phases as applicable.
- Preserve the explanatory include-order comment and exact required order for
  external headers, `Common.h`, `ShaderLayouts.h`, `Engine.h`, and the game
  aggregation headers.
- Verify all current Engine/Common/Tools consumers, client/server target
  affinities, and project/filter membership for any new or removed files.

## Out of scope

- Changing a switch's value, per-configuration table, type, or derived value;
  changing behavior gated by a switch; or changing `keNetworkSimulation`'s
  semantics.
- `#ifndef` macro overrides, untyped defaults, or compatibility shims without
  explicit user approval.
- Settings-file ownership (tracked separately by `EngineClientSettingsOwnership.md`),
  `Game.h` identity, localization, Frame state, CRC/replay behavior, network
  packet formats, shader layouts, `.pack` data, or unrelated PCH cleanup.
- Removing the PCH or changing the repository-wide PCH aggregation model.
- Unit tests, a new test framework, and speculative support for future switch
  names or future standalone packaging.

## Risk tier and invariants

Tier 3 applies: this changes the cross-subsystem compile contract used by
Common, Engine, Tools, and game code, preserves load-bearing PCH include order,
and can affect deterministic/replay/network/render compile paths through policy
switches. It also reaches build/project membership. No wire or CRC format change
is intended, and all current values and type choices remain fixed.

Invariants:

1. All 42 named constants remain available to the same consumers; the 41
   independently configured values retain their current values in every
   Debug/Profile/Release and client/server branch.
2. `kbFramebufferClearColor` remains derived from `kbVulkanWireframe`; it is
   not independently configurable.
3. `keLogLevelInput` remains included in the extracted log-level range, and
   `keLogLevelNavData` remains available to Common rather than becoming a
   game-only declaration.
4. `keNetworkSimulation` remains typed as
   `engine::NetworkSimulationLevel` and is declared/validated only after the
   type is available from `Engine.h`.
5. External headers precede the shader layout header, and
   `Shaders/ShaderLayouts.h` precedes `Engine.h`; the load-bearing comment
   remains accurate.
6. The contract checks the declarations needed before `Common.h` separately
   from the post-`Engine.h` network simulation declaration. Missing identifiers
   may still cascade; the implementation must not hide that limitation behind
   a false diagnostic guarantee.
7. The selected reference-source policy is explicit and is validated against
   existing sandbox source and targets; it does not silently maintain a
   duplicate or an untyped macro override.

## Coordination

This Plan is independent of D6 and has no settings or localization dependency.
Any implementation that adds a new header/source must coordinate client/server
project and filter membership and must preserve the PCH include-order contract.
The required-header, two-phase-validation, and reference-source decisions are
mandatory handoff points; neither the earlier closed-set candidate nor the
rejected macro alternative may be silently presented as settled.

## Acceptance criteria

1. **Contract choice is explicit.** The implementation record names the selected
   game-header/closed-set shape, exact pre-Common and post-Engine validation
   placement, and compiled-vs-shipped reference policy. If closed-set is
   selected, it also identifies which current values/configuration blocks are
   fixed engine policy versus game extensions while preserving all baseline
   values; the rejected macro alternative is ruled out or explicitly
   user-approved.
2. **Switch census is preserved.** Inspect the resulting headers and compile
   client and server Debug/Profile/Release targets. Expected: all 42 names are
   available, all 41 independent values match the baseline, the derived color
   follows wireframe, and `keLogLevelInput` is present.
3. **Common phase is valid.** Compile a target that reaches `Common/Log/Log.h`
   through the new aggregation. Expected: typed log levels and category table
   remain valid, including the Common-consumed navigation level.
4. **Engine phase is valid.** Compile the engine network and profile consumers.
   Expected: `keNetworkSimulation` remains a typed post-`Engine.h` value and
   all current simulation gates compile with the same disabled default.
5. **Include order is preserved.** Source review of `Pch.h` and the contract
   shows the existing load-bearing comment and external → shader layout →
   Engine order unchanged, with no shader layout include moved behind Engine.
6. **Reference policy works.** Perform a reproducible source/compile inspection
   of the selected reference contract and the existing sandbox client/server
   targets. Expected: the selected policy is usable by those targets without a
   silently maintained duplicate or an untyped macro override; no second-game
   fixture is required or authorized.
7. **No unrelated contract changed.** Source review confirms no settings,
   persisted file, Frame/PostRender CRC, replay, packet, `.pack`, shader-layout,
   runtime switch behavior, or unrelated PCH aggregation change.

## Notes

The census is intentionally a contract census, not a list of every call site:
34 of the 41 independently configured constants are consumed outside the game.
The current sandbox's `kbAgent`, `kbRenderDoc`, and configuration blocks
exhibit per-game policy. The closed-set option must prove and classify any
values or configuration blocks that are truly fixed engine policy versus game
extensions, while preserving all baseline values. The required game-header
shape remains a preferred recommendation, not an approved outcome.
