<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-15T22:33:41.595Z","dependsOn":[]} -->
# Store the game clock and every frame-collection instant as 64-bit integer nanoseconds

## Context

The game clock is a `float` number of seconds. The fixed tick adds to it and nothing ever resets it during a session: `GameBase::mfCurrentTime` (`Engine/Source/GameBase.h:304`) advances at `Engine/Source/GameBase.cpp:305` (`+= iFullTicks * kfDeltaTime`) and `:386` (`+= kfDeltaTime`). `RunFrameTick` (`GameBase.cpp:517`, `Engine/Source/Frame/FrameBase.cpp:189,245`) and `GameBase.cpp:962` copy it into `FrameInterpolateBase::fCurrentTime` (`Engine/Source/Frame/FrameBase.h:116`). `fCurrentTime` is one of the frame-wide `Values()` (`FrameBase.h:176`), so it is serialized, sent in full-state snapshots, and hashed into the shared CRC (`FrameBase.cpp:17`). Grid saves persist it across restarts (`Engine/Source/File/GridSave.h:16`, `GridSave.cpp:143,152,193`), and replays and client sessions restore it (`Engine/Source/File/Replay.cpp:518,534,550`, `Engine/Source/Network/Client/ClientSessionRuntime.cpp:305,320`, `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:69-75`). Only `Game::Reset` (`Projects/BrokenEngineSandbox/Source/Game.cpp:375-376`) sets it back to zero.

Defect. This is arithmetic from code reading, not observed in a run; acceptance criterion 1 reproduces it. One tick is `kTickNanoseconds` = 31,250,000 ns (`Engine/Source/Frame/TimeStep.h:9-10`, `kiTickRate` 32), which is `kfDeltaTime` = 2^-5 s. A float holds integer multiples of 2^-5 exactly only below 2^19 s, about 6.07 days of game time. At 2^19 s one ulp is 2^-4 s, so adding one tick is an exact half-ulp tie. It rounds to even, so the clock does not change. The clock then stalls at that value for good. Every "now minus start" timer in the frame collections freezes, and every instant compared against the clock stops expiring. Precision degrades long before the stall. Near one day the ulp is 2^-7 s (about 7.8 ms), and the client's render time adds the sub-tick remainder in float (`GameBase.cpp:898`). At a 64 or 128 Hz tick rate the stall arrives at 2^18 or 2^17 s.

Prerequisite. `Documents/Plans/Game/CollectionTimesToChrono.md` (the `dependsOn` edge) splits the frame-collection `float` seconds into two `std::chrono` types: `engine::SimulationTime` for simulation instants, a `std::chrono::time_point` on the empty clock tag `engine::SimulationClock` (`Engine/Source/Frame/TimeStep.h:12-15`), and `std::chrono::duration<float>` seconds for lengths, intervals, and countdowns. `SimulationTime`'s duration is `std::chrono::duration<float>` there. User decisions recorded during that Plan's approval: a separate follow-up changes the representation of `SimulationTime`, and of the game clock, to 64-bit integer nanoseconds; lengths, intervals, and countdowns stay `std::chrono::duration<float>` seconds; signed 32-bit nanoseconds for lengths was considered and rejected, because it caps at about 2.1 s, shorter than existing cooldowns and destroy times; the change is a format change with a version bump. The rule that converts a float length to whole nanoseconds is the Plan author's recommendation (`## Design`), not a user decision. This Plan is that follow-up. The instant columns are the ones the prerequisite classifies as instants. Read them from the tree at claim time; this Plan does not list them.

Existing nanosecond conventions: `TimeStep::mTickRemainderNanoseconds` and `kTickNanoseconds` (`TimeStep.h`), and `common::NanosecondsToFloatSeconds` (`Common/Timer.h:7-12`).

## Design

The user decisions in `## Context` cover only part of this design. Every other choice below is the Plan author's recommendation, given with its reason.

Recommended representation: `engine::SimulationTime` becomes `std::chrono::time_point<SimulationClock, std::chrono::nanoseconds>`, so the value is one `int64_t` of nanoseconds since game-clock zero. The type keeps its name and its `engine::SimulationClock` tag; only the duration changes. Signed 64-bit nanoseconds last about 292 years.

Game clock:

- `GameBase::mfCurrentTime` becomes the instant type, renamed `mCurrentTime` following the prerequisite's drop-the-`f` naming. It advances by `iFullTicks * kTickNanoseconds` and `kTickNanoseconds`, which is exact integer arithmetic. Every writer listed in `## Context` assigns instants. `ClientSessionRuntime.cpp:320` becomes `fullStateTime - iAppliedBehind * kTickNanoseconds`, and the restore at `GameBase.cpp:384,417` keeps an instant.
- `FrameInterpolateBase::fCurrentTime` becomes the instant type, renamed `currentTime`, and keeps its position in `Values()`. Its CRC term and `LogDifference` row (`FrameBase.cpp:17,33`) hash and compare the 8-byte value. `LogDifference` gets the `int64_t` nanosecond count, because no `std::formatter` exists for a `time_point` on a custom clock. The same applies to `RunFrameTick`'s parameter (`FrameBase.h:319`), `StagedGridSave::fCurrentTime` (`GridSave.h:16`), the `ReconcileReplay` `rfTime` parameter and locals (`Engine/Source/Network/Client/ReconcileReplay.cpp:219,226,245`, `ReconcileReplayTick.cpp:250,357`), `Cell::fLastRenderedTime` (`GameBase.h:90,130`, read at `GameBase.cpp:767-773`), and the `GameSaveLoad.cpp:69-75` and `Replay.cpp:534,550` locals. The bit-cast float comparisons at `GridSave.cpp:155` and `Replay.cpp:518` become integer `==`.
- Grid-save clock validation (`GridSave.cpp:145-149`): the server validates every grid save. Replace `!std::isfinite(fCurrentTime)` with a range check that rejects a negative clock or one greater than `std::numeric_limits<int64_t>::max()` nanoseconds minus `kiMaximumAccumulatorTicks * kTickNanoseconds`, mirroring the existing `iTick` bound.
- `FrameInterpolateBase::fDeltaTime`, `kfDeltaTime`, and every other length stay `float` seconds.

Mixing rules (determinism, `Documents/FloatingPointDeterminism.txt`):

- Instant ± length: the length converts to `std::chrono::nanoseconds` through one new helper beside `NanosecondsToFloatSeconds` in `Common/Timer.h`, `FloatSecondsToNanoseconds(std::chrono::duration<float>)`. It returns `std::chrono::round<std::chrono::nanoseconds>(std::chrono::duration<double>(seconds))`. Recommended rounding rule: round to the nearest whole nanosecond, ties to even, applied to the exact product. Widening a float to double is exact. A 24-bit float significand times 10^9 (2^9 × 1,953,125, 21 significant bits) needs at most 45 bits, so the double multiply inside `round` is exact, and the result does not depend on evaluation details under `/fp:strict`.
- Instant − instant gives `std::chrono::nanoseconds`, which is exact. When the difference is compared with or added to a float length, the length converts with `FloatSecondsToNanoseconds`. When the difference feeds float math (a ratio, an animation time, `std::pow`), it converts once with `common::NanosecondsToFloatSeconds<float>`.
- Never let chrono's implicit common type mix `std::chrono::nanoseconds` with `duration<float>`. That common type is `duration<float, std::nano>`, so the comparison or sum would happen in float nanoseconds. Assigning such a result to an instant fails to compile, but comparisons compile silently, so the review checks every instant expression by reading it.
- Sentinel instants keep their value: a `-1` s float sentinel becomes `-1,000,000,000` ns.

Boundary conversions (non-CRC consumers keep their current types):

- Render and GPU time: `GameBase.cpp:898` computes `mCurrentTime + mTickRemainderNanoseconds` in integer nanoseconds (the remainder clamped at zero as today). The time since epoch then converts to `duration<float>` seconds for `RenderGlobal`/`RenderMainPresentAcquire` (`GameBase.cpp:916,920`, `Engine/Source/Main.cpp:331-332`). `GameBase.cpp:738`'s `fWindowStartTime` takes the frame instant as `std::chrono::duration<double>` seconds. `MainUniforms.cpp:579` and `Engine/Source/Profile/ProfileScreens.cpp:206` take float seconds.
- Server statistics: `ServerCellStats.cpp:22` fills its existing `duration<float>` from the frame instant's time since epoch, so `Engine/Source/Server/ServerDisplay.cpp:558` is unchanged.
- Agent commands: in `read_frame` and `edit_frame` (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`), an instant value's JSON form is an integer number of nanoseconds within `int64_t`. The existing `engine::SimulationTime` branches of `ReadElement` (`ServerFrameEdit.cpp:188-191`) and `WriteElement` (`:497-500`) change from float seconds to that integer nanosecond count. `kRefusedFrameValues` (`ServerFrameEdit.cpp:47`) names `currentTime`. `AgentCommandsServerQueries.cpp` emits any instant it reports as the nanosecond count. Update the value forms in `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md` (`edit_frame`, `read_frame`).

Format versions (root `AGENTS.md`: no backward compatibility):

- Each collection whose instant columns change element size bumps its own `kiVersion`, following the "Bump on any SOA layout change" rule beside it (for example `Explosions.h:240`). The `Frame::kiVersion` base (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:38`, currently `134`) goes up by one for the frame clock, which belongs to no collection (`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`). Together these reject old grid saves (`GridSave.cpp:77-79`), old replay difference streams (`Replay.cpp:439-446`), and a mismatched client at Hello (`Engine/Source/Network/Server/Server.cpp:600`).
- `engine::kiProtocolVersion` (`Engine/Source/Network/NetworkProtocol.h:67`) goes up by one, because every full-state snapshot carries the frame clock's new bytes (`Engine/Source/Network/AGENTS.md`: `Frame::kiVersion` does not substitute for the protocol bump).
- `FrameInput::kiVersion` (`Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h:10`) goes up by one only if a `StatusChange` or `TransferData` payload member (`Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h`) carries the instant type at claim time. The replay manifest holds no time (`Replay.cpp:114-123`), so `kiReplayManifestVersion` stays.

Risk tier: Tier 3 (`.agents/references/risk-tiers.md`). Triggers: determinism/CRC (the shared-CRC clock and every CRC-visible instant change value and arithmetic), serialization and data layout plus save/replay compatibility (an element size change in saved columns and in the frame clock), and wire/protocol (full-state snapshot bytes). The change spans engine and game.

## Critical files

- `Engine/Source/GameBase.h`, `Engine/Source/GameBase.cpp`
- `Engine/Source/Frame/FrameBase.h`, `Engine/Source/Frame/FrameBase.cpp`
- `Common/Timer.h`
- `Engine/Source/File/GridSave.h`, `Engine/Source/File/GridSave.cpp`, `Engine/Source/File/Replay.cpp`
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp`, `ReconcileReplay.cpp`, `ReconcileReplayTick.cpp`, `Engine/Source/Network/NetworkProtocol.h`
- `Engine/Source/Main.cpp`, `Engine/Source/Graphics/Render/MainUniforms.cpp`, `Engine/Source/Profile/ProfileScreens.cpp`
- `Engine/Source/Frame/Collections/**` and `Projects/BrokenEngineSandbox/Source/Frame/Collections/**` (sites that declare or compute the instant type)
- `Projects/BrokenEngineSandbox/Source/Game.cpp`, `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp`, `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`, `Projects/BrokenEngineSandbox/Source/Frame/ServerCellStats.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`, `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp`, `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`

## In scope

- The representation of the prerequisite's instant type, changed to `std::chrono::nanoseconds`, and every declaration of that type: columns, Spawn/Sync fields, parameters, locals, and constants.
- Every expression that combines an instant with an instant or a length, rewritten to the mixing rules, including each removed float-boundary wrap of the frame clock in collection code.
- `GameBase::mfCurrentTime` and `FrameInterpolateBase::fCurrentTime`, with every writer and reader listed in `## Design`: the clock advance, `RunFrameTick`, grid save staging and validation, replay restore, client-session restore, save load, reconcile replay, render-regression check, and the render, profile, server-statistics, and agent boundaries.
- `FloatSecondsToNanoseconds` in `Common/Timer.h`.
- The collection `kiVersion` bumps, the `Frame::kiVersion` base bump, the `kiProtocolVersion` bump, and the conditional `FrameInput::kiVersion` bump.
- The `read_frame`/`edit_frame` instant value form and its documentation, plus `/update-claude-docs` for AGENTS.md text that names `fCurrentTime` or `mfCurrentTime`.

## Out of scope

- Lengths, intervals, and countdowns, which stay `std::chrono::duration<float>` seconds; `fDeltaTime`, `kfDeltaTime`, and `game::Frame::fSpawnTimer` keep their current types.
- Converting countdowns into "expires at" instants. This is a candidate for the claim-time review to evaluate (`## Notes`); without approval there, every countdown stays a decremented length.
- `GameBase::mfRenderTime` and `AdvanceRenderClock` (client-only `double` render clock), GPU and shader time uniforms, and the float precision of the render-time value handed to `RenderGlobal`.
- The wall-clock `common::Timer`, `TimeStep` accumulation, and time-scale logic.
- `iTick` and the tick counter.
- Any landed agent command or parameter that sets the game clock. Acceptance uses a temporary, reverted edit.
- The replay manifest format and `kiReplayManifestVersion`.
- Compatibility readers for old saves, replays, or protocol versions.

## Acceptance criteria

The temporary start-clock edit used below sets `miTickCounter` and the clock together in `Game::Reset` (`Game.cpp:375-376`) to a chosen tick T and T × 31,250,000 ns, or T × 2^-5 s on the unchanged tree. It is local verification code, reverted before landing and never committed.

1. Defect reproduction, run before implementing, on the unchanged tree: with the temporary edit at T = 16,777,216 (2^19 s), a server unpaused for at least 32 ticks shows the same `read_frame` `frame` `fCurrentTime` at both reads while `iTick` advances. If the clock does advance, stop and report: the defect statement in `## Context` is disproven. Before reverting, `save` a grid save and record a short replay with this baseline build for criteria 5 and 6.
2. `/compile` builds the client and server (`BrokenEngineSandbox`, Debug x64) with no new warnings, and the repository static checks pass.
3. Long-running clock: with the temporary edit at T = 19,353,600 (7 days, 604,800,000,000,000 ns), two `read_frame` `frame` reads N ticks apart show `currentTime` as JSON integers whose difference is exactly N × 31,250,000. A combat scenario running in that session completes its timers: at least one explosion row appears and later disappears, and weapons fire more than once per unit (blaster or missile counts change across 10 s of play). `save` that state, revert the edit, rebuild, and `load` it: `currentTime` reads back exactly as saved and keeps advancing by 31,250,000 ns per tick.
4. Determinism: the `/agent-harness` replay determinism check passes for a combat scenario recorded from the 7-day start of criterion 3, with every per-tick checksum matching. A live client and server run of it logs no `LogDifferences` CRC mismatch and no `CONFIRMED DESYNC`.
5. Old-version save rejection: `load` of the criterion-1 baseline save logs `ReadGrid <file> failed: version <old> != <new>` and returns `resetToFresh:true`.
6. Old-version replay rejection: playback of the criterion-1 baseline replay is refused with a version-mismatch error log (`ReadGrid ... failed: version` for its saved start, or `SaveLoadReplay aborted: replay version`) and plays no tick.
7. Agent value form: `edit_frame` accepts a JSON integer within `int64_t` for an instant column and rejects a non-integer for it, each with the existing error shape. `edit_frame` refuses `currentTime` in a `frame` edit.
8. Review: no instant-typed declaration in the tree still uses a float duration, and every expression that combines an instant with a `duration<float>` goes through `FloatSecondsToNanoseconds` or `common::NanosecondsToFloatSeconds`.

## Notes

- Candidate for the claim-time review, not decided here: every clock writer read for this Plan keeps `currentTime == iTick * kTickNanoseconds` (`Game.cpp:375-376`, `GameBase.cpp:305,386,417`, `ClientSessionRuntime.cpp:319-320`, `Replay.cpp:549-550`, `GameSaveLoad.cpp:72-75`). If that holds at claim time, deriving the clock from `iTick` instead of storing it would remove the separate field and its save validation. The user decision stores a nanosecond clock, so this stays an alternative for `/plan-alternatives` to present.
- Candidate for the claim-time review, not decided here: some countdowns that decrement by `fDeltaTime` each tick could become "expires at" instants. This changes behavior and CRC, so it needs its own approval.
- `Documents/Plans/Game/FrameEditRows.md` (`:32,109`) and `Documents/Plans/Game/ShipTypeSplit.md` cite the float frame clock or time fields that this Plan changes. Whichever lands second carries the other's names and types forward.
- `Documents/Plans/Engine/HardenServerForUnattendedHosting.md` prepares a server meant to run unattended. Such a server reaches the 6-day stall, so this Plan is relevant to that deployment, but neither Plan blocks the other.
