<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T20:57:20.738Z","dependsOn":[]} -->
# Fix: landing-lock fixture suite asserts a stale source-text pattern for the recover re-read policy

## Context
`.agents/skills/finalize-changes/scripts/Test-LandingLockStatusFixtures.ps1`
fails exactly one assertion when run as documented:

```text
FAIL recover failed re-read routes through absent or unverifiable policy
```

Every other assertion in the suite passes and the suite otherwise completes, so
the failure is a single stale check rather than a broken suite or broken
coordination behavior.

The failing check is a source-text regex, not a runtime observation. At
`.agents/skills/finalize-changes/scripts/Test-LandingLockStatusFixtures.ps1:348`
the suite builds `$failedRereadPolicy` from a pattern that requires the literal
call form `std::filesystem::exists ( rLocator.path , error )`, and line 349
asserts it. The C++ region it inspects is `HandleRecover` in
`Tools/WorktreeCli/LandingLockCommands.cpp`, where line 207 now reads:

```cpp
const bool bRevalidatedExists = std::filesystem::exists(ExtendedLengthPath(rLocator.path), error);
```

The `ExtendedLengthPath(...)` wrapper around the path argument is what the
pattern does not accommodate. The behavior the assertion exists to certify is
present and correct: `LandingLockCommands.cpp:204-209` still re-reads the
metadata, and on a failed re-read still dispatches
`LandingRecordState::kAbsent` only when the existence probe succeeded and
reported the record absent, and `LandingRecordState::kUnverifiable` otherwise.
Only the fixture's literal expectation is stale, so the suite reports a failure
where no defect exists.

This is pre-existing: the session that observed it had no commits of its own,
so both the fixture line and the C++ line are unchanged tree content. It is
out of scope of the change that observed it —
`Documents/Plans/Agents/FinalizeFixtureSuiteInvocationUndocumented.md` is a
Tier-1 documentation Plan whose `## Out of scope` states "Any behavior change to
the fixture suites or to the finalize bundled scripts they exercise; this is a
documentation gap only" — so the fix could not land there.

## Design
Re-verify both citations against the current tree first, because either file may
have moved on: confirm the failing assertion is still the source-text regex at
`Test-LandingLockStatusFixtures.ps1:348-349`, and confirm the routing in
`LandingLockCommands.cpp` `HandleRecover` still dispatches absent versus
unverifiable exactly as described above.

Then make the smallest change that lets the assertion pass against the current
C++ while still certifying the same routing: update the stale pattern so the
path argument may carry the `ExtendedLengthPath(...)` wrapper. Keep the pattern
anchored on the parts that carry the meaning — the failed `ReadMetadata`
re-read, the `std::error_code`-guarded existence probe, and the
`!error && !bRevalidatedExists ? LandingRecordState::kAbsent :
LandingRecordState::kUnverifiable` dispatch — so a real routing regression
still fails the assertion. Do not weaken the check into one that would pass
against arbitrary code.

No C++ change is required or authorized: the runtime behavior under test is
already correct.

## Critical files
- `.agents/skills/finalize-changes/scripts/Test-LandingLockStatusFixtures.ps1` —
  the stale `$failedRereadPolicy` pattern at `:348` and its assertion at `:349`;
  this file is the authorized fix boundary.
- `Tools/WorktreeCli/LandingLockCommands.cpp` — `HandleRecover` at `:195-213`,
  read-only reference for the routing the pattern must keep certifying.

## In scope
- The `$failedRereadPolicy` pattern assignment and its `Assert-True` call in
  `Test-LandingLockStatusFixtures.ps1`

## Out of scope
- Any change to `Tools/WorktreeCli/LandingLockCommands.cpp` or any other
  WorktreeCli C++, including the `ExtendedLengthPath` wrapper itself
- Any other assertion, fixture, or helper in
  `Test-LandingLockStatusFixtures.ps1`, and the two sibling finalize fixture
  suites
- The finalize fixture-suite invocation documentation owned by
  `Documents/Plans/Agents/FinalizeFixtureSuiteInvocationUndocumented.md`
- Replacing source-text assertions with a different verification approach

## Risk tier and invariants
Expected Tier 1: a local, mechanical refresh of one verification pattern inside
one script, with no public signature exposure and no change to what the
assertion certifies. Escalate to Tier 2 if the fix turns out to require changing
what the assertion checks, touching another assertion, or changing any script
parameter or result contract; escalate to Tier 3 if it reaches WorktreeCli
landing-lock C++, which is build and landing coordination that can block other
sessions. The assertion must keep failing on a genuine absent-versus-
unverifiable routing regression.

## Acceptance criteria
- `.agents/skills/finalize-changes/scripts/Test-LandingLockStatusFixtures.ps1`
  run as documented reports no failures, including the previously failing
  `recover failed re-read routes through absent or unverifiable policy`
- The refreshed pattern still fails when the absent-versus-unverifiable dispatch
  in `HandleRecover` is altered, demonstrated against a scratch copy of the C++
  text rather than by editing the tracked source
- `plan validate` exits 0

## Notes
This Plan is keyed to the pair (`Test-LandingLockStatusFixtures.ps1` failed
re-read policy assertion, stale `std::filesystem::exists` source-text pattern).
`Documents/Plans/Agents/FinalizeFixtureSuiteInvocationUndocumented.md` names the
same script but owns a different root cause — no documented invocation for the
finalize fixture suites — and explicitly excludes fixture behavior changes, so
the two do not overlap.
