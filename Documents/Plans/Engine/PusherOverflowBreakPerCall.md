<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-22T17:52:39.082Z","dependsOn":[]} -->
# Break once per SetupZones call on pusher zone overflow, not once per dropped registration

## Context

`Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp:91` calls
`DEBUG_BREAK()` inside the innermost per-zone registration loop of
`PushersInterpolate::SetupZones`, so it fires once for every dropped
registration. The per-call `kWarning` summary that carries the actual count is
separate and correct, at `PushersUpdate.cpp:102-105`:
`"PushersInterpolate::SetupZones dropped {} pusher zone registrations (cap {} per zone)"`.

The per-registration placement of the break predates the session that observed
this; only its cost changed. `DEBUG_BREAK()` now always logs one `kWarning`
naming its call site (`Common/ErrorUtils.h:19`, `common::LogDebugBreak`), so a
break that used to be silent without an attached debugger now writes a log line
every time.

Observed evidence (PusherCellWideCoverage session harness run, Debug|x64,
session baseline `2058c850eaf1c6c451d3a06c06b51ea99c5f285b`): a deliberate
768-player overflow stress run produced 26,352 lines of
`DEBUG_BREAK at ...PushersUpdate.cpp:91...` against only 220 dropped-registration
summary lines — roughly 120 break lines per summary line. Overflow is beyond
the supported pusher density and is deliberately loud, but tens of thousands of
lines per run can drown every other diagnostic in the same log, which is the
concrete cost being fixed.

The `Pushers` `AGENTS.md` "Zone acceleration" bullet already describes the
intended shape — "overflow `DEBUG_BREAK`s and logs one `kWarning` summary of the
dropped registrations per `SetupZones` call" — while the code breaks per
registration; the documentation is the side to trust for intent, and the code is
what changes.

## Design

Recommended fix, and the smallest one: keep the existing per-registration drop
accounting exactly as it is, and move the `DEBUG_BREAK()` out of the innermost
loop so it fires at most once per `SetupZones` call. Concretely, delete the
`DEBUG_BREAK()` at `PushersUpdate.cpp:91`, leaving that branch to increment
`iDroppedRegistrations` and `continue`, and add the `DEBUG_BREAK()` inside the
existing `if (iDroppedRegistrations > 0)` block at `PushersUpdate.cpp:102-105`,
next to the summary `LOG`, so the count-carrying warning and the break stay
together.

That keeps every property the current behavior has that matters: overflow is
still loud, still breaks into an attached debugger at the offending
`SetupZones` call, and still reports the exact number of dropped registrations.
It only removes the duplication of one log line per dropped registration.

Rejected alternative: rate-limiting or deduplicating the break's log output
inside `common::LogDebugBreak`. That is a general logging mechanism added to
solve one call site's problem, and it would mask genuinely distinct breaks
elsewhere.

Logging and the break are outside the per-tick shared CRC, and the drop
accounting, the registration order, and the zone tables are untouched, so this
does not change simulation results.

## Critical files

- `Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp:88-105` — the
  overflow branch holding the `DEBUG_BREAK()` and the per-call summary `LOG`.
- `Engine/Source/Frame/Collections/Pushers/AGENTS.md` — the "Zone acceleration"
  bullet describing overflow behavior; update its wording only if it does not
  already match the resulting behavior.

## In scope

- Removing the `DEBUG_BREAK()` from the per-zone registration overflow branch in
  `PushersInterpolate::SetupZones` and firing it once, alongside the existing
  per-call dropped-registration summary `LOG`.
- Any wording correction needed in the `Pushers` `AGENTS.md` zone-acceleration
  bullet so it matches the resulting behavior.

## Out of scope

- The per-zone capacity value, the zone grid dimensions, the zone bounds, the
  registration order, and everything else `SetupZones` computes.
- The dropped-registration counting and the summary `LOG`'s text, level, and
  category.
- The `DEBUG_BREAK` / `DEBUG_BREAK_NO_LOG` macros and `common::LogDebugBreak`,
  including any general log rate-limiting or deduplication.
- Every other `DEBUG_BREAK` call site, including the engine startup allocation
  burst (`Documents/Plans/Engine/StartupMainLoopAllocationBurst.md`).
- Push falloff math, pusher flags, `ApplyPush`, `ApplyClampedPush`, collection
  layout, serialization, and the collection version.
- Making overflow itself less likely, and any pusher density or gameplay
  retuning.
- Backward compatibility, runtime toggles, and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 under the root `AGENTS.md` tiers: one
subsystem's runtime logging behavior. No determinism/CRC surface is exposed —
logging and debug breaks are outside the per-tick shared CRC — and no wire,
serialization, save/replay, threading, or trust-boundary surface is touched.

Invariants to preserve:

- Per-tick shared CRCs are unchanged; client and server still agree, and a
  replay of the same run reproduces them.
- Every dropped registration is still counted, and the per-call `kWarning`
  summary still reports the exact total for that call.
- Overflow still breaks into an attached debugger at the `SetupZones` call where
  it happened, and still logs at least one `kWarning`.
- `SetupZones` remains allocation-free and its `thread_local` zone state,
  lifetime, and post-`Update` phase timing are unchanged.

## Acceptance criteria

- The client and server both build (Debug|x64).
- A deliberate pusher overflow harness run produces exactly one
  `PushersUpdate.cpp` `DEBUG_BREAK` warning per `SetupZones` call that dropped
  at least one registration — one break line per summary line — where the
  baseline run produced roughly 120 break lines per summary line (26,352 against
  220).
- The dropped-registration counts in the summary warnings for that run match the
  pre-change run's counts.
- A run with no overflow produces no `PushersUpdate.cpp` `DEBUG_BREAK` warning
  and no summary warning.
- Client and server per-tick CRCs for the overflow run match each other and
  match the pre-change run.

## Coordination

`Documents/Plans/Engine/PusherCellWideCoverage.md` changed `SetupZones` in the
same file, while keeping "the 512 per-zone cap and its overflow handling"
unchanged, which is why this log-volume fix was outside that Plan's boundary
rather than part of it. That Plan has since completed and landed, so this Plan's
former `dependsOn` edge on it was removed at completion and this Plan is
immediately eligible. Re-read `PushersUpdate.cpp` before implementing here: the
line numbers above may have moved, but the fix — one break per call instead of
one per dropped registration — is unaffected.
