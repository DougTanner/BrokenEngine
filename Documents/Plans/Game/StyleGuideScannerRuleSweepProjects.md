<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T13:29:03.182Z","dependsOn":["Documents/Plans/ChangeWorkflow/CodeStyleReviewRule18Coverage.md"]} -->
# Cleanup: Projects — fix existing code that breaks the style guide rules the /code-style-review scanner kinds check

## Context
`Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md` routes
style guide rules that no mechanism enforced to new `/code-style-review`
scanner kinds (`Documents/Plans/ChangeWorkflow/CodeStyleReviewScannerRuleKinds.md`),
and rule 18's top-level `const` locals are the `style-rule-18` kind landed in
c8dde46b. Both report only lines a session adds and leave existing code as it
is (the scanner-kinds Plan's `## Out of scope`), so existing code still breaks
those rules. The user asked for a follow-up that cleans up the existing
rule-breaking code. This Plan does that for first-party C++ under `Projects/`;
sibling Plans cover `Engine/`
(`Documents/Plans/Engine/StyleGuideScannerRuleSweepEngine.md`) and `Common/`,
`DataPacker/` and `Tools/`
(`Documents/Plans/Engine/StyleGuideScannerRuleSweepCommonDataPackerTools.md`).

Rules swept: every `style-rule-<n>` kind the landed scanner reports. That is
the kinds the scanner-kinds Plan adds — 1, 5 (allocation half), 6, 10, 11,
17, 20 (scalar half), 22, 23, 25 (bare half), 26, 30, 33, 34, 35, 36
(scalar half), 37, 39 (`(void)` discard half), 40, 44, 46, 54, 55 and 59, with
the forms its `## Context` lists — and the kinds that predate it, which also
report only added lines: 2, 15, 18 (top-level `const` locals), 19, 27, 28, 29,
32, 41, 50, 52, 57 and 58
(`$script:CandidatePatterns`, `.agents/scripts/Find-SessionCandidates.ps1:45-57`)
and 61 (`Test-Rule61Line`, :148-178). The user decisions the scanner-kinds
Plan records bind here too: rule 26
accepts a `using enum` only when the enum is declared in the file's own header
(the file itself when it is a header; for a `.cpp`, the header declaring the
functions or class it defines), rule 34 needs separators on decimal literals of
1'000 and larger with hexadecimal and binary exempt, and rule 40 covers
read-only text parameters only.

Rule 26 sites under `Projects/` outside the own-header definition, from
`git grep -n "using enum" -- "*.h" "*.cpp" ":!ThirdParty"` at
`1a719473e24b903d7810f80d885122c838c595aa` (3 lines):
- `Projects/BrokenEngineSandbox/Source/Game.cpp:15` (`engine::UiState`,
  declared in `Engine/Source/GameBase.h:43`, not in `Game.h`)
- `Projects/BrokenEngineSandbox/Source/Ui/Localization.h:11` and `:14`
  (`engine::Language` and `engine::StandardString`, declared in
  `Engine/Source/Ui/LocalizationBase.h:7` and `:39`). The comment at
  `Localization.h:9-10` says these re-exports keep game code calling
  `kString...` unqualified; a search at that baseline found no unqualified
  `kString*` or language enumerator use under `Projects/`, so the fix deletes
  both lines and updates that comment to match.

The other 11 `Projects/` `using enum` lines name an enum declared in the
file's own header (for example
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp:21`
with `Players.h:150`, and `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:13`
with `Frame.h:26`) and stay.

Size: an approximate single-line search for the added kinds' forms before
they landed matched about 480 non-comment lines under `Projects/` (an upper
bound before adjudication, not a violation count).

## Design
The author's recommendation:
1. Find candidate sites with the landed scanner's whole-file mode, run exactly
   as `.agents/skills/code-style-review/SKILL.md` `## Inputs` documents it,
   batch by batch: list the files with
   `git ls-files -- "Projects/*.h" "Projects/*.cpp"`, pass one directory's
   files per run, and when a run reports `truncated` `true`, split that batch
   and rerun each part until none does. Every `style-rule-<n>` hit is a
   candidate.
2. Adjudicate each candidate against its rule in `Documents/C++StyleGuide.txt`
   and the rule's adjudication note in
   `.agents/skills/code-style-review/references/worker.md` step 10, exactly as
   a `style-rule-<n>` row is adjudicated there. A line reports only its first
   matching kind, so read the whole line against every swept rule.
3. Fix each accepted site in place when the resulting C++ meaning is
   unchanged, the bound of that worker's step 11. Public signature changes that
   rules 40 and 55 require are in scope when behavior is unchanged: every caller
   is updated in the same change, a `std::string_view` parameter is never
   passed on where null termination is required, and an `enum class` keeps its
   underlying type and enumerator values. A rule 40 parameter stays unchanged
   when the function stores the pointer (for example the
   `ScopedLogDifferenceContext` constructor, `Common/Log/LogDifference.h:11`)
   or any caller can pass null. A rule 6 site is fixed only with an existing
   `kb*` toggle. Rule 26 is fixed by deleting the `using enum` line and
   qualifying each enumerator it served.
4. A site whose fix would change container type or access semantics, class
   layout, control flow, overload resolution, or numeric behavior (worker.md
   step 12), or would need a new `kb*` toggle (rule 6), is not changed; record
   it as a residual with its `path:line` and rule for the session's follow-up
   routing.

Rationale: the landed kinds' own patterns and adjudication notes make the
sweep match what review will enforce on new lines, and the step 11/12 bound
keeps the sweep behavior-preserving.

## Critical files
- First-party C++ under `Projects/` (`*.h`, `*.cpp`)
- `Projects/BrokenEngineSandbox/Source/Game.cpp`
- `Projects/BrokenEngineSandbox/Source/Ui/Localization.h`

## In scope
- Accepted sites of the rules listed in `## Context` in `Projects/**/*.h` and
  `Projects/**/*.cpp`, including the 3 rule 26 lines listed there and the
  `Localization.h:9-10` comment
- Reference updates any of those fixes requires within `Projects/`

## Out of scope
- `ThirdParty/`, shaders, and every non-C++ file
- `Engine/`, `Common/`, `DataPacker/` and `Tools/` (sibling Plans); a fix
  that would need a change there is recorded as a residual instead
- Rules the record routes to the step 7 hand read, `/repo-code-review` or a
  build setting, and every rule not listed in `## Context`
- Any fix outside the step 11 bound, and any rule 6 fix that would need a new
  `kb*` toggle (step 4 of `## Design`)
- `.agents/`, `Documents/C++StyleGuide.txt`, `.clang-tidy`, `.editorconfig`
- Any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 3 (invariant/integration): trigger is a change spanning independently
owned subsystems (`.agents/references/risk-tiers.md`): rule 40 and 55 public
signature changes, with every caller updated, cross the game's subsystems,
each with its own `AGENTS.md` (for example
`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` and
`Projects/BrokenEngineSandbox/Source/Network/AGENTS.md`), with runtime
behavior unchanged. No fix changes a serialized, CRC-covered, wire, save, or
`.pack` type's layout or values. Sim output stays bit-identical: no change to
arithmetic, evaluation order, or float operations. Never embed transcript
paths or home paths.

## Acceptance criteria
- Rerunning step 1's batches reports no unadjudicated candidate in
  `Projects/`: every remaining hit is a permitted form or a recorded residual
- `git grep -n "using enum" -- "Projects/*.h" "Projects/*.cpp"` lists none of
  the 3 rule 26 lines in `## Context`
- `/compile` Client and Server Debug and Release builds pass (rule 6 fixes
  edit configuration-conditional code a Debug build never compiles), plus
  Profile when the sweep edits code conditional on `BT_PROFILE`
- An `/agent-harness` replay determinism check passes

## Notes
Originating record:
`Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md`.
