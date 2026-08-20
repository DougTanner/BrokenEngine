# Engine UI - Shared Wrapper and Screen Infrastructure

Shared runtime settings, player-facing menu helpers, renderer quality levels, network-pending controls, and client-only curve/editor support. Engine Tweaks registration contracts live in TweaksScreen (`Screens/TweaksScreen/AGENTS.md`); game localization, quality-level persistence, and menu consumption live in game UI (`../../../Projects/BrokenEngineSandbox/Source/Ui/AGENTS.md`).

## Wrapper Contracts

- `Wrapper` stores float, bool, or discrete values with one-consumer change tracking. `Changed<T>()` advances the previous-value state, so exactly one consumer may poll a wrapper each frame.
- Mutate through `Set` or `Reset`; assignment is disabled. `Set` leaves the prior value intact so the next poll observes a change. `Reset` updates both values and is for initialization or capability clamping that must not trigger rebuilds.
- Discrete wrappers fall back to their first allowed value when persisted or capability-derived input is invalid. Float wrappers may snap to a configured grid.
- One-argument construction is Boolean-only. Non-Boolean numeric and enum constructors are deleted so a scalar cannot silently convert to the Boolean overload; floats use the ranged constructor and discrete values supply an allowed-value set.
- Wrapper bounds annotated with shader invariants are correctness constraints, not UI tuning.
- Settings that alter baked render-target dimensions must participate in `Graphics::Refresh`; prefer per-frame uniforms when recreation is unnecessary.
- Wrapper headers are intentionally consumed directly rather than aggregated into `Engine.h`, limiting recompilation from tuning edits.

## Menu and Panel Helpers

- Engine UI owns reusable player-facing menu layout and interaction helpers, including workbuffer-backed localized text conversion, wrapper controls, common sizing, menu chrome, and slide-panel behavior. Game UI owns screen-specific composition, game-state gating, labels, and interaction flows.
- The shared menu-helper header remains parseable in both client and server projects because shared game UI code includes it on both sides. Its client-only panel state, font, and chrome declarations stay behind `BT_CLIENT`; game screen headers guard their client-only members without hiding shared declarations needed by server-side screens.

## Graphics Quality Levels

- The renderer quality levels are engine-owned client-only wrappers. Each discrete level is the persisted source of truth. Water is consumed directly during per-frame rendering; the other levels drive derived renderer wrappers that are never persisted.
- Apply functions write derived renderer wrappers through `Set` when a level changes and once after both successful and failed graphics-settings loads. `Graphics::Refresh` is the sole consumer polling those wrappers; apply functions never call `Changed<T>()`.
- Values read from a settings file are opaque input, so clamp each level before indexing its table.
- `GraphicsQualityWrappersBase.h` remains a `BT_CLIENT` direct include and stays out of `Engine.h`; game UI owns persistence and menu consumption.

## Shared Types

- `engine::UiState` and the unguarded `engine::GameBase` members `meUiState`, `mModalMessage`, and `mbShowImGui` form the shared engine/game UI-state contract. The engine owns their public vocabulary and storage for both client and server; game screens retain state transitions, gating, and rendering semantics.
- Height-dependent wrapper groups resolve camera-height-conditioned render values; Render (`../Graphics/Render/AGENTS.md`) owns how those resolved values reach the per-frame GPU buffers.
- Client-only curves use bounded monotone cubic interpolation and an ImPlot editor. Endpoints remain X-locked and interior control points ordered.
- The Lighting tab deliberately keeps two combine curves, `gCombineCurveOld` and `gCombineCurveNew`, behind the `gbUseCombineCurveNew` toggle so tuning can be compared live against the shipping baseline. They currently hold identical control points; that is the A/B setup, not dead duplication. Collapse to a single curve once tuning settles.
- `NetworkUiControl` disables a control while authoritative state has not resolved its request; call `Update` every frame with that authoritative state.

Most wrapper storage compiles into both builds so server-side simulation can read defaults. Client-only ImPlot types and menu panel, font, and chrome state remain `BT_CLIENT`-guarded; the shared menu helper's conversion and wrapper-control surface is available in both builds.

## See Also

- Screens (`Screens/AGENTS.md`) - Engine screen routing
- TweaksScreen (`Screens/TweaksScreen/AGENTS.md`) - Runtime parameter UI contracts
