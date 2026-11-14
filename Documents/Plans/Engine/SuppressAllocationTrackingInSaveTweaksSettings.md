<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:59:10.350Z","dependsOn":[]} -->
# Suppress Allocation Tracking in SaveTweaksSettings

## Context

The three sibling settings savers open the allocation-tracking suppression
before their file write, each with the comment `// Heap: file I/O allocates`:
`SaveGameSettings` (`GameSettings.cpp:37-38`), `SaveGraphicsSettings`
(`GraphicsSettings.cpp:91-92`), and `SaveSoundSettings`
(`SoundSettings.cpp:32-33`). `SaveTweaksSettings` (`TweaksSettings.cpp:24-36`)
calls `WriteVersionedFile` with no suppression.

That save runs inside the main loop on one path. `ImGuiManager::~ImGuiManager`
calls it (`ImGuiManager.cpp:272-275`); the full-destroy branch of
`Graphics::Destroy` resets the manager (`Graphics.cpp:790-795`); and device-loss
recovery destroys `Graphics` in place with `pGraphics.reset()` at
`Engine/Source/Main.cpp:486`, while `gbAllocationTrackingReady` is true (set at
`Main.cpp:397`, cleared at `:521`). The file write allocates, so
`TrackAllocation` (`GlobalAllocator.cpp:5-29`) reaches its `DEBUG_BREAK`. The
other route into that destroy branch, `Graphics::Create`, already runs under
the suppression at `Graphics.cpp:340`.

The producer is a Vulkan device loss in a build with `kbDebugInput` (the save
returns early otherwise). No harness rig triggers device loss, so the path is
confirmed by code reading only.

Related evidence, not this Plan's scope: the next statement of the same
recovery path, `std::make_unique<Graphics>` at `Main.cpp:487`, also allocates
with no suppression at that site. Fixing the saver alone therefore does not
make device-loss recovery free of tracked allocations.

Originating gap: a deep analysis of `Engine/Source/Ui` — a refactor finding,
independently verified as partly confirmed (the saver's gap is real; it is not
the only allocation on the path). Unmet criterion: every settings saver opens
the file I/O suppression.

## Design

The author's recommendation: in `SaveTweaksSettings`, after the `kbDebugInput`
early return, add the same two lines the siblings carry — the
`// Heap: file I/O allocates` comment and a `ScopedSuppressAllocationTracking`
local.

## Critical files

- `Engine/Source/Ui/TweaksSettings.cpp`

## In scope

- `SaveTweaksSettings`: the added comment and suppression local.

## Out of scope

- `Main.cpp:486-487` and the allocation behavior of device-loss recovery as a
  whole, including the `make_unique<Graphics>` noted in `## Context`.
- `LoadTweaksSettings`, which runs at startup construction and under
  `Graphics::Create`'s suppression.
- `WriteVersionedFile` and the file helpers.

## Acceptance criteria

- The diff is the two added lines, in the same form as the three siblings.
- The BrokenEngineSandbox client and ModelViewer build.
- No live check: nothing reaches device loss on demand, so the path is
  verified by code reading plus the build.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- Change Workflow tier: Tier 2 (the author's classification). Trigger: the
  change alters where the main-loop allocation tripwire is suppressed, an
  invariant exposure (the main-loop allocation boundary in
  `Engine/Source/AGENTS.md` `## Hub Conventions`) that Tier 1 excludes. It is
  one subsystem's scoped behavior: no file format, determinism/CRC, wire,
  replay, threading, or shader exposure.
