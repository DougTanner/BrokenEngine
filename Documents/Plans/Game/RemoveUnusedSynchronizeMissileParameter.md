<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:03:08.102Z","dependsOn":[]} -->
# Remove the unused fDeltaRotation parameter from SynchronizeMissile

## Context

`SynchronizeMissile` in
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp`
(definition at line 88, inside `#if defined(BT_CLIENT)`) ends its parameter
list with `[[maybe_unused]] float fDeltaRotation`. Its body (lines 89-135)
syncs the exhaust area light, the smoke trail, and the looping sound, and never
reads `fDeltaRotation`. Both callers still load
`pfDeltaRotations` from the PostRender row and pass it:

- `MissilesInterpolate::ClientInitialize`, `Missiles.cpp:163`;
- `MissilesInterpolate::Update`, `MissilesUpdate.cpp:81`.

The function is not declared in any header; its only other declaration is the
client-only forward declaration at `MissilesUpdate.cpp:34`. Logged as a
refactor candidate on 2026-11-13; verified against commit `213d2577`.

## Design

Recommended: delete the parameter and the argument at each call, because the
value is dead and the `[[maybe_unused]]` attribute exists only to silence the
warning the dead parameter would raise.

1. Definition, `Missiles.cpp:88`: remove
   `, [[maybe_unused]] float fDeltaRotation`.
2. Forward declaration, `MissilesUpdate.cpp:34`: remove
   `, float fDeltaRotation`.
3. Caller `Missiles.cpp:163`: remove the trailing argument
   `, rPostRender.pfDeltaRotations[iIndex]`.
4. Caller `MissilesUpdate.cpp:81`: remove the trailing argument
   `, rPreviousPostRender.pfDeltaRotations[i]`.

The parameter is the last one and a scalar `float`, so the `XM_CALLCONV`
vector-argument order (`FXMVECTOR` x3, `GXMVECTOR`) is unchanged.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp`

## In scope

- `SynchronizeMissile` definition parameter list (`Missiles.cpp`).
- `SynchronizeMissile` forward declaration (`MissilesUpdate.cpp`).
- The `SynchronizeMissile` call in `MissilesInterpolate::ClientInitialize`
  (`Missiles.cpp`) and in `MissilesInterpolate::Update`
  (`MissilesUpdate.cpp`): the last argument only.

## Out of scope

- The `pfDeltaRotations` collection member, its serialization, CRC
  membership, and every other reader (missile homing in Missiles Update, agent
  queries and frame edits): it stays as is.
- `SynchronizeMissileTrail` and the body of `SynchronizeMissile`.
- Any other `[[maybe_unused]]` parameter in these files.
- `AGENTS.md` files: none describes this parameter.

## Risk tier

Tier 1 — mechanical. Trigger: local behavior-preserving work with no public
signature or invariant exposure; the function is a client-only free function
declared in no header. No determinism/CRC, wire, serialization,
`Frame::kiVersion`, replay, threading, or trust exposure: the removed value
was never read, and the collection layout is untouched.

## Acceptance criteria

- No `fDeltaRotation` identifier remains in `Missiles.cpp` or
  `MissilesUpdate.cpp`, and `SynchronizeMissile` has eleven parameters at its
  definition, forward declaration, and both calls.
- The BrokenEngineSandbox client and server build without errors or new
  warnings.

## Notes

The server build does not compile `SynchronizeMissile` or either call (all
inside `BT_CLIENT` guards); it is built only to confirm the guards still
balance.
