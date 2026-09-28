<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-28T00:12:10.431Z","dependsOn":[]} -->
# Fix: rule 51 wrapped call and declaration arguments across tracked C++

## Context
`Documents/C++StyleGuide.txt` rule 51 (:213) keeps every argument of a call and
every parameter of a declaration on one line. The only exception (:213) is a
lambda or struct literal argument: its `{}` goes on the next line per rule 2,
while every other argument, and a lambda's `[...](...)` header, stays on the
call line (good example :228-232). User decision D2 fixed that reading: a
lambda header placed on the line after the call is a violation.

The `style-rule-51` kind that `.agents/scripts/Find-SessionCandidates.ps1`
gained (`Test-Rule51Line`) now reports these wraps. Its acceptance runs over two
directories found pre-existing violations that were outside that change's scope
(no C++ source change was allowed there):

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/`: Blasters.cpp:220,
  Missiles.cpp:387, Players.cpp:229, Spaceships.cpp:417 — each a call whose
  lambda argument's header sits on the next line (for example the
  `engine::DestroySweep` call in Blasters.cpp).
- `Engine/Source/Frame/Collections/`: Collection.h:305 (a
  `common::LogDifference` call opened with `(` and its arguments on later
  lines), and a lambda header on the next line at AreaLightsRender.cpp:29,
  BillboardsRender.cpp:26, HexShieldsRender.cpp:28, PointLights.cpp:23,
  PointLightsRender.cpp:28, PointLightsUpdate.cpp:132, Puffs.cpp:23,
  PuffsRender.cpp:25, PuffsUpdate.cpp:72, SmokeTrailsRender.cpp:26,
  WindRadials.cpp:23, WindRadialsRender.cpp:29, WindRadialsUpdate.cpp:61,
  WindTrailsRender.cpp:40.

Those two runs reported no false row. The other tracked C++ trees (the rest of
`Engine/Source`, `Common`, `DataPacker`, `Tools`) were not scanned with the new
kind; an earlier rough one-line probe over all tracked C++ files suggested
roughly 40 real wraps in total, so the list above is not complete.

## Design
The author's recommendation:

1. Enumerate sites with the scanner's whole-file mode, one directory's tracked
   `*.h`/`*.cpp` files per run, as `.agents/skills/code-style-review/SKILL.md`
   documents it (`-Path` with repository-relative paths). Cover every tracked
   C++ directory under `Common/`, `DataPacker/`, `Engine/Source/`, `Projects/`
   and `Tools/`. Keep only `style-rule-51` rows, and read each one before
   editing.
2. Fix each confirmed row by joining the wrapped arguments onto the call or
   declaration line, regardless of length. For a lambda argument, move its
   `[...](...)` header onto the call line and leave its `{` on the next line
   with the body indentation rule 2 gives; a struct literal argument keeps its
   `{` on the next line. The change is whitespace and line breaks only.
3. The scanner assigns one kind per line and decides `style-rule-51` after its
   pattern table, so a wrap on a line another kind claims first (for example a
   `return std::tie(` line that `style-rule-37` reports) is not reported as
   rule 51. While reading each directory's other rows, check those lines by
   hand for a rule 51 argument wrap and fix any found the same way.
4. Build every project whose sources changed through `/compile` (client and
   server for `Engine/`, `Common/` and `Projects/` files; DataPacker, WorktreeCli
   or AgentHarness for their own trees) to confirm the edits compile.

## Critical files
- `Engine/Source/Frame/Collections/Collection.h`
- `Engine/Source/Frame/Collections/**/*.cpp` named in `## Context`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/**/*.cpp` named in
  `## Context`
- Any further tracked `*.h`/`*.cpp` under `Common/`, `DataPacker/`,
  `Engine/Source/`, `Projects/` or `Tools/` with a confirmed rule 51 argument
  wrap

## In scope
- The line breaks and continuation indentation of each confirmed rule 51
  argument or parameter wrap in a call or function declaration, including a
  lambda argument's header moved onto the call line and its body
  re-indented as rule 2 requires

## Out of scope
- The Boolean-expression half of rule 51 (140-column conditions, assignments
  and returns)
- Any other style rule, any rename, and any token change beyond whitespace
- `ThirdParty/`, and GLSL under `Engine/Data/Shaders/`
- `.agents/scripts/Find-SessionCandidates.ps1`,
  `.agents/skills/code-style-review/`, and `Documents/C++StyleGuide.txt`

## Risk tier and invariants
Expected Change Workflow Tier 1. Trigger: style-only, behavior-preserving
edits with no public signature or invariant exposure
(`.agents/references/risk-tiers.md`).

- Every edit changes whitespace and line breaks only; no token, determinism,
  CRC, serialization or wire surface changes.

## Acceptance criteria
- A whole-file scanner run over each directory in `## Design` item 1 reports
  no `style-rule-51` row.
- Every project whose sources changed builds cleanly.

## Notes
`Documents/Plans/ChangeWorkflow/CodeStyleReviewRule51WrappedArgumentKind.md`
adds the `style-rule-51` kind this Plan uses; it lands in the change that
recorded this Plan, so no dependency is needed.
