<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T14:22:29.117Z","dependsOn":[]} -->
# Remove the library pow call from the pusher push and document the ban

## Context

Found by the architecture review phase of a deep analysis of
`Engine/Source/Frame/Collections` (2026-11-14). Line numbers are from commit
`ae681f03` plus the change this Plan landed with; locate by symbol if they
moved. `C/` abbreviates `Engine/Source/Frame/Collections/`, `BES/` abbreviates
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`.

`PushersInterpolate::ApplyPush` raises the falloff to a per-pusher power with
`XMVectorPow(vecIntensity, XMVectorReplicate(rCurrent.pfPowers[i]))`
(`C/Pushers/PushersUpdate.cpp:185`). In this build that is four scalar library
`powf` calls, verified on Windows SDK 10.0.26100.0 with MSVC 14.51.36231 and
the static C runtime: `Common/ExternalHeaders.h:155-158` defines only
`_XM_SSE4_INTRINSICS_` and `_XM_NO_XMVECTOR_OVERLOADS_` before
`<DirectXMath.h>`, so the SVML branch of `XMVectorPow` is not compiled.

The result reaches CRC-checked state. `BES/Ships/ShipBehaviours.cpp:958-963`
feeds it to `ApplyClampedPush`; `BES/Omnis/Omnis.cpp:755,773` stores the ship
velocity inside `OmnisPostRender::Update`, and that column is in the shared
tuples (`BES/Omnis/Omnis.h:323,347`). Battleships and Fighters call the same
path.

Unmet rule: `Common/Math/AGENTS.md` `## Deterministic Math` — simulation or
CRC-fed code uses the deterministic helpers "instead of library transcendental
functions". That bullet gives no reason and names no function.
`Documents/FloatingPointDeterminism.txt` section 5 guarantees only `+ - * /`,
`sqrt`, and comparisons, names only `sinf`/`cosf`/`tanf`, and gives the reason
"known to differ across implementations". `Documents/C++StyleGuide.txt` has no
rule on library math; its rule 46 covers DirectXMath estimate functions only.
No document sanctions `pow`.

What is and is not proven:

- Proven: the rule violation and its reach into a `SharedCrcMembers()` column.
- Proven: `PushersUpdate.cpp:185` is the only `pow`/`exp`/`log`-family call in
  `Engine/Source/Frame`, `Projects/BrokenEngineSandbox/Source/Frame`, and
  `Common/Math` that is not under `BT_CLIENT`. The five `std::pow` calls there
  are client-only: `C/Explosions/ExplosionsSpawn.cpp:84`,
  `BES/Fighters/FightersRender.cpp:176`, `BES/Omnis/OmnisCombat.cpp:309`,
  `BES/Omnis/OmnisRender.cpp:194`, and `BES/Missiles/MissilesRender.cpp:120`.
- Verified on the toolchain named above, by disassembling `powf` from the
  static C runtime library: `powf` picks its FMA path from the flag that
  `_set_FMA3_enable(0)` (`Engine/Source/Main.cpp:120`) clears. The only other
  dispatch on the remaining path tests the CPU feature level and chooses
  between `roundss` and a `cvtss2si` fallback, which give the same integer
  under round-to-nearest, the mode `Common/Determinism.cpp:31` sets. That path
  holds no estimate, x87, or FMA instruction. So no cross-CPU mismatch is shown
  on this toolchain, and no desync was observed. The Debug runtime's `powf`
  object has the same external symbols; its code was not disassembled.
- Not guaranteed: the result bits depend on runtime-library internals that no
  document guarantees across toolchain or SDK versions. That is the reason for
  the ban. A known AMD/Intel difference is not: none was found.
- Not verified: whether a later DirectXMath routes `XMVectorPow` to a
  vectorised library.
- Every BrokenEngineSandbox power is a whole number: 3.0
  (`BES/Fighters/Fighters.h:56`, `BES/Battleships/Battleships.h:52`) and 2.0
  (`BES/Omnis/Omnis.h:67`). The agent `edit_frame` column
  (`Engine/Source/Agent/Commands/FrameEdit.h:28`) can write any float, and
  Pushers has no range check on it.
- All four lanes of `vecIntensity` hold the same value, so each peer pays four
  identical `powf` calls.

## Design

User decisions, recorded by the authoring agent from the user's own words
(2026-11-14):

1. The ban on library `pow` in simulation code stays, and the pusher is fixed.
   The `XMVectorPow` call in `PushersInterpolate::ApplyPush` is replaced with
   repeated multiplication for whole-number powers. `pfPowers` stays a float
   column, and fractional powers stop being allowed. The pusher format
   version, `PushersInterpolate::kiVersion` (`C/Pushers/Pushers.h:36-37`, "any
   SOA layout or push behavior change"), is bumped.
2. The same change adds the ban to the style guide,
   `Documents/C++StyleGuide.txt`, for simulation files, and states it clearly
   in the relevant `AGENTS.md`. The reason given is the accurate one:
   undocumented library internals, not a known vendor difference.

Author's recommendations for details the user's words did not state, not user
decisions. The executing session follows each unless the user decides
otherwise at the Plan's approval:

- Bad-value handling. A power that is not a whole number is treated as a bug
  under the bad-value rule in `.agents/references/cpp-conventions.md`.
  `PushersInterpolate::Sync` gets an ASSERT that `rData.fPower` is a whole
  number in range, because its callers are inside the codebase. A new
  `PushersInterpolate::PostRead` rejects a row whose power fails the same test
  by throwing `std::ios_base::failure`, the form the game hooks use
  (`BES/Fighters/Fighters.h:74`), because a save holds the column. Neither
  rounds nor clamps. The range runs from 0, the `SyncData::fPower` default
  (`Pushers.h:48`), for which the loop yields 1 as `powf` did, to a new named
  maximum in the Pushers code that bounds the loop. The agent `edit_frame`
  column is an exempt developer tool under the root `AGENTS.md` trust policy
  and takes no check.
- Which functions the documented ban names. The `pow`, `exp`, and `log`
  families and their DirectXMath wrappers such as `XMVectorPow`, in simulation
  and CRC-fed code, instead of `pow` alone. Supporting fact:
  `Common/Math/AGENTS.md:8-11` already bans "library transcendental
  functions" generally, and `## Context` shows no such call outside
  `BT_CLIENT` other than the pusher's.
- Which documents are edited. The relevant `AGENTS.md` is read as
  `Common/Math/AGENTS.md` `## Deterministic Math`, the bullet `## Context`
  quotes. `Documents/FloatingPointDeterminism.txt` section 5 is edited as
  well, because its current reason, "known to differ across implementations",
  is not the accurate one.
- Where each documentation fact lives. Each fact lives once at its owning
  layer, per the root `AGENTS.md` progressive-disclosure directive. A new
  numbered rule in `Documents/C++StyleGuide.txt` owns the ban and the named
  functions. `Documents/FloatingPointDeterminism.txt` section 5 owns the
  reason, stated as `## Context` does (runtime-library internals no document
  guarantees across toolchain or SDK versions), and the statement of what
  simulation code calls, replacing its `sinf`/`cosf`/`tanf` paragraph. The
  `Common/Math/AGENTS.md` bullet states the ban for simulation and CRC-fed
  code and points to both instead of repeating them.

## Critical files

- `Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp`
- `Engine/Source/Frame/Collections/Pushers/Pushers.h`
- `Documents/C++StyleGuide.txt`
- `Documents/FloatingPointDeterminism.txt`
- `Common/Math/AGENTS.md`

## In scope

- The power step of `PushersInterpolate::ApplyPush`
  (`PushersUpdate.cpp:184-186`).
- `PushersInterpolate::kiVersion`.
- One new rule in `Documents/C++StyleGuide.txt` banning library `pow` in
  simulation code, and the statement of that ban in the relevant `AGENTS.md`.
- Following the `## Design` recommendations, unless the user decides otherwise
  at the Plan's approval: the whole-number check on `SyncData::fPower` in
  `PushersInterpolate::Sync` and on `pfPowers` in a new
  `PushersInterpolate::PostRead`, with the named maximum they share; the
  `exp` and `log` families and the DirectXMath wrappers in the new rule; the
  first bullet of `Common/Math/AGENTS.md` `## Deterministic Math` as the
  `AGENTS.md` statement; and section 5 of
  `Documents/FloatingPointDeterminism.txt`.
- The documentation sync `/update-claude-docs` requires, including the push
  falloff bullet of `C/Pushers/AGENTS.md`.

## Out of scope

- The falloff shape, the coincident-pusher correction, `ApplyClampedPush`, and
  every caller's tuning value.
- Changing `pfPowers` or `SyncData::fPower` to an integer, and adding a
  deterministic float power helper to `Common/Math/MathUtils.h`.
- The five client-only `std::pow` calls listed in `## Context`, and any other
  library math call in the repository.
- Style guide rule 46 and every other existing rule.
- Finding out whether a later DirectXMath routes `XMVectorPow` to a vectorised
  library: the change removes the call.

## Risk tier

Tier 3 — invariant/integration. Trigger: the change alters what a
determinism/CRC surface computes (CRC-checked ship velocity) and bumps a
version that gates saves and replays.

## Acceptance criteria

- No `XMVectorPow`, `pow`, or `powf` call remains in
  `Engine/Source/Frame/Collections/Pushers/`, and no `pow`/`exp`/`log`-family
  call or DirectXMath wrapper of one remains outside `BT_CLIENT` in the three
  directories `## Context` names.
- `PushersInterpolate::kiVersion` is higher than at `ae681f03`.
- `Documents/C++StyleGuide.txt` carries a rule banning library `pow` in
  simulation code, and the relevant `AGENTS.md` states the ban. The reason
  the change gives is undocumented library internals, and no edited document
  claims a CPU vendor difference.
- Following the `## Design` recommendations, unless the user decides otherwise
  at the Plan's approval:
  - A power that is not a whole number in range asserts in
    `PushersInterpolate::Sync` and fails a frame read in
    `PushersInterpolate::PostRead`; no code path rounds or clamps it.
  - The style guide rule also names the `exp` and `log` families and the
    DirectXMath wrappers, and covers simulation and CRC-fed code.
  - `Documents/FloatingPointDeterminism.txt` section 5 gives the reason.
    `Common/Math/AGENTS.md` `## Deterministic Math` states the ban and points
    to the rule and to section 5. None of the three repeats another's fact.
- `/compile` builds the BrokenEngineSandbox client and server.
- A replay determinism check through `/agent-harness`, on a scene with ships
  pushing each other, reports no CRC mismatch. The supplied evidence records no
  harness setup, so the executing session builds the scene.

## Notes

The priority of this Plan rests on the rule violation and the user's decision
to keep the ban as a precaution, not on an observed failure.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
