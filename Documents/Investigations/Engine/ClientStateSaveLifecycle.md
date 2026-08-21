# Investigation: What should the `ClientState.bin` save/load lifecycle be?

Open question, not a decision. Recorded at user request as a residual of the
`Documents/Plans/Engine/EngineClientSettingsOwnership.md` session, which keeps
`ClientStateSettings` explicitly out of scope. Nothing here is a proven defect
yet; the point is to decide what the intended behavior is before anyone changes
code.

## Why this was raised

`ClientState.bin` is the only one of the five persisted client files that is not
written by the shutdown save block in `Engine/Source/Main.cpp`. That looked like
a missing shutdown call. It is not — the file uses a different, deliberate-looking
mechanism (write-through on change). The open question is whether that mechanism
is the intended design, and whether it behaves acceptably.

## Verified evidence

All line numbers verified against the session baseline
`af5c27d3dd1bb0f49624c134411b7062d673c813`.

The data and its two file operations:

- `Projects/BrokenEngineSandbox/Source/ClientSettings.cpp:359-368` —
  `ClientStateSettings` (`kiVersion = 3`): fleet GUID, focused ship id, camera
  eye-height target; path constant `ClientState.bin`.
- `Projects/BrokenEngineSandbox/Source/ClientSettings.cpp:370-382` —
  `SaveClientState()` copies the three `gpGame` "remembered" fields and calls
  `engine::WriteVersionedFile`.
- `Projects/BrokenEngineSandbox/Source/ClientSettings.cpp:384-402` —
  `LoadClientState()` restores those fields and additionally snaps both
  `gpCamera->mfCameraEyeHeight` and `mfCameraEyeHeightTarget` to the saved value
  so the camera starts at the saved zoom instead of easing to it.

Load side:

- `Engine/Source/Main.cpp:281` — `game::LoadClientState()` runs once at startup,
  after `Camera`, `Game`, and `Input` exist.

Save side (this is what makes the shutdown block look incomplete):

- `Engine/Source/Main.cpp:460-466` — the client shutdown block calls
  `SaveTweaksSettings()`, `SaveSoundSettings()`, `SaveGraphicsSettings()`, and
  `SaveGameSettings()`. `SaveClientState()` is absent.
- `Projects/BrokenEngineSandbox/Source/Game.h:130-133` — the three remembered
  fields are documented as "In-memory mirror of `ClientState.bin`; loaded at
  startup, written through whenever any tracked field changes."
- `Projects/BrokenEngineSandbox/Source/Game.cpp:653-684` —
  `Game::CaptureClientStateAndSaveIfChanged()` recomputes the three values,
  returns early when all three are unchanged, and otherwise assigns them and
  calls `SaveClientState()`.

Its callers:

- `Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp:79` — called at the
  end of the camera update, i.e. on every client frame, guarded only by the
  diff check above.
- `Projects/BrokenEngineSandbox/Source/FleetSelection.cpp:44`, `:54`, `:102`,
  `:265` — focus-next fleet, focus-previous fleet, focus change, and the end of
  `SyncFleets` (server-driven focus changes).

So the write-through design is real and intentional-looking, and the original
"`SaveClientState()` has no call site" hypothesis is disproven.

## Questions to answer

1. **Is write-through the intended lifecycle for this file, or an artifact?**
   The other four client settings files save at shutdown. `ClientState.bin`
   saves on change. Should the two conventions be unified, and in which
   direction?
2. **How often does the file actually get written during normal play?** The
   camera call site runs every frame, but `mfCameraEyeHeightTarget` only changes
   on a frame whose input poll reports a nonzero scroll delta that the min/max
   clamp does not absorb (`Engine/Source/Input/Input.cpp:118`,
   `Engine/Source/Graphics/EngineCamera.cpp:243-258`); the
   animation frames in between only move `mfCameraEyeHeight`, which the diff
   check does not test, so they write nothing. That makes the write count
   roughly one per wheel notch rather than one per frame. Measure the real write
   count for one ordinary zoom gesture before assuming this is or is not a
   problem.
3. **If that write rate is high, does it matter?** Relevant angles: disk churn,
   the `ScopedSuppressAllocationTracking` wrapper at
   `ClientSettings.cpp:372-373` that exists specifically because the file write
   allocates, and any frame-time hitch from synchronous file I/O inside the
   camera update.
4. **Is crash-durable client state a requirement?** Write-through survives a
   crash or a hard kill; a shutdown-only save does not. Determine which client
   exit paths actually reach `Main.cpp:460-466` — if common exits skip it, that
   is an argument for keeping write-through.
5. **If write-through is kept, should the write be coalesced?** A dirty flag
   plus a debounce, an end-of-frame flush, or a shutdown flush would preserve
   the behavior with far fewer writes. Whether that complexity is warranted
   depends on question 2's measurement.
6. **Should a shutdown save be added as well?** It may be redundant given
   write-through, or it may be the cheap way to make coalescing safe. Decide
   only after question 5.
7. **Confirm this state is genuinely outside determinism.** Fleet focus and
   camera zoom read as client-only visual state, so `ClientState.bin` should be
   irrelevant to CRC and replay. Confirm that before any change, because a
   change to when focus is captured could touch input-derived state.

## What a follow-up needs before it becomes a Plan

An answer to question 1 (a user or architectural decision on the intended
lifecycle) plus the measurement from question 2. With both, the remaining work
is decision-complete and moves to `Documents/Plans/Engine/` with the required
byte-zero metadata marker.

## Out of scope

No code change is authorized by this document. It authorizes no change to the
save/load lifecycle in `ClientSettings.cpp`, `Main.cpp`, `Game.cpp`,
`Camera.cpp`, or `FleetSelection.cpp`.
