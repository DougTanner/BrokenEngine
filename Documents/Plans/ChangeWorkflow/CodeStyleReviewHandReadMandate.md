<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T12:47:15.288Z","dependsOn":["Documents/Plans/ChangeWorkflow/CodeStyleReviewRule18Coverage.md","Documents/Plans/ChangeWorkflow/CodeStyleReviewScannerRuleKinds.md"]} -->
# Fix: /code-style-review — extend the step 7 hand-read list to the judgment-shaped style guide rules

## Context
`Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md` maps every
numbered rule of `Documents/C++StyleGuide.txt` to what enforces it on the agent
Change Workflow path. The rules below have no enforcing mechanism, or only a
partial one, and deciding them needs the type, scope, or several lines of
context that a line pattern cannot see. The record routes them here.

Current tree:
- `.agents/skills/code-style-review/references/worker.md` step 7 (:67-76)
  hand-reads rules 2 (forms the scanner does not emit), 3, 14, 16, 21, 41 (the
  "always write `std::`" half), 49, 51, 56 and 62. Its sentence at :73-74 says
  those rules plus the scanner's `style-rule-<n>` kinds are the review's whole
  style mandate and every other guide rule is outside it, so a rule on neither
  list has no owner in this review and nothing says who owns it.
- `.agents/scripts/Find-SessionCandidates.ps1` :35-36 says the step 7 list "is
  the complement of" the `style-rule-<n>` kinds. It is not: rules 2 and 41 are
  on both lists, and most guide rules are on neither.
- Rules routed here, with the part the hand read covers:
  - 4 — every session-added header starts with `#pragma once`
  - 5 (locking half) — a mutex locked and unlocked by hand instead of through
    a RAII lock owner
  - 7 — case of class, function, variable and template-parameter names
  - 8 — `i`/`j`/`k` counters, a range-based loop where one fits, the C++20
    init-statement form, and `it` for iterators
  - 12 — a duration or time held in a raw number instead of `std::chrono`
    with literal suffixes
  - 13 — integer width choice (`int64_t` default; narrower only for packing,
    unsigned for bitwise work, the API's type at API boundaries)
  - 18 (const-contract half) — a read-only reference or pointer parameter,
    or a range-for reference, missing `const`; the `style-rule-18` kind
    (`.agents/scripts/Find-SessionCandidates.ps1:47`) already reports
    top-level `const` locals
  - 20 (class half) — brace initialization of a class that is neither an
    aggregate nor initializer-list constructed
  - 24 — normalized `XMVECTOR` names end in `Normal` or `Direction`
  - 25 (scope half) — `static constexpr` at function scope and
    `inline constexpr` at global header scope, not the other way round
  - 29 (no-`virtual` half) — a derived-class override declared without
    `virtual` and without `override`; the scanner kind only sees lines that
    carry `virtual`
  - 31 — a runtime `ASSERT` whose condition is a constant expression
  - 36 (member half) — a class member initialized in a `.cpp` file instead of
    by an inline initializer in the header
  - 38 — a range-based loop over an associative container without a
    structured binding
  - 42 — a struct initialized without designated initializers
  - 47 (order half) — include grouping and order; the external-header half is
    already a `/repo-code-review` check
    (`.agents/skills/repo-code-review/references/checks.md:179-181`)
  - 48 — extra braces added only to close a `std::fstream`

The scanner-kinds Plan
`Documents/Plans/ChangeWorkflow/CodeStyleReviewScannerRuleKinds.md` edits the
same comment at :31-36. This Plan depends on it, so its mandate sentence and
comment describe the final kind set.

## Design
The author's recommendation:
- Add the rules above to worker.md step 7's hand-read list, each with the
  half it covers where the rule is split with another owner.
- Rewrite the sentence at :73-74 so no guide rule is left outside every
  owner: this review owns the hand-read rules and the scanner kinds; the rest
  are owned by `/comment-review` (rule 64), `/repo-code-review` (its guide
  contract checks), or the compiler (the RTTI setting, the rotate poison, and
  any warnings promoted to errors). Enumerate the owners from the current
  tree at execution time, including any Plan that has landed since this one
  was written.
- Replace the "complement" claim in the `Find-SessionCandidates.ps1` comment
  (:35-36) with the true relationship: the hand-read list and the kinds
  together make the review's mandate, and a rule may appear in both when each
  covers a different form.

Rationale: these rules need type, scope, or cross-line judgment, so a named
hand-read mandate is the owner that can decide them; the explicit owner
sentence keeps a future unowned rule from going unnoticed.

## Critical files
- `.agents/skills/code-style-review/references/worker.md`
- `.agents/scripts/Find-SessionCandidates.ps1` (comment only)

## In scope
- worker.md step 7: the hand-read list and the mandate sentence at :73-74
- `Find-SessionCandidates.ps1`: the kind-list comment's "complement" sentence
  at :35-36 only

## Out of scope
- `$script:CandidatePatterns` entries and any scanner behavior
- Rules 26, 34 and 40 (scanner kinds in
  `Documents/Plans/ChangeWorkflow/CodeStyleReviewScannerRuleKinds.md`), and
  rule 18's top-level `const` locals (the `style-rule-18` kind)
- `/repo-code-review`, `/comment-review`, `.clang-tidy`, `.editorconfig`, any
  vcxproj, `Documents/C++StyleGuide.txt`
- Sweeping existing violations in unchanged code
- Any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 1 (documentation): trigger is an edit to a review skill's worker prose
and one script comment, with no script behavior change. Never embed transcript
paths or home paths.

## Acceptance criteria
- worker.md step 7 names every rule listed in `## Context`, with its covered
  half where split
- The step 7 mandate sentence names an owner for every numbered guide rule
  outside this review's hand-read list and scanner kinds
- The `Find-SessionCandidates.ps1` comment no longer calls the hand-read list
  the complement of the kinds
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing

## Notes
Originating record:
`Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md`.
