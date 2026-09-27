<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T13:28:58.910Z","dependsOn":["Documents/Plans/ChangeWorkflow/CodeStyleReviewRule18Coverage.md"]} -->
# Cleanup: Engine — fix existing code that breaks the style guide rules the /code-style-review scanner kinds check

## Context
`Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md` routes
style guide rules that no mechanism enforced to new `/code-style-review`
scanner kinds (`Documents/Plans/ChangeWorkflow/CodeStyleReviewScannerRuleKinds.md`),
and rule 18's top-level `const` locals are the `style-rule-18` kind landed in
c8dde46b. Both report only lines a session adds and leave existing code as it
is (the scanner-kinds Plan's `## Out of scope`), so existing code still breaks
those rules. The user asked for a follow-up that cleans up the existing
rule-breaking code. This Plan does that for first-party C++ under `Engine/`;
sibling Plans cover `Projects/`
(`Documents/Plans/Game/StyleGuideScannerRuleSweepProjects.md`) and `Common/`,
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

Rule 26 sites under `Engine/` outside the own-header definition, from
`git grep -n "using enum" -- "*.h" "*.cpp" ":!ThirdParty"` at
`1a719473e24b903d7810f80d885122c838c595aa` (25 lines; the enum's declaring
header in parentheses):
- `Engine/Source/Graphics/Managers/BufferManager.cpp:11` (`Graphics/Objects/Buffer.h`)
- `Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp:11` (`Graphics/Objects/Texture.h`)
- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp:15` (`Graphics/Objects/Texture.h`)
- `Engine/Source/Graphics/Managers/DynamicPipelines.cpp:12-13` (`Graphics/Objects/Pipeline.h`)
- `Engine/Source/Graphics/Managers/PipelineManager.cpp:11-12` (`Graphics/Objects/Pipeline.h`)
- `Engine/Source/Graphics/Managers/RenderTargetTextures.cpp:12-13` (`Graphics/Objects/Texture.h`)
- `Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp:11-12` (`Graphics/Objects/Texture.h`; the file defines `RenderTargetTextures` members)
- `Engine/Source/Graphics/Managers/TextureManager.cpp:14-15` (`Graphics/Objects/Texture.h`)
- `Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp:12-13` (`Graphics/Objects/Pipeline.h`)
- `Engine/Source/Graphics/Objects/PipelineCreator.cpp:13-14` (`Graphics/Objects/Pipeline.h`; its own header is `PipelineCreator.h`)
- `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp:10-11` (`Graphics/Objects/Pipeline.h`; its own header is `PipelineDescriptorWriter.h`)
- `Engine/Source/Ui/Screens/GameSettingsScreen.cpp:23` (`Ui/LocalizationBase.h`)
- `Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp:46` (`Ui/LocalizationBase.h`)
- `Engine/Source/Ui/Screens/MainMenuScreen.cpp:25-26` (`Ui/LocalizationBase.h`, `GameBase.h`)
- `Engine/Source/Ui/Screens/PauseMenuScreen.cpp:14` (`Ui/LocalizationBase.h`)
- `Engine/Source/Ui/Screens/SoundMenuScreen.cpp:24` (`Ui/LocalizationBase.h`)

The other 15 `Engine/` `using enum` lines name an enum declared in the file's
own header (for example `Engine/Source/Audio/StaticVoice.cpp:10` with
`Engine/Source/Audio/StaticVoice.h:10`, and the collection split files such as
`Engine/Source/Frame/Collections/Billboards/BillboardsRender.cpp:10`, which
define members declared in `Billboards.h`) and stay.

Size: an approximate single-line search for the added kinds' forms before
they landed matched about 950 non-comment lines under `Engine/` (an upper
bound before adjudication, not a violation count), which is why `Engine/` has
its own Plan.

## Design
The author's recommendation:
1. Find candidate sites with the landed scanner's whole-file mode, run exactly
   as `.agents/skills/code-style-review/SKILL.md` `## Inputs` documents it,
   batch by batch: list the files with
   `git ls-files -- "Engine/*.h" "Engine/*.cpp"`, pass one directory's files
   per run, and when a run reports `truncated` `true`, split that batch and
   rerun each part until none does. Every `style-rule-<n>` hit is a candidate.
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
   qualifying each enumerator it served (`PipelineFlags::kX`).
4. A site whose fix would change container type or access semantics, class
   layout, control flow, overload resolution, or numeric behavior (worker.md
   step 12), or would need a new `kb*` toggle (rule 6), is not changed; record
   it as a residual with its `path:line` and rule for the session's follow-up
   routing.

Rationale: the landed kinds' own patterns and adjudication notes make the
sweep match what review will enforce on new lines, and the step 11/12 bound
keeps the sweep behavior-preserving.

## Critical files
- First-party C++ under `Engine/` (`*.h`, `*.cpp`)
- Callers anywhere in first-party C++ of an `Engine/` signature a rule 40 or
  55 fix changes

## In scope
- Accepted sites of the rules listed in `## Context` in `Engine/**/*.h` and
  `Engine/**/*.cpp`, including the 25 rule 26 lines listed there
- Call-site and reference updates any of those fixes requires, wherever the
  referencing first-party C++ lives

## Out of scope
- `ThirdParty/`, shaders (`*.vert`, `*.frag`, `*.comp`, `*.glsl`), and every
  non-C++ file
- `Common/`, `DataPacker/`, `Projects/` and `Tools/` sites (sibling Plans),
  except the reference updates `## In scope` names
- Rules the record routes to the step 7 hand read, `/repo-code-review` or a
  build setting, and every rule not listed in `## Context`
- Any fix outside the step 11 bound, and any rule 6 fix that would need a new
  `kb*` toggle (step 4 of `## Design`)
- `.agents/`, `Documents/C++StyleGuide.txt`, `.clang-tidy`, `.editorconfig`
- Any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 3 (invariant/integration): trigger is a change spanning independently
owned subsystems (`.agents/references/risk-tiers.md`): rule 40 and 55 public
signature changes, with every caller updated wherever it lives, cross the
engine subsystems and reach `Projects/`, with runtime behavior unchanged. No
fix changes a serialized, CRC-covered, wire, or `.pack` type's layout or
values. Sim output stays
bit-identical: no change to arithmetic, evaluation order, or float
operations. Never embed transcript paths or home paths.

## Acceptance criteria
- Rerunning step 1's batches reports no unadjudicated candidate in `Engine/`:
  every remaining hit is a permitted form or a recorded residual
- `git grep -n "using enum" -- "Engine/*.h" "Engine/*.cpp"` lists none of the
  25 rule 26 lines in `## Context`
- `/compile` Client and Server Debug and Release builds pass (rule 6 fixes
  edit configuration-conditional code a Debug build never compiles, such as
  `Engine/Source/Graphics/EngineCamera.cpp:15-19`), plus Profile when the
  sweep edits code conditional on `BT_PROFILE`, and DataPacker when a changed
  file is compiled into it
- An `/agent-harness` replay determinism check passes

## Notes
Originating record:
`Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md`.
