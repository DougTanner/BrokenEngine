# C++ Style Guide Rule Coverage

## Scope and method

Findings record for every numbered rule of `Documents/C++StyleGuide.txt`: 63
rules, 1 through 64, with no rule 45. Each row lists every mechanism that
touches the rule, at baseline `3a7a24829b22d2658d27742b9595ba64b04d1a25`.

A mechanism counts as enforcement only when it runs on the agent Change
Workflow path for session-changed C++ and can fail or produce a finding:
- a `.agents/scripts/Find-SessionCandidates.ps1` `style-rule-<n>` kind;
- a `/code-style-review` step 7 hand-read mandate;
- a `/comment-review` or `/repo-code-review` mandate;
- a compiler error or poison under a default `/compile` build.

These are listed as `recorded:` and never count as enforcement:
- Jev style-rule judgment, which is advisory: the review ignores its rule 3
  and rule 56 entries (`.agents/skills/code-style-review/references/worker.md:58-61`)
  and takes its rule 49 entries only as extra candidates
  (`.agents/skills/code-style-review/references/worker.md:96-97`, `:127-128`);
- a `.clang-tidy` check, because `/compile` force-disables Clang-Tidy
  (`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md:26`);
- an `.editorconfig` setting, which only an editor's formatter applies;
- a compiler warning that fails only a build configuration the default agent
  path does not build (`/compile` builds Client and Server in Debug unless
  asked, `.agents/skills/compile/references/worker.md:117-119`).

Status: `enforced` (every part of the rule has an enforcing mechanism),
`partial` (the uncovered part is named), or `unowned`. A rule split across two
enforcing mechanisms that together cover it is `enforced`.

Totals: 22 enforced, 5 partial, 36 unowned. The scanner kinds and the step 7
hand-read list together cover 22 rules; 41 rules are on neither list.

## Coverage

Routed-to paths (Plans since landed):
- Scanner kinds: `Documents/Plans/ChangeWorkflow/CodeStyleReviewScannerRuleKinds.md`
- Hand read: `Documents/Plans/ChangeWorkflow/CodeStyleReviewHandReadMandate.md`
- Repo review: `Documents/Plans/ChangeWorkflow/RepoCodeReviewStyleGuideContracts.md`
- Build warnings: `Documents/Plans/Game/SandboxDebugProfileGuideWarningsAsErrors.md`

| Rule | Subject | Current mechanisms | Status | Recommendation and reason | Routed to |
|---|---|---|---|---|---|
| 1 | Tab indentation | recorded: `.editorconfig:9` | unowned | Scanner kind: a leading-space indent shows on one line | Scanner kinds |
| 2 | Brace placement | `.agents/scripts/Find-SessionCandidates.ps1:45`; `.agents/skills/code-style-review/references/worker.md:67-68` (forms the scanner does not emit); recorded: `.editorconfig:34-38` | enforced | none | none |
| 3 | Hungarian notation | `.agents/skills/code-style-review/references/worker.md:69`; recorded: `.agents/scripts/Test-StyleRuleJudgment.ps1:50`, `.clang-tidy:26` (deferred) | enforced | none | none |
| 4 | `#pragma once` | none | unowned | Hand read: a missing line in a new header is not a line pattern | Hand read |
| 5 | RAII | recorded: `.agents/skills/repo-code-review/references/checks.md:25-26` (traces cleanup correctness, does not require RAII) | unowned | Scanner kind for raw allocation calls, which show on one line; hand read for manual lock and unlock, which needs the lock object's type | Scanner kinds; Hand read |
| 6 | Feature toggles, no config macros | none | unowned | Scanner kind: configuration macro names and a preprocessor test of a `kb` toggle show on one line | Scanner kinds |
| 7 | Camel case | none | unowned | Hand read: case depends on whether a name is a class, function, variable, or template parameter | Hand read |
| 8 | Loop variable names and range-based loops | none | unowned | Hand read: whether a range-based loop fits is a judgment | Hand read |
| 9 | Exceptions for fatal errors only | none (`.agents/skills/repo-code-review/references/checks.md:54-55` says no universal throw policy) | unowned | `/repo-code-review` check: control-flow use needs the catch site, a correctness contract | Repo review |
| 10 | `/` in include paths | recorded: `.editorconfig:90` | unowned | Scanner kind: a backslash in an `#include` shows on one line | Scanner kinds |
| 11 | C++-style casts | recorded: `.clang-tidy:34` | unowned | Scanner kind: a parenthesized type before an operand shows on one line | Scanner kinds |
| 12 | `std::chrono` durations | none | unowned | Hand read: whether a number holds a time needs its meaning | Hand read |
| 13 | Integer width choice | none | unowned | Hand read: the width depends on packing and API context | Hand read |
| 14 | Count, not Num | `.agents/skills/code-style-review/references/worker.md:69` | enforced | none | none |
| 15 | `auto` restrictions | `.agents/scripts/Find-SessionCandidates.ps1:46` | enforced | none | none |
| 16 | No map `operator[]`; vector `.at()` | `.agents/skills/code-style-review/references/worker.md:69` | enforced | none | none |
| 17 | `int64_t` and `std::ssize()` for vector sizes and indices | none | unowned | Scanner kind: any integer counter against `.size()`, or a non-`int64_t` counter against `std::ssize()`, shows on one line | Scanner kinds |
| 18 | `const` contracts; no top-level `const` locals | `.agents/scripts/Find-SessionCandidates.ps1:47` (top-level `const` locals, landed in c8dde46b); recorded: `.clang-tidy:25` (deferred, pushes the opposite way), `.clang-tidy:42` | partial: a read-only reference or pointer parameter, or range-for reference, missing `const` | Hand read: whether a reference or pointee is modified needs the function body | Hand read |
| 19 | `typename` and upper-case template parameters | `.agents/scripts/Find-SessionCandidates.ps1:48` | enforced | none | none |
| 20 | Uniform initialization only where allowed | none | unowned | Scanner kind for brace-initialized scalars, which show on one line; hand read for class types, which needs the type | Scanner kinds; Hand read |
| 21 | `std::span` parameters; no `std::bitset` | `.agents/skills/code-style-review/references/worker.md:69` | enforced | none | none |
| 22 | Trailing comma in initializer lists | none | unowned | Scanner two-line check shaped like `.agents/scripts/Find-SessionCandidates.ps1:148-178`: the last list line and the closing brace | Scanner kinds |
| 23 | `using`, not `typedef` | recorded: `.clang-tidy:41` | unowned | Scanner kind: `typedef` shows on one line | Scanner kinds |
| 24 | `Normal`/`Direction` suffix | none | unowned | Hand read: whether a vector is normalized is semantic | Hand read |
| 25 | `static constexpr` / `inline constexpr` | none | unowned | Scanner kind for a bare `constexpr` variable; hand read for the scope choice, which needs the enclosing scope | Scanner kinds; Hand read |
| 26 | Limited `using enum` | none | unowned | Scanner kind reporting every `using enum`, accepted only when the enum is declared in the file's own header (user decision on `Documents/C++StyleGuide.txt:138`, made knowing about 30 of the 56 current uses fall outside it; only added lines are reported) | Scanner kinds |
| 27 | Float literal form | `.agents/scripts/Find-SessionCandidates.ps1:49` | enforced | none | none |
| 28 | `nullptr` | `.agents/scripts/Find-SessionCandidates.ps1:50`; recorded: `.clang-tidy:39` | enforced | none | none |
| 29 | `override` | `.agents/scripts/Find-SessionCandidates.ps1:51` (only lines carrying `virtual`), adjudicated at `.agents/skills/code-style-review/references/worker.md:131-132`; recorded: `.clang-tidy:40` | partial: an override declared without `virtual` and without `override` is never a candidate | Hand read: the missing form needs the base class | Hand read |
| 30 | No space in `>>` | none | unowned | Scanner kind: `> >` shows on one line | Scanner kinds |
| 31 | `static_assert` where possible | recorded: `.clang-tidy:37`, `.agents/skills/repo-code-review/references/checks.md:90-106` (judges an assertion's value, not compile-time eligibility) | unowned | Hand read: whether a condition is a constant expression needs its types | Hand read |
| 32 | `std::unordered_map` | `.agents/scripts/Find-SessionCandidates.ps1:52` | enforced | none | none |
| 33 | `std::numeric_limits`, not `limits.h` | none | unowned | Scanner kind: the macro names show on one line | Scanner kinds |
| 34 | Digit separators | none | unowned | Scanner kind for an unseparated decimal literal of four or more digits; hexadecimal literals exempt (user decision) | Scanner kinds |
| 35 | `std::filesystem` | none | unowned | Scanner kind: the Win32 and CRT filesystem calls show on one line | Scanner kinds |
| 36 | Always initialize; inline member initializers | none | unowned | Scanner kind for a scalar or pointer declared without an initializer; hand read for members initialized in a `.cpp`, which needs the class | Scanner kinds; Hand read |
| 37 | Structured bindings for tuple returns | none | unowned | Scanner kind: `std::get<N>` and `std::tie` show on one line | Scanner kinds |
| 38 | Structured bindings over associative containers | none | unowned | Hand read: the container type is off the line | Hand read |
| 39 | `[[maybe_unused]]` | `DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj:215` (Release-only target, warnings as errors); recorded: `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj:248`, `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj:246` (Release only), `.clang-tidy:38` | partial: Sandbox Debug and Profile builds, and a `(void)` discard that silences the warning | Promote the unused-name warnings to errors in Sandbox Debug and Profile, since the compiler already decides the rule; scanner kind for the `(void)` form | Build warnings; Scanner kinds |
| 40 | `std::string_view` parameters | none | unowned | Scanner kind for read-only text parameters only; writable string references are not reported (user decision) | Scanner kinds |
| 41 | Explicit `std::` | `.agents/scripts/Find-SessionCandidates.ps1:53`; `.agents/skills/code-style-review/references/worker.md:84` (the "always write `std::`" half) | enforced | none | none |
| 42 | Designated initializers | none | unowned | Hand read: whether the type is a struct is off the line | Hand read |
| 43 | No RTTI | `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj:131`, `:185`, `:252` and `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj:129`, `:183`, `:250` (RTTI off); `DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj:219` (RTTI off) with `:215` (Release-only target, warnings as errors) | partial: with RTTI off, `dynamic_cast` and `typeid` on a polymorphic type still compile with warning C4541, an error only in Release | Promote C4541 to an error in Sandbox Debug and Profile | Build warnings |
| 44 | Aligned DirectXMath | recorded: `.agents/references/cpp-conventions.md:9` (guidance, no review step) | unowned | Scanner kind: the unaligned load and store names show on one line; step 10 checks the storage type | Scanner kinds |
| 46 | No `Est` DirectXMath functions | none | unowned | Scanner kind: an `Est` suffix shows on one line | Scanner kinds |
| 47 | Include order | `.agents/skills/repo-code-review/references/checks.md:179-181` (external headers go in `Common/ExternalHeaders.h`); recorded: `.editorconfig:86-88` | partial: grouping and order within the file | Hand read: order needs the whole include block | Hand read |
| 48 | Close `std::fstream`, no extra braces | none | unowned | Hand read: the purpose of a scope is a judgment | Hand read |
| 49 | Public state; no trivial accessors | `.agents/skills/code-style-review/references/worker.md:70`; `.agents/skills/repo-code-review/references/checks.md:165-172`; recorded: `.agents/scripts/Test-StyleRuleJudgment.ps1:55` | enforced | none | none |
| 50 | Pointer tests compare with `nullptr` | `.agents/scripts/Find-SessionCandidates.ps1:54`; recorded: `.clang-tidy:45` | enforced | none | none |
| 51 | One-line parameters and arguments; Boolean wrapping | `.agents/skills/code-style-review/references/worker.md:70` | enforced | none | none |
| 52 | Space before `{}` | `.agents/scripts/Find-SessionCandidates.ps1:55`; recorded: `.editorconfig:56` | enforced | none | none |
| 53 | `reserve` before repeated appends | none | unowned | `/repo-code-review` check: the loop count and container type need tracing | Repo review |
| 54 | `Vk<Type>` member suffix | none | unowned | Scanner kind: the declared type and name show on one line | Scanner kinds |
| 55 | `enum class` | none | unowned | Scanner kind: an `enum` without `class` shows on one line; step 10 exempts index-and-count enums | Scanner kinds |
| 56 | Complete words | `.agents/skills/code-style-review/references/worker.md:70`; recorded: `.agents/scripts/Test-StyleRuleJudgment.ps1:62`, `:295` | enforced | none | none |
| 57 | No `Impl`/`Internal` suffixes | `.agents/scripts/Find-SessionCandidates.ps1:56` | enforced | none | none |
| 58 | `#if defined()` | `.agents/scripts/Find-SessionCandidates.ps1:57`; recorded: `.clang-tidy:36` | enforced | none | none |
| 59 | Indented `case` | recorded: `.editorconfig:25-26` | unowned | Scanner lookback check: an added `case` line against the nearest enclosing `switch` line | Scanner kinds |
| 60 | `std::memcpy` size gated on the destination | none | unowned | `/repo-code-review` check: the destination size needs tracing | Repo review |
| 61 | Braces on every `if`/`else` body | `.agents/scripts/Find-SessionCandidates.ps1:148-178`, `:205` | enforced | none | none |
| 62 | Early returns, one guard per `if` | `.agents/skills/code-style-review/references/worker.md:70` | enforced | none | none |
| 63 | No raw DirectXMath rotate | `Common/ExternalHeaders.h:447-448` (compile poison) | enforced | none | none |
| 64 | Comment content | `.agents/skills/comment-review/SKILL.md:6`, `:19` | enforced | none | none |

## Observations

- `.agents/scripts/Find-SessionCandidates.ps1:35-36` calls the step 7
  hand-read list the complement of the scanner kinds. Rules 2 and 41 are on
  both lists and 41 rules are on neither. The Hand read Plan corrects the
  comment and the mandate sentence at
  `.agents/skills/code-style-review/references/worker.md:97-99`.
- `.editorconfig:26` sets `cpp_indent_case_labels = false`, which appears to
  put `case` labels level with `switch`, against rule 59; current code
  indents them, and `.editorconfig` does not count as enforcement here.
- Rule 59's `switch` example sits under rule 60 in the guide
  (`Documents/C++StyleGuide.txt:280-286`).
- The routed Plans check only new lines. Sweeps for existing violations,
  each run after the Plan that adds its check:
  scanner kinds and rule 18 (landed) —
  `Documents/Plans/Engine/StyleGuideScannerRuleSweepEngine.md`,
  `Documents/Plans/Game/StyleGuideScannerRuleSweepProjects.md`,
  `Documents/Plans/Engine/StyleGuideScannerRuleSweepCommonDataPackerTools.md`;
  hand read — [`/sweep` `style-guide` type](../../../.agents/skills/sweep/references/types/style-guide.md);
  repo review (landed) — `Documents/Plans/Engine/StyleGuideRepoReviewRuleSweep.md`.
  The Build warnings Plan needs no sweep: it already fixes the existing sites
  its promoted warnings report.

## Decisions needed

None remain. The user answered rules 26, 34 and 40; their rows record the
chosen reading and route to the Scanner kinds Plan.
