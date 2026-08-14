# Engine UI - Shared Wrapper and Screen Infrastructure

Shared runtime settings, renderer quality levels, network-pending controls, and client-only curve/editor support. Engine Tweaks registration contracts live in TweaksScreen (`Screens/TweaksScreen/AGENTS.md`); game localization, quality-level persistence, and menu consumption live in game UI (`../../../Projects/BrokenEngineSandbox/Source/Ui/AGENTS.md`).

## Wrapper Contracts

- `Wrapper` stores float, bool, or discrete values with one-consumer change tracking. `Changed<T>()` advances the previous-value state, so exactly one consumer may poll a wrapper each frame.
- Mutate through `Set` or `Reset`; assignment is disabled. `Set` leaves the prior value intact so the next poll observes a change. `Reset` updates both values and is for initialization or capability clamping that must not trigger rebuilds.
- Discrete wrappers fall back to their first allowed value when persisted or capability-derived input is invalid. Float wrappers may snap to a configured grid.
- Wrapper bounds annotated with shader invariants are correctness constraints, not UI tuning.
- Settings that alter baked render-target dimensions must participate in `Graphics::Refresh`; prefer per-frame uniforms when recreation is unnecessary.
- Wrapper headers are intentionally consumed directly rather than aggregated into `Engine.h`, limiting recompilation from tuning edits.

## Graphics Quality Levels

- The renderer quality levels are engine-owned client-only wrappers. Each discrete level is the persisted source of truth, while the renderer wrappers it drives are derived and are never persisted.
- Apply functions write the derived renderer wrappers through `Set` when a level changes and once after both successful and failed graphics-settings loads. `Graphics::Refresh` is the sole consumer polling those wrappers; apply functions never call `Changed<T>()`.
- Values read from a settings file are opaque input, so clamp each level before indexing its table. Keep the `kWaterLevels` counts within the `gWaterLowCount`/`gWaterMediumCount` allowed set `{15, 31, 63, 127, 255}`.
- `GraphicsQualityWrappersBase.h` remains a `BT_CLIENT` direct include and stays out of `Engine.h`; game UI owns persistence and menu consumption.

## Shared Types

- Height-dependent wrapper groups resolve camera-height-conditioned render values; Render (`../Graphics/Render/AGENTS.md`) owns how those resolved values reach the per-frame GPU buffers.
- Client-only curves use bounded monotone cubic interpolation and an ImPlot editor. Endpoints remain X-locked and interior control points ordered.
- The Lighting tab deliberately keeps two combine curves, `gCombineCurveOld` and `gCombineCurveNew`, behind the `gbUseCombineCurveNew` toggle so tuning can be compared live against the shipping baseline. They currently hold identical control points; that is the A/B setup, not dead duplication. Collapse to a single curve once tuning settles.
- `NetworkUiControl` disables a control while authoritative state has not resolved its request; call `Update` every frame with that authoritative state.

Most wrapper storage compiles into both builds so server-side simulation can read defaults. ImGui/ImPlot-dependent types remain `BT_CLIENT`-guarded.

## See Also

- Screens (`Screens/AGENTS.md`) - Engine screen routing
- TweaksScreen (`Screens/TweaksScreen/AGENTS.md`) - Runtime parameter UI contracts
