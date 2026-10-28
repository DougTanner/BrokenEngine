# Camera Projection Controls

Revisit When: the user schedules the deferred wider-FoV camera and Graphics-menu projection controls for implementation.

## Context

The user liked a temporary change that passed `gFieldOfView` directly, in
radians, to `XMMatrixPerspectiveFovRH`, and requested its reversion for now
and a follow-up implementing it with more FoV-related Graphics sliders and a
non-perspective matrix toggle. The user also observed the terrain shadow
texture's coverage rectangle fully on-screen after widening the view. Fixing
that coverage is part of this feature, not a separate deferred cleanup.

`Engine/Source/Graphics/CameraBase.cpp:380`,
`CameraBase::CalculateMatricesAndVisibleArea`, currently divides the setting
by aspect ratio. `Engine/Source/Ui/WrapperBase.cpp:7` defines its default as
45 degrees and range as 25–110, with no menu binding. At 16:9 the current
vertical angle is 25.3125 degrees; the liked change made it 45 degrees.

`Engine/Source/Graphics/Render/Render.h:33`,
`ComputeWorldSizedTexelArea`, independently divides FoV by aspect ratio.
`PopulateShadowArea` in `GlobalUniforms.cpp` and the lighting uniforms use
that helper. Changing only the camera therefore widens the view while these
world-space footprints retain the old angle. This proves a projection/coverage
mismatch; the precise contribution of headroom, temporal history, elevated
geometry, and retained grids to the observed border still needs live diagnosis.
The supplied observation records no reproducible harness command sequence.

This is manually executed feature work under `Documents/Features`, not a
scheduler-tracked debt Plan. No implementation is authorized by saving it.

## Design

The author's recommended minimum controls are:

| Graphics control | Behavior and initial recommendation |
| --- | --- |
| Vertical Field of View | Perspective vertical angle in degrees, default 45, range 25–110; aspect ratio changes horizontal coverage only. |
| Orthographic Projection | Toggle, default off; selects a right-handed orthographic projection. |
| Orthographic Scale | Framing multiplier, default 1, recommended range 0.5–2; enabled in orthographic mode. |

These are two related sliders and one toggle, without adding unrelated camera
tuning. Disable the FoV slider in orthographic mode without resetting it.
Recommend orthographic vertical span
`2 * eye-to-target distance * tan(45 degrees / 2) * Orthographic Scale`,
and horizontal span equal to vertical span times aspect ratio. This gives the
same target-plane framing as the default perspective view at scale 1, keeps
wheel zoom meaningful, and leaves FoV independent of orthographic framing.
Keep the existing camera pose and target when changing mode; switching from a
nondefault perspective angle may change framing deliberately. Confirm these
recommendations, ranges, and labels during implementation preparation; they
are author recommendations, not recorded user choices.

Use the existing Wrapper, `ColumnSlider`, and `WrapperToggle` infrastructure
in `GraphicsMenuScreen::Render`. Persist all three values through
`GraphicsSettings.cpp`, validate them before applying any settings, restore
them through Defaults, and bump the owning settings version for the changed
format. Reject older files using the existing current-format gate; add no
migration or parallel settings store. Keep stable, distinct harness labels.

Use the active projection consistently for models and world geometry,
CPU/GPU projection and unprojection, picking, screen-space overlays, panning,
visible areas, and culling. Audit consumers rather than assuming a matrix
replacement alone makes orthographic behavior correct. Preserve the camera's
cell-local basis, flight-plane targeting, wheel-zoom smoothing, and depth
convention. Do not introduce separate model and world camera projections.

Derive coverage and world-texel scale from the active projection's footprint,
including terrain shadow and lighting areas, visible-area snapping, water and
terrain LOD selection, object/terrain caster and receiver culling, and sun-side
shadow extension. Keep shadows in world space and independent of camera
orientation. Preserve shared grid snapping, temporal cadence, reprojection,
newly exposed texel rejection, and cell-rebase behavior. Update or invalidate
retained bounds, quad-size latches, and temporal footprints on FoV, mode,
scale, or aspect changes; camera height alone no longer identifies framing.
The coverage must include visible elevated receivers and required casters,
not merely the screen-corner intersections with the zero-height plane.

Do not hide insufficient coverage with cosmetic fading or an arbitrary large
texture. Establish the footprint and required headroom, then update existing
coverage calculations and refresh conditions. Keep quality-level texture
budgets and verify the density/performance consequences of wider views.

## Critical files

- `Engine/Source/Graphics/CameraBase.cpp` and `.h`: projection, visible corners,
  zoom/LOD latches, texel-height references, screen projection.
- `Engine/Source/Graphics/Render/Render.h`, `GlobalUniforms.cpp`,
  `LightingUniforms.cpp`, and `MainUniforms.cpp`: coverage, temporal products,
  world/model uniforms, and shadow extension.
- `Engine/Source/Ui/WrapperBase.cpp` and `.h`,
  `GraphicsSettingsWrappersBase.cpp` and `.h`, `GraphicsSettings.cpp`, and
  `Screens/GraphicsMenuScreen.cpp`: wrapper ownership, controls, persistence.
- `Engine/Source/Input/` and
  `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.cpp`: inventory
  projection-dependent picking and pan consumers before editing.
- `Engine/Source/Graphics/Managers/`, `Engine/Data/Shaders/`, and game render
  consumers: change only projection-dependent coverage/culling/LOD contracts
  shown by the implementation inventory to require propagation.

## In scope

- Implement the vertical FoV correction, the two sliders, and orthographic toggle.
- Correct all projection-dependent render coverage and retained state needed
  to support their full advertised ranges, starting with terrain shadows.
- Persist, validate, load, and reset the new player-facing values.
- Update affected subsystem documentation and CPU/GPU contracts where changed.
- Capture future runtime evidence for the acceptance matrix below.

## Out of scope

- Simulation, PostRender/CRC state, network or replay formats, gameplay rules,
  flight-plane definitions, and server behavior.
- Reverse-Z depth, new camera modes beyond perspective/orthographic,
  cinematic controls, camera rotation UI, and a general projection framework.
- Unrelated Graphics-menu redesign or shadow/lighting algorithm replacement.

## Acceptance criteria

1. At 16:9, default perspective uses a 45-degree vertical angle. At 4:3,
   21:9, and portrait aspect ratios it retains that vertical angle and changes
   horizontal coverage consistently. Verify minimum, default, and maximum FoV.
2. Both sliders and the toggle apply live, retain inactive values, survive
   save/reload, and reset through Defaults. Invalid/non-finite settings reject
   the entire file through the existing loader policy; old versions use defaults.
3. Orthographic scale endpoints and default give the specified span, parallel
   projection, and predictable wheel zoom. Toggle repeatedly at minimum and
   maximum zoom without stale bounds, invalid matrices, or clipping regressions.
4. Across the FoV/scale endpoints, both projection modes, zoom extremes, and
   the aspect ratios above, no terrain-shadow coverage rectangle becomes
   visible, no visible receiver loses required terrain/object shadows, and
   lighting, terrain, and water cover the view. Include elevated flight-plane
   objects, low sun angles, each shadow/lighting quality, and slow update cadence.
5. Pan across a cell boundary, zoom inward/outward, resize, and change controls
   while temporal updates are throttled. Coverage expands in time, history
   does not smear or expose stale strips, and grid/LOD transitions remain stable.
6. Pick/select objects near the center and each screen edge, pan, track a moving
   unit, and compare model/world alignment in both modes. Input and overlays
   agree with the displayed scene, including elevated objects.
7. Record before/after frame-time and render-area/texel-density evidence in the
   same populated scene at default and widest supported framing. Explain any
   material cost increase and verify existing texture budgets remain bounded.

## Notes

Future implementation is Tier 3: new persisted settings alter serialization
layout, and projection changes integrate camera, renderer, UI, and input
contracts. Keep all presentation settings outside deterministic frame state.
Before implementation, follow the normal preparation, alternatives, plan
review, and approval workflow; this saved feature is not an approved execution
card. Run client/server compile checks through `/compile` and runtime checks
through `/agent-harness`, with screenshots and recorded setup for the matrix
above. Add no unit tests. No builds or runtime verification were performed to
save this document.

No existing Plan or Feature found by FoV, field-of-view, orthographic, or
non-perspective searches owns this feature. `ReverseZDepth.md` touches the same
projection constructor but owns a different depth-mapping change; retain its
depth contract if it lands first and do not bundle that feature here.
