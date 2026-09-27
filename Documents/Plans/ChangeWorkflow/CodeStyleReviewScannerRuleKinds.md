<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T12:47:10.058Z","dependsOn":["Documents/Plans/ChangeWorkflow/CodeStyleReviewRule18Coverage.md"]} -->
# Fix: /code-style-review — add scanner kinds for the unowned pattern-shaped style guide rules

## Context
`Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md` maps every
numbered rule of `Documents/C++StyleGuide.txt` to what enforces it on the agent
Change Workflow path. The rules below have no enforcing mechanism today, or
only a partial one, and each has a violating form that one added line (or an
added line plus a nearby head-side line) shows. The record routes them here.

Current tree:
- `.agents/scripts/Find-SessionCandidates.ps1` `$script:CandidatePatterns`
  (:37-58) emits `style-rule-<n>` kinds for rules 2, 15, 18, 19, 27, 28, 29,
  32, 41, 50, 52, 57 and 58; `Test-Rule61Line` (:148-178, called at :205)
  decides rule 61 from the next head-side line. The table comment (:31-36)
  lists the kinds' relationship to the hand-read list.
- `.agents/skills/code-style-review/references/worker.md` step 10 (:94-110)
  adjudicates every `style-rule-<n>` row against rule n, reading the
  surrounding code; the scanner stays read-only and candidates-only (the
  worker's `## Rules`, :159-161).
- No rule below is covered by the step 7 hand-read list (worker.md :67-76).
  Clang-Tidy and `.editorconfig` settings that touch some of them never run on
  the agent path (`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md:26`).

Rules routed here, with the candidate form each kind reports:
- 1 — an added line indented with a leading space (block-comment continuation
  lines excepted; rule 51 alignment padding follows tabs, so step 10 rejects
  it).
- 5 (allocation half) — raw `new T`, `delete`, `malloc`/`calloc`/`realloc`/
  `free` calls (`= delete` and placement `new (` excepted).
- 6 — `BT_DEBUG`, `BT_RELEASE` or `BT_PROFILE` on an added line, and a
  preprocessor `#if` testing a `kb*` toggle.
- 10 — an `#include` path containing a backslash.
- 11 — a C-style cast: a parenthesized builtin, fixed-width integer, or
  pointer type directly followed by an operand.
- 17 — a non-`size_t` integer declared as a loop counter or index and compared
  with, or initialized from, a `.size()` call.
- 20 (scalar half) — a builtin scalar or fixed-width integer declared with
  brace initialization.
- 22 — a two-line check like `Test-Rule61Line`: an added line inside a
  multi-line brace list that ends without a comma while the next non-blank
  head-side line starts with the closing `}`.
- 23 — `typedef`.
- 25 (bare half) — a `constexpr` variable declaration with neither `static`
  nor `inline` (constexpr functions excepted).
- 26 — every `using enum` declaration (user decision, reading
  `Documents/C++StyleGuide.txt:138`: step 10 accepts it only when the enum is
  declared in the file's own header, as `## Design` defines it). The user chose
  this knowing that about 30 of the 56 current `using enum` lines fall outside
  it (for example `Engine/Source/Graphics/Managers/PipelineManager.cpp:11-12`);
  only added lines are reported, and the existing sites are left to the
  scanner sweep Plans (`Documents/Plans/Engine/StyleGuideScannerRuleSweepEngine.md`,
  `Documents/Plans/Game/StyleGuideScannerRuleSweepProjects.md`,
  `Documents/Plans/Engine/StyleGuideScannerRuleSweepCommonDataPackerTools.md`).
- 30 — `> >` closing nested template argument lists.
- 33 — `limits.h` macros (`INT_MAX`, `UINT_MAX`, `LLONG_MIN`, `CHAR_BIT` and
  their siblings).
- 34 — a decimal literal whose integer part has four or more digits with no
  digit separator (user decision: `1'000` and larger need separators, `999`
  and smaller do not). Hexadecimal and binary literals, fraction and exponent
  digits, and digits inside identifiers, string or character literals, and
  comments are excepted.
- 35 — Win32 or CRT filesystem-namespace calls that `std::filesystem` replaces
  (directory creation and removal, file deletion, attribute and existence
  queries, directory enumeration).
- 36 (scalar half) — a builtin scalar, fixed-width integer, or raw pointer
  declared without an initializer.
- 37 — `std::get<N>(` with a numeric index, and `std::tie(`.
- 39 (discard half) — a `(void)name;` statement silencing an unused name.
- 40 — a read-only text parameter: `const char*`, `const wchar_t*`,
  `const std::string&` or `const std::wstring&` following a `(` or `,` in a
  parameter list (user decision: writable `std::string&` and `std::wstring&`
  parameters, such as the one `Common/WindowsUtils.h:21` changes, are not
  reported).
- 44 — the unaligned `XMLoadFloat2/3/4` and `XMStoreFloat2/3/4` forms (and the
  `3x3`/`4x4` matrix forms) where an `A` variant exists.
- 46 — any DirectXMath call whose name ends in `Est`.
- 54 — a `Vk<Type>` declaration whose name does not end in `Vk<Type>`
  (a back-reference `Except`).
- 55 — an `enum` declaration without `class`.
- 59 — a lookback check: an added `case`/`default:` line whose indent is not
  exactly one tab deeper than the nearest preceding head-side `switch` line
  with a smaller indent.

Rule 18's top-level `const` locals are already the `style-rule-18` kind (:47);
its const-contract half is routed to the hand-read list
(`Documents/Plans/ChangeWorkflow/CodeStyleReviewHandReadMandate.md`).

## Design
Add one `style-rule-<n>` entry per rule above to `$script:CandidatePatterns`,
each with an `Except` for the rule's permitted forms that one line shows. The
author's recommendation:
- Append the single-line entries after `style-rule-58` in rule-number order,
  except where a line that matches an earlier kind would lose a more specific
  hit (a `const auto` line keeps reporting rule 15; a `#ifdef BT_DEBUG` line
  keeps reporting rule 58).
- Decide rules 22 and 59 in two functions shaped like `Test-Rule61Line`,
  called at :205 after the rule 61 check and before `Test-CandidatePattern`,
  because each needs a head-side line other than the added one.
- Update the kind-list comment (:31-36) to name the new kinds and the two new
  functions.
- Add one worker.md step 10 adjudication note for each kind whose permitted
  form is off the line: rule 5 (ownership handed straight to a RAII owner),
  rule 11 (a macro argument or a function type), rule 17 (the container is not
  a `std::vector`), rule 22 (an enum body or a function body is not an
  initializer list), rule 26 (accepted only when the enum is declared in the
  file's own header: the file itself when it is a header, or for a `.cpp` the
  header declaring the functions or class it defines), rule 36 (a class type with a constructor, or an
  out-parameter filled on the next line), rule 44 (the storage is not an
  aligned type), rule 40 (the match is a local, member, cast, or template
  argument rather than a parameter, or an external callback signature fixes
  the parameter type), rule 54 (a non-handle `Vk` struct type, or a local rather
  than a member), rule 55 (an index-and-count enum).
- Keep every entry candidates-only: step 10 decides each row against the
  guide.
- Add a whole-file mode for the scanner sweep Plans, which must list existing
  sites: a `-Path` parameter, in its own parameter set without `-Baseline`,
  `-Head` or `-IncludeUntracked`, naming repo-relative tracked `*.h`/`*.cpp`
  files. It skips the session change inventory and scans every line of each
  named file as the working tree holds it, through `Test-Rule61Line`, the rule
  22 and 59 lookback functions, and `Test-CandidatePattern` limited to the
  `style-rule-<n>` entries (a residue kind such as `log` would otherwise hide a
  style kind on an existing line), with the same hit and output caps and
  `truncated`. A named path that is not a tracked C++ file blocks the run
  (`status` `blocked`, exit 2). Document the mode in
  `.agents/skills/code-style-review/SKILL.md` as a short note closing
  `## Inputs`, since the sweep Plans' implementers run it directly and
  `worker.md` is private to the skill's executor
  (`.agents/references/skill-skeleton.md` `## Section placement`), in the
  array form root `AGENTS.md` prescribes:
  `pwsh -NoProfile -Command "& '.agents/scripts/Find-SessionCandidates.ps1' -RepositoryRoot '<absolute repository toplevel>' -Path '<file>','<file>'"`,
  stating that a review never runs it, that a line reports only its first
  matching kind, and that a caller passes one directory's files per run.
  worker.md step 8 stays the executor's `-Baseline` run.

Rationale: each rule's violating form is visible on one or two lines, so a
scanner kind reports it deterministically, and a worker's hand read cannot
skip it — the failure that let rule 18 pass.

## Critical files
- `.agents/scripts/Find-SessionCandidates.ps1`
- `.agents/skills/code-style-review/references/worker.md`
- `.agents/skills/code-style-review/SKILL.md`

## In scope
- `Find-SessionCandidates.ps1`: new entries in `$script:CandidatePatterns`,
  two new lookback functions for rules 22 and 59 and their call at :205, and
  the kind-list comment at :31-36
- `Find-SessionCandidates.ps1`: the `-Path` whole-file parameter set and its
  scan path, as `## Design` defines them
- `worker.md` step 10: one adjudication note per kind listed in `## Design`
- `SKILL.md` `## Inputs`: a closing note with the whole-file mode's
  invocation and its three stated limits

## Out of scope
- Rule 18 (the existing `style-rule-18` kind), and every rule the record
  routes to the step 7 hand-read list, `/repo-code-review`, or a build setting
- worker.md step 7's hand-read list and its mandate sentence
- `.clang-tidy`, `.editorconfig`, `Documents/C++StyleGuide.txt`
- Sweeping existing violations in unchanged code (the scanner sweep Plans
  named in `## Context`)
- Any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 2 (scoped tool behavior): trigger is a behavior change in a review
skill's candidate scanner. The scanner stays read-only, candidates-only, and
within its hit and output caps. Never embed transcript paths or home paths.

## Acceptance criteria
- For each new kind, a temporary uncommitted scratch edit adding one line (or,
  for rules 22 and 59, the line pair) of the violating form to a tracked C++
  file, scanned with `-Baseline` set to the full SHA of `HEAD` and no `-Head`,
  reports one hit of that kind; revert the scratch edit afterwards
- In the same scratch edit, each permitted form the kind's `Except` names
  produces no hit of that kind
- Run as `SKILL.md` documents it, the whole-file mode on
  `Engine/Source/Graphics/EngineCamera.cpp` reports that file's unchanged
  `#if defined(BT_RELEASE)` line (:15) as `style-rule-6` with no `-Baseline`;
  on one directory's tracked C++ files (a batch as the note documents it)
  after a temporary scratch edit adds more violating lines to one of them than
  the hit cap (`$script:MaximumHits`), it reports `truncated` `true` with the
  output within its caps; and on an untracked or non-C++ path it reports
  `status` `blocked`; revert the scratch edit afterwards
- The session-mode scan of the same scratch edit reports the same hits it did
  before the `-Path` parameter set was added, apart from the new kinds
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
Originating record:
`Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md`.
