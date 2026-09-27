<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T20:06:09.587Z","dependsOn":[]} -->
# Cleanup: Engine — make the two function-scope constexpr locals in constexpr functions static (style rule 25)

## Context
`Documents/C++StyleGuide.txt` rule 25 asks for `static constexpr` at function
scope. The Engine scanner-rule sweep left two function-scope `constexpr` locals
unchanged because they sit inside `constexpr` functions, where a `static` local
needs C++23 P2647 ("Permitting static constexpr variables in constexpr
functions"), and the sweep had not confirmed toolchain support:
- `Engine/Source/Frame/Collections/CollectionMemory.h:16` —
  `constexpr size_t N = std::extent_v<MEMBER>;` inside the `constexpr` template
  `ForEachMemberPointer`.
- `Engine/Source/Agent/Commands/CellCoordinateProbe.cpp:21` —
  `constexpr float kfGridPitch = ...;` inside the `constexpr` function
  `DistinctAxisSamplePositions`.

Support is now confirmed: the project builds as `stdcpp23`
(`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
`LanguageStandard`), and the installed MSVC 14.51.36231 compiler accepts a
`static constexpr` local in a `constexpr` function used in a `static_assert`
under `-std:c++latest` and `-std:c++23preview`, while `-std:c++20` rejects the
same code with C3615. The two sites were checked on 2026-09-27.

## Design
The author's recommendation: add `static` to each of the two declarations and
change nothing else. Both initializers are constant expressions, so the value
and every use are unchanged; only the storage duration of a compile-time
constant changes.

## Critical files
- `Engine/Source/Frame/Collections/CollectionMemory.h`
- `Engine/Source/Agent/Commands/CellCoordinateProbe.cpp`

## In scope
- `ForEachMemberPointer` (`CollectionMemory.h:16`): `constexpr size_t N` becomes
  `static constexpr size_t N`.
- `DistinctAxisSamplePositions` (`CellCoordinateProbe.cpp:21`):
  `constexpr float kfGridPitch` becomes `static constexpr float kfGridPitch`.

## Out of scope
- Every other rule 25 site, and every other change to either function.
- `Common/`, `DataPacker/`, `Projects/` and `Tools/` (sibling sweep Plans).

## Risk tier and invariants
Tier 1 (mechanical): trigger is local behavior-preserving style work with no
public signature or invariant exposure (`.agents/references/risk-tiers.md`).
`ForEachMemberPointer` walks collection member pointers, so the change must
leave its iteration unchanged; it does, because `N` keeps the same value. No
serialized, CRC-covered, wire, or `.pack` layout changes.

## Acceptance criteria
- `/compile` Client and Server Debug and Release builds pass.

## Notes
Originating residual: the Engine scanner-rule style sweep (rule 25 rows for
`Engine/Source/Frame` and `Engine/Source/Agent`).
