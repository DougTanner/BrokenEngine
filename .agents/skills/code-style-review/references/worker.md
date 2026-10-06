# Code Style Review Worker

The numbered steps and the judgment rules for `/code-style-review`. The public
contract main reads is [`../SKILL.md`](../SKILL.md).

## Steps

1. Fix the review scope.
   - When the caller supplies a cleanup scope, use exactly those C++ files and
     ranges; otherwise use the `.cpp` and `.h` ranges changed in this session,
     taken from the implementation handoff and conversation edits.
   - A supplied `Paths` input narrows a session-changed scope to the paths
     under those prefixes.
   - Done when the scope is fixed and stated as session-changed or
     caller-supplied.
2. For a session-changed scope, create `Temp/` if absent, in a call of its
   own: it is gitignored, so a fresh worktree lacks it, and no script creates
   it. Done when `Temp/` exists.
3. For a session-changed scope, derive those ranges from the read-only
   inventory: `pwsh -NoProfile -File
   .agents/scripts/Get-SessionChangeInventory.ps1 -RepositoryRoot '<absolute
   repository toplevel>' -Baseline <full 40-character SHA> -Regions
   -OutputPath Temp/code-style-review-inventory.json`, whose result document
   is one `broken-engine-session-change-inventory/v1` object.
   - When the caller supplied untracked paths, add
     `-IncludeUntracked '<comma-separated paths>'` to that command.
   - When the caller supplied `Paths`, add
     `-PathPrefix '<comma-separated prefixes>'` to that command.
   - With no summary line the ranges are unavailable — report that instead of
     proceeding.
   - Done when that object is in hand or the unavailability is reported.
4. Select the session-changed C++ ranges: the object's `regions` rows whose
   path carries the `class` `cpp` or `dual-language-header` in `entries`. Never
   enumerate these ranges inline. Done when the range list exists without an
   inline enumeration.
5. Confirm the inventory run is usable. Only `status` `pass` is usable; any
   other status means the ranges are unavailable — report that instead of
   proceeding. Done when the status is `pass` or the unavailability is
   reported.
6. Read `Documents/C++StyleGuide.txt`; it is the authority every step-9
   adjudication is decided against. Hand-read the selected ranges for every
   Rule 2 form the narrow scanner does not emit, and in every review for
   rules 3, 4, 7, 8, 12, 13, 14, 16 (including its vector `.at()` clause), 21,
   24, 31, 38, 42, 48, 49, 51, 56, 62, 65, 69 and 71, and for these halves of
   rules split with another owner, except rule 47, whose half is checked by the
   script run its bullet states:
   - rule 5: a mutex or OS lock, such as a critical section or SRW lock,
     locked and unlocked by hand instead of through a RAII lock owner; an OS
     allocation or handle, such as `VirtualAlloc`, `HeapAlloc`, `LocalAlloc`
     or a `HANDLE` closed with `CloseHandle`, released by hand instead of
     through a RAII owner, unless it is step 9's rule 5 permitted form; and
     the guide's scope-guard and `std::unique_ptr` custom deleter clauses;
   - rule 6: a feature toggle — a compile-time switch that turns a feature on
     or off, not every `kb*` constant, since rule 3 gives that prefix to any
     `constexpr bool` — defined as a macro, defined outside the project's
     `Pch.h`, not declared `inline constexpr bool kb*`, or tested with a plain
     `if` instead of `if constexpr`, unless it is one of step 9's rule 6
     permitted forms;
   - rule 9: a `throw` whose operand is not an exception object, such as a
     string, a number, or another non-exception value;
   - rule 11: a functional-notation cast `T(x)` whose `T` is a built-in type or
     a named scalar or enum type (an alias such as `int64_t`, or an enum), not
     a class constructor call, instead of a C++-style cast, unless it is one
     of step 9's rule 11 permitted forms other than the macro argument; the
     `style-rule-11` kind matches only the C-style `(T)x` form and a
     `static_cast<void>` whose operand is not a bare name;
   - rule 15: an `auto` holding an immediately invoked lambda's result, which
     the `style-rule-15` kind clears as a lambda assignment; and a range-based
     `for` structured binding over a range that is neither an associative
     container nor a rule 8 enumeration, which that kind clears as a
     structured binding;
   - rule 17: an integer of a type other than `int64_t` that holds a
     `std::vector` size or indexes a `std::vector`, judged from its use sites,
     when its declaring line names neither `.size()` nor `std::ssize()`; and
     any `std::vector` size obtained with `.size()` or `std::size()` other than
     in an integer declaration or `for` header the `style-rule-17` kind
     matches;
   - rule 18: a read-only reference or pointer parameter, or a range-for
     reference, missing `const`;
   - rule 20: brace initialization of a class that is neither an aggregate nor
     initializer-list constructed, of an enum variable, and of a scalar, enum,
     or pointer member in a constructor's member-initializer list;
   - rule 22: a last initializer-list element without a trailing comma whose
     line ends in `}`, such as a nested braced element or a lambda, or that a
     comment line separates from the closing `}`; the `style-rule-22` kind
     tests neither;
   - rule 25: `static constexpr` at function scope and `inline constexpr` at
     global header scope, not the other way round; and a function-scope
     `constexpr` variable initialized with parentheses, which the
     `style-rule-25` kind skips as a function declaration;
   - rule 27: an `f`-suffixed literal whose destination or other operand is
     `double`; an integer literal whose destination or other operand is
     `float` or `double`; and a floating literal with an exponent but no
     decimal point, such as `1e-20f`;
   - rule 29: an override declared without `virtual` and without `override`;
   - rule 33: the guide's `std::cmp_*` and `std::in_range` clause;
   - rule 35: a path, file, or directory operation through a C runtime call
     that `std::filesystem` provides and the `style-rule-35` kind does not
     list;
   - rule 36: a class member initialized in a `.cpp` file instead of by an
     inline initializer in the header, and the guide's `constinit` clause;
     any declarator without an initializer, whatever its storage duration
     (local, class member, or namespace scope) and whether or not it shares a
     multi-name declaration, whose type — or array element type — is a scalar,
     a pointer, or a class type without a user-provided default constructor
     whose default initialization leaves a member without a default member
     initializer unset, beyond the scanner's one-name scalar-or-pointer form;
     any header namespace-scope variable definition that is not `inline`; and
     a `.cpp` `extern` variable declaration — a global shared between `.cpp`
     files with no header — which rule 36 wants as an `inline` header variable
     with an initializer. Step 9's rule 36 permitted forms, with its
     exceptions, apply to these forms too;
   - rule 37: two reference aliases of the same pair's `first` and `second`
     reached through `->`; one alias alone is not a violation;
   - rule 39: an unused variable or parameter without `[[maybe_unused]]` that
     the warnings-as-errors build
     (`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`
     `## Build Configuration`) does not reject, such as a class-type local, a
     structured binding, or a parameter whose name is removed or commented
     out;
   - rule 40: a string parameter in a spelling the `style-rule-40` kind does
     not match — a `LPCSTR`-family alias, a `const char` or `const wchar_t`
     array parameter, or a non-`const` `std::string&` or `std::wstring&`
     parameter the function only reads — unless it is one of step 9's rule 40
     permitted forms;
   - rule 41: the "always write `std::`" half;
   - rule 47: include grouping and order, not hand read: run
     `pwsh -NoProfile -Command "& '.agents/scripts/Test-IncludeOrder.ps1' -RepositoryRoot '<absolute repository toplevel>' -Path '<file>','<file>' -Fix"`
     once over the distinct paths of the selected `cpp`-class ranges (or the
     caller-supplied scope's files). Only `status` `pass` or `fail` is usable;
     `error` means the include check is unavailable — report that instead of
     using the run. Each rewritten file is one `Fixes Applied` row (Rule 47,
     include block reordered) and adds its owning build targets to
     `Build required`; that build is the rewrite's meaning-preservation proof
     step 10 requires. Each violation the run still reports is
     reported as a residual;
   - rule 50: a handle or pointer that the `style-rule-50` kind's `p`-prefix
     name test misses, such as a `HANDLE`-family or Vulkan handle or a pointer
     named without a `p` prefix, used directly as an `if` or `while`
     condition operand;
   - rule 53: the guide's `append_range`, initializer-list `insert`, and
     `emplace_back` reference clauses; a single initializer-list `insert` is
     one call, so the `reserve` half does not apply to it;
   - rule 57: a function name ending in another suffix that, like `Impl` or
     `Internal`, marks an internal version instead of naming what the
     function does, such as `Inner`, `Detail`, `Helper`, `Ex`, a trailing
     underscore, or a digit that numbers a copy of an existing function name
     (`Foo2` beside `Foo`), which the `style-rule-57` kind does not match;
   - rule 60: a `std::memcpy` between two arrays of the same element type
     instead of `std::copy`;
   - rule 66: for each namespace-scope function (including a `constexpr`
     function or function template) or variable a selected range defines in a
     `.cpp` file (a header definition is never file-local) whose name no
     other file declares — in a header or a `.cpp` file, `extern` or not,
     found with one repository-wide Grep for the name — read the whole file
     for its enclosing scope and report a function or non-const variable
     missing `static`, or a `const`/`constexpr` variable given `static` or
     `inline`; for each file-local type a selected range defines, run one
     repository-wide Grep for a type definition of that name, and for each hit
     in another file check whether a `.vcxproj` compiles both files into one
     executable and the hit is in the same namespace;
   - rule 70: a dead fallback statement in a selected range directly after an
     `ASSERT(false)` the session did not add, which step 10 replaces as step 16
     states; step 16 covers a session-added `ASSERT(false)`.

   The hand-read rules, rule 47 through the script run, step 16's rule 70
   check, and the rules the scanner's `style-rule-<n>` kinds cover are this
   review's whole style mandate; a rule is on two lists when each covers a
   different form.
   Every other guide rule has another owner: `/repo-code-review` owns rule 9's
   control-flow recovery and empty custom exception type parts, rule 67, rule
   53's `reserve` half, rule 60's destination-size half, and rule 47's
   external-header half
   ([`../../repo-code-review/references/checks.md`](../../repo-code-review/references/checks.md)),
   `/comment-review` owns rule 64, and the compiler owns rule 63 (the rotate
   poison in `Common/ExternalHeaders.h`).
   Done when the guide is in hand, the hand read covers every selected range,
   and either the rule 47 script's run has `status` `pass` or `fail` with every
   remaining violation reported as a residual, or its unavailability is
   reported.
7. Run the session-added candidate scanner once: `pwsh -NoProfile -File
   .agents/scripts/Find-SessionCandidates.ps1 -RepositoryRoot '<absolute
   repository toplevel>' -Baseline <full 40-character SHA> -OutputPath
   Temp/code-style-review-candidates.json`,
   - with optional `-Head <commit>` and the `-IncludeUntracked` switch, which
     makes the scanner enumerate every untracked file itself and include those
     files in the scan; pass the switch when the caller supplied any untracked
     path;
   - with `-PathPrefix '<comma-separated prefixes>'` when the caller supplied
     `Paths`.
   - Done when one `broken-engine-session-candidates/v1` object with `hits`
     rows of `path`, `line`, `kind`, and `text`, plus `counts`, is read from
     that file, or the run printed no summary line.
8. Confirm the scan is usable. Only `status` `pass` (exit 0) is usable;
   `blocked` (exit 2), `error` (exit 1), or no summary line from the step-7
   run means both the style candidates and the added-versus-pre-existing
   distinction are unavailable — report that and never reconstruct either scan
   inline. Done when the scan is usable or its unavailability is reported.
9. Adjudicate every `style-rule-<n>` row against rule n of the guide, reading
   the surrounding code; the rows are a starting list, not the finding set. For
   Rule 2, surrounding code must reject declaration-shaped text inside a
   block comment or raw string opened on an earlier line. Rule 29 needs the
   base class, which is off the line, so look it up.
   - These kinds' permitted forms are off the line, so reject a row that is
     one: rule 5, ownership handed straight to a RAII owner; rule 6, a
     preprocessor guard `if constexpr` cannot replace — around an `#include`,
     a namespace-scope declaration, or code naming a symbol declared only in
     that configuration — or a `kb*` definition block in `Pch.h`, or a feature
     toggle in a dual-language header, which GLSL must also read; rule 11, a
     macro argument, a function type, or a functional-notation cast in code of
     a dual-language header that GLSL compiles, since GLSL has no
     `static_cast`; rule 17, a container that is not a
     `std::vector`; rule 22, an enum body
     or a function body, which is not an initializer list; rule 25, a
     namespace-scope `constexpr` variable in a `.cpp` file, since rule 25
     governs only function scope and header global scope; rule 26, an enum
     declared in the file's own header — the file itself when it is a header,
     or for a `.cpp` the header declaring the functions or class it defines;
     rule 27, a literal with both digits and no suffix whose destination or
     other operand is double; rule 36, a class type with a user-provided default
     constructor, an out-parameter filled on the next line, or a static member whose
     initializer names types the header only forward-declares, but the
     class-type-with-constructor rejection does not apply to an `extern` row,
     a `.cpp` `extern` declaration, or a header namespace-scope definition that
     is not `inline`, because rule 36's inline-header-global clause covers
     every type; rule 39,
     a discard of a lambda capture held only for lifetime, in any of the
     `style-rule-39` kind's spellings;
     rule 40, a local, member, cast, or template argument rather than a
     parameter, a parameter type an external callback signature fixes, or a
     `const char*` or `const wchar_t*` parameter that a caller can pass as
     null, or whose value reaches an API that needs null termination; rule 44,
     storage that is not an aligned type; rule 51, one of the established
     multi-line forms the guide keeps; rule 52, a brace that opens a
     function, lambda, type, namespace, or control-statement body, which is
     not an initializer; rule 54, a local or a struct
     member rather than a class member; rule 55, an index-and-count enum.
   - The rows carry their own rule number, so this step covers whatever kinds
     the run emits; step 6's hand read supplies the other hand-read rules'
     findings.
   - Done when every style row is accepted as a finding or rejected.
10. Auto-fix only when the resulting C++ meaning is demonstrably unchanged.
    Examples include whitespace, argument layout, an exact deduced type
    replacing disallowed `auto`, and `NULL` replaced where it is a null pointer
    constant. Done when every applied fix is meaning-preserving.
11. Do not auto-fix a proposed finding that requires changing container type or
    access semantics, public API, class/struct access or layout, control flow,
    overload resolution, or numeric behavior.
    - Report it for caller classification and the applicable domain review.
    - Done when each such finding is listed under `Routed Findings`.
12. Rename an identifier only when it is a meaning-preserving style correction
    and all code references can be propagated, searching the old identifier
    across the repository before editing. Done when that search covers every
    reference.
13. Propagate every reference the rename breaks in C++ and shader sources,
    including references outside the selected ranges. Applying the shader-side
    reference updates is part of the rename. Done when no broken reference to
    the old identifier remains.
14. Route stale `AGENTS.md` references to `/update-claude-docs`, and list
    ordinary documentation and plan references as caller residuals. Done when
    each stale reference is routed or listed.
15. Return the exact affected build targets for every rename; a rename is not
    verified without those builds. Done when `Build required` names those
    targets.
16. Remove confirmed temporary debug instrumentation added during the session,
    including temporary `LOG`, `printf`, `DEBUG_BREAK()`, `assert(false)`,
    `// FIXME`, and `// HACK` lines, taking the added-versus-pre-existing
    distinction from the scanner.
    - When an `assert-false` row is an `ASSERT(false)` kept as an intended
      assertion rather than temporary instrumentation, check the next
      statement against rule 70. `ASSERT` always throws
      (`Common/AGENTS.md:41`), so a fallback `return` there never runs and
      step 10 applies the `std::unreachable()` replacement.
    - Done when a search for their exact text or existing unique debug tag
      returns zero remaining matches in session-added C++, where a kept,
      intended `ASSERT(false)` is not remaining residue.

## Rules

- Run inside one delegated `mechanic`; never delegate. Review C++ only. Style
  review is not a landing gate (a Change Workflow definition).
- Shader style is out of scope; do not review or route it. The only shader
  edits are the reference updates that propagate a C++ rename (steps 12-15).
- Rule 49 forwarding findings are routed, not auto-fixed — see
  `/repo-code-review` (`../../repo-code-review/SKILL.md`).
- The untracked rule differs per script: the step-3 inventory covers an
  untracked file only when `-IncludeUntracked '<comma-separated paths>'` lists it,
  and its `counts.unlistedUntracked` reports how many it did not list; the
  step-7 scanner takes `-IncludeUntracked` as a switch and enumerates the
  untracked files itself.
- Each `-OutputPath` run in steps 3 and 7 writes its result document to
  that file and prints one summary line naming its status, code, and message
  (the headers of `.agents/scripts/Get-SessionChangeInventory.ps1` and
  `.agents/scripts/Find-SessionCandidates.ps1`). Read the file only after that
  run printed the line; no summary line means no result document, so that
  step's result is unavailable.
- Comment content — what a comment says and whether it should exist — is
  `/comment-review` work; this review touches a comment only as the step-16
  residue removal directs.
- Every judgment in steps 9 and 16 stays here, because the scanner's
  contract (`.agents/scripts/Find-SessionCandidates.ps1`) never edits a source
  file and reports candidates only.
- Never add a debug tag merely to defer cleanup, and do not alter pre-existing
  intentional debug logs. Step 16's residue removal deletes only the residue
  lines and touches no other string or non-comment code.
