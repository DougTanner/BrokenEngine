<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T19:34:48.990Z","dependsOn":[]} -->
# Cleanup: Tools and DataPacker — const locals that pick an overload by constness, and lookup-only std::map locals (style guide rules 18 and 32)

## Context
The whole-file scanner sweep
(`Documents/Plans/Engine/StyleGuideScannerRuleSweepCommonDataPackerTools.md`)
left these sites as residuals because each fix changes overload resolution or
a container type, which the `/code-style-review` worker step 12 bound
excludes from auto-fix. For every site below the changed overload or container
yields the same observable result.

Rule 18 forbids top-level `const` on local value variables. For the types
below the non-const overload returns the same value or reference with a
non-const qualifier, except `nlohmann::json::operator[]`, whose non-const
overload inserts a missing key. Sites (final-tree lines):

- `const std::optional<...>` later used through `operator*`, `operator->` or
  `.value()`: `DataPacker/Source/Main.cpp:223,292`;
  `Tools/ToolCommon/CoordinationStore.cpp:397`;
  `Tools/WorktreeCli/LandingLockLifecycle.cpp:55`;
  `Tools/WorktreeCli/PlanMetadata.cpp:196,233,264`;
  `Tools/WorktreeCli/PlanScheduler.cpp:221,231,310,426,427,537,615,616,623,646,662,722,741,742,753,758,822,852,902,974`
- `const std::u8string` / `const std::wstring` later used through `.data()`:
  `DataPacker/Source/DiagnosticReporter.cpp:13,38`;
  `Tools/WorktreeCli/PlanScheduler.cpp:179`
- `const nlohmann::json metadata` later indexed with `operator[]` after a
  `contains` check: `Tools/WorktreeCli/PlanMetadata.cpp:95` (uses at `:96`,
  `:100`)

Rule 32 prefers `std::unordered_map` to `std::map`. Two local maps are only
looked up, never iterated: `fields` at `Tools/WorktreeCli/PlanScheduler.cpp:248`
(`try_emplace` and `at`) and `colors` at `Tools/WorktreeCli/PlanMetadata.cpp:298`
(`find` and `insert_or_assign`).

## Design
The author's recommendation:
1. Remove the top-level `const` from every listed rule 18 local. No listed
   local or its contents is passed to `std::move`, so no copy becomes a move.
2. At `PlanMetadata.cpp:96,100`, replace `metadata["dependsOn"]` with
   `metadata.at("dependsOn")`: the `contains` check already proves the key is
   present, and `at` never inserts.
3. Change the `fields` and `colors` maps to `std::unordered_map` with the same
   key and value types.

Rationale: each change reaches the rule's form while every later read keeps
its result; only the json indexing needs a different accessor.

## Critical files
- `Tools/WorktreeCli/PlanScheduler.cpp`
- `Tools/WorktreeCli/PlanMetadata.cpp`
- `Tools/WorktreeCli/LandingLockLifecycle.cpp`
- `Tools/ToolCommon/CoordinationStore.cpp`
- `DataPacker/Source/Main.cpp`
- `DataPacker/Source/DiagnosticReporter.cpp`

## In scope
- The listed rule 18 local declarations and their later uses in the same
  function
- The declarations of the `fields` and `colors` locals

## Out of scope
- Other rule 18 sites, and `const` on references, pointees or parameters
- The `std::map<std::wstring, Plan>` rule 32 residuals, not planned because
  the scheduler iterates them in key order to produce its validate notices,
  cycle diagnostics and child rewrites, so `std::unordered_map` would need an
  explicit key sort at each iteration site:
  `Tools/WorktreeCli/PlanMetadata.h:29,30,31,32`;
  `Tools/WorktreeCli/PlanMetadata.cpp:194,230,283,296`;
  `Tools/WorktreeCli/PlanScheduler.cpp:295,424,522,555,630,631,766,777,1008`
  (iterated at `PlanScheduler.cpp:570,585,1022` and
  `PlanMetadata.cpp:328,340`)
- Rule 35 residuals in the same files (their own follow-up Plan)

## Risk tier and invariants
Tier 1 (mechanical): trigger is local behavior-preserving style work with no
public signature; every WorktreeCli and DataPacker output stays identical.

## Acceptance criteria
- `/compile` passes for WorktreeCli and DataPacker (Release)

## Notes
Originating record: the scanner sweep's rule 18 (overload by constness) and
rule 32 (container type) residuals.
