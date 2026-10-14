<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-13T13:09:59.264Z","dependsOn":[]} -->
# Extract the time-scale debug text from client input processing

## Context

`GameBase::ProcessInput` (`Engine/Source/GameBase.cpp:45-202`) runs client menu-input policy. Inside its `kbDebugInput` block, the time-scale debug text block (137-163) does not respond to input: it reacts to `mTimeStep.mbTimeScaleChanged`, which `TimeStep` sets when a timespeed change is applied (`Engine/Source/Frame/TimeStep.cpp:64-98`), clears the flag, logs the new ratio, and rewrites the `kTextDebug` text area with `Time ratio: Nx`, `Time ratio: 1/Nx`, or an empty string. The server consumes its own copy of the flag in `Server::BroadcastTimespeedIfChanged` (`Engine/Source/Network/Server/Server.cpp:836-842`).

Origin: a user-directed read-only review of the top 20 functions by complexity mass at `43450112`. The user's rule: a function is never split because it is big; code that belongs together stays together, and only a piece that does not belong moves. The reviewer rated this item weak, and the user asked for this Plan anyway. The rest of `ProcessInput` was judged to belong and stays as is.

Trade-off to weigh at claim time: the pause toggle right after it (166-177) also writes `kTextDebug`, and that block responds to input. While both writers sit next to each other, a reader sees at a glance that the pause text (written second) wins over a time-ratio text in the same frame. After the move, that ordering lives only in the call's position, and one of the two writers is out of view. Keeping the block inline is defensible for that reason.

## Design

Recommended: a file-static `static void UpdateTimeScaleDebugText(TimeStep& rTimeStep)` under `BT_CLIENT` in `GameBase.cpp`, holding lines 139-162 with `mTimeStep` replaced by `rTimeStep`, called as `if (mTimeStep.mbTimeScaleChanged) { UpdateTimeScaleDebugText(mTimeStep); }` at the same spot inside the `kbDebugInput` block. Equivalently, the flag test can move inside the function and the call becomes unconditional; the implementer picks one. The comment at 136 stays at the call site, since it explains ordering against the later game callback. Rationale for a file-static over a `GameBase` member, as the review proposed: the function needs only `mTimeStep` and `gpImGuiManager`, and a member would add a declaration to `GameBase.h`, which `Engine.h` aggregates into both projects' PCH, for no gain.

Unchanged: the call stays before the pause toggle, so the pause text still wins in a frame with both; the log line, text strings, and workbuffer use stay as they are.

Risk tier: Tier 1 (`.agents/references/risk-tiers.md`). Trigger: a local behavior-preserving move into a file-static function in one translation unit, with no public signature or invariant exposure. `mbTimeScaleChanged` and the debug text are client-only presentation state outside the CRC; the server's handling is untouched.

## Critical files

- `Engine/Source/GameBase.cpp`

## In scope

- New file-static `UpdateTimeScaleDebugText` in `GameBase.cpp` under `BT_CLIENT`.
- The time-scale debug text block of `GameBase::ProcessInput` (137-163), replaced by the call.

## Out of scope

- The pause toggle (166-177) and every other part of `GameBase::ProcessInput`.
- `GameBase.h`, `TimeStep`, `Server::BroadcastTimespeedIfChanged`, and `ImGuiManager`.
- Any change to text, log level, or the place in the frame where the text updates.

## Acceptance criteria

- Client builds (`/compile`).
- `/agent-harness`: after the server `timescale` command `{"faster":true}`, the client log carries `Timespeed changed Multiply: 2 Divide: 1`.

## Coordination

- `Documents/Plans/Engine/TextChat.md` also edits `GameBase.cpp`: in `GameBase::ProcessInput`, after the quit gate (51-54), a chat-open early return and a chat-opening early return on `kOpenChat`, both modelled on the modal early return (56-60), plus the policy-order comment above the function (42-44). Neither touches the time-scale debug text block (137-163) or its call site inside the `kbDebugInput` block; both early returns precede that block, so a chat frame skips it as a modal frame does today, and this Plan keeps the call in place. Neither Plan needs the other first. Whichever lands second rebases onto the other's edits.
