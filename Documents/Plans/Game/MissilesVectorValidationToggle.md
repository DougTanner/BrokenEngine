<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T19:27:40.567Z","dependsOn":[]} -->
# Cleanup: Missiles — gate the per-tick velocity validation on a kb* toggle instead of BT_DEBUG

## Context
`Documents/C++StyleGuide.txt` rule 6 (:51) toggles features with
`inline constexpr bool kb*` variables in `Pch.h` and `if constexpr`, not
`BT_DEBUG`/`BT_RELEASE` differences. Two sites in
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp`
wrap `common::ValidateVector<false>(vecVelocity);` in `#if defined(BT_DEBUG)`
(:160-162 inside the direction-jitter branch, :229-231 before the velocity
store). `common::ValidateVector` (`Common/Math/MathUtils.h:34`) exists in
every configuration and runs `ASSERT`, which `Common/ErrorUtils.h:20` defines
unconditionally, so dropping the guard would add these per-tick checks to
Profile and Release. No existing toggle in
`Projects/BrokenEngineSandbox/Source/Pch.h` means debug validation, which is
why the `Projects/` style sweep
(`Documents/Plans/Game/StyleGuideScannerRuleSweepProjects.md`) left both
sites as rule 6 residuals. These are the only `BT_DEBUG`-guarded
`ValidateVector` calls in `Engine/`, `Common/` and `Projects/`; the spawn and
transfer boundary calls are unguarded by design
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md`).

## Design
1. Add one toggle to each of the three configuration blocks in `Pch.h`, next
   to the other `kbDebug*` toggles: `true` under `BT_DEBUG`, `false` under
   `BT_PROFILE` and `BT_RELEASE`. The name `kbDebugVectorValidation` is
   recommended, following `kbDebugNavCrossingCheck`.
2. Replace each `#if defined(BT_DEBUG)` / `#endif` pair in `MissilesUpdate.cpp`
   with `if constexpr (kbDebugVectorValidation)` and a braced body holding the
   same call.

## Critical files
- `Projects/BrokenEngineSandbox/Source/Pch.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp`

## In scope
- The three configuration blocks of `Pch.h` (one new toggle line each)
- The two `ValidateVector` guards in `MissilesUpdate.cpp` at the lines cited
  in `## Context`

## Out of scope
- Every other `ValidateVector` call, `ValidateVector` itself, and `ASSERT`
- Other `BT_DEBUG` guards, including the Agent fixture guards that
  `if constexpr` cannot replace (owned by
  `Documents/Plans/ChangeWorkflow/CodeStyleReviewPermittedForms.md`)
- Any other toggle, and any sim arithmetic

## Risk tier and invariants
Expected Change Workflow Tier 1. Trigger: local behavior-preserving work with
no public signature or invariant exposure (`.agents/references/risk-tiers.md`).

- Each configuration evaluates exactly the checks it does today: Debug runs
  both `ValidateVector` calls, Profile and Release run none.
- `ValidateVector` only reads `vecVelocity`, so no PostRender state, CRC,
  serialization, `kiVersion`, or replay output changes.

## Acceptance criteria
- `git grep -n "BT_DEBUG" -- Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp`
  lists nothing.
- `/compile` Client and Server pass in Debug and Release.

## Notes
Independent of `Documents/Plans/ChangeWorkflow/CodeStyleReviewPermittedForms.md`;
either may land first. The new toggle lines add no rule 6 scanner hit: the
scanner matches the `#if defined(BT_*)` block lines in `Pch.h`, not the toggle
lines.
