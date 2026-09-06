<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T23:42:54.254Z","dependsOn":[]} -->
# Clarify and scan ordinary C++ expression wrapping

## Context

The required condition is that `/code-style-review` can enforce the user's
single-line policy for ordinary Boolean assignments and conditions and can
describe a passing review without implying coverage beyond its declared rule
subset. That condition is false today. `Documents/C++StyleGuide.txt:203-214`
keeps function parameters and call arguments on one line, but does not state a
rule for Boolean assignment or condition expressions. The review worker
hand-reads Rule 51 and limits its mandate to the listed hand-read and scanned
rules (`.agents/skills/code-style-review/references/worker.md:36-42`), while the
candidate table has no wrapped-expression kind
(`.agents/scripts/Find-SessionCandidates.ps1:35-54`).

The gap was observed when session-added `const bool` assignments and conditions
continued logical operators onto following lines in
`Engine/Source/Network/Client/ClientReceive.cpp:97-128`; the candidate scan
passed with no style-rule hits and the manual review reported Rule 51 covered.
The current session is correcting those expressions, so this Plan owns only the
separate guide and review-tool coverage debt.

## Design

The user's decision is that ordinary Boolean assignments and conditions remain
on one line instead of using gratuitous logical-continuation lines. Extend the
guide's existing expression-layout policy to state that rule directly while
preserving layouts structurally required by lambdas, braced initializers, and
initializer-list literals.

The author's recommendation is to add a candidates-only scanner classification
for obvious added-line Boolean assignment and condition continuations. Use
limited neighboring-line context around inventory-selected added ranges to
recognize the ordinary `&&` and `||` continuation shapes. Keep human
adjudication authoritative, avoid rewriting expressions in the scanner, and
leave ambiguous constructs to the worker's manual review. Preserve the
scanner's current selected-range, `-Head`, `-IncludeUntracked`, truncation, and
added-versus-pre-existing behavior.

Align the worker's hand-read list and candidate adjudication with the clarified
guide rule. Update the public review contract and worker reporting instructions
only as needed so a passing handoff states that the fixed mandate was reviewed;
it must not represent that result as compliance with every rule in the guide.

## Critical files

- `Documents/C++StyleGuide.txt:203-214` — owning single-line expression-layout
  rule and its examples.
- `.agents/scripts/Find-SessionCandidates.ps1:35-54,152-178,195-201` — candidate
  table, inventory-selected added lines, and per-line classification loop.
- `.agents/skills/code-style-review/references/worker.md:36-65` — hand-read
  coverage, scanner use, and candidate adjudication.
- `.agents/skills/code-style-review/SKILL.md:9-13,31-50` — public fixed-mandate
  and handoff language.

## In scope

- Clarifying the existing guide policy so ordinary Boolean assignments and
  conditions stay on one line, including explicit lambda and braced-initializer
  boundaries.
- Adding an obvious wrapped-expression candidate to
  `Find-SessionCandidates.ps1` without turning it into an auto-fixer or parser.
- Keeping manual review for ambiguous or scanner-incomplete cases and aligning
  the worker's hand-read/candidate coverage with the new guide text.
- Making the public skill and worker handoff language describe a pass as
  complete for the declared fixed mandate rather than for the whole style
  guide.
- Focused positive and negative evidence for assignments, conditions, allowed
  brace/lambda layouts, range selection, and pre-existing lines using the
  repository's existing script-validation route.

## Out of scope

- Reformatting current or pre-existing C++ call sites; the originating session
  owns its current correction.
- Auto-rewriting Boolean expressions, building a C++ parser, or scanning every
  style-guide rule.
- Changing expression meaning, operator precedence, control flow, public C++
  interfaces, product behavior, or build targets.
- Expanding comment review, shader style, or any Change Workflow stage outside
  the existing `/code-style-review` cleanup responsibility.
- Adding unit tests.

## Acceptance criteria

- The guide unambiguously keeps ordinary Boolean assignments and conditions on
  one line and gives enough allowed-layout boundaries to avoid treating
  lambda, braced-initializer, and initializer-list structure as a violation.
- The scanner reports candidates for added ordinary Boolean assignments and
  conditions continued with `&&` or `||`, while focused negative evidence shows
  no candidate for the allowed brace/lambda shapes.
- Candidate results remain limited to inventory-selected added C++ ranges and
  preserve the existing `-Head`, `-IncludeUntracked`, and truncation contracts;
  an equivalent pre-existing-only expression is not reported.
- The worker still adjudicates every candidate against the guide and manually
  reviews any clarified-rule cases the bounded scanner does not cover.
- The skill and worker reporting contract makes the scope of a passing style
  review explicit and does not claim full-guide compliance.
- The repository's script validation and `/validate-skill` pass for the changed
  scanner and skill package, and the required progressive-disclosure review
  finds no duplicated workflow detail.

## Notes

This is a Tier 2 scoped-behavior change because it changes one review tool's
candidate and reporting behavior. It does not touch runtime code, determinism
or CRC state, wire/protocol or serialization formats, threading, allocation,
shader contracts, builds, or live game verification.

The Plan has no dependencies and requires no coordination with another live
Plan. Its implementation still follows the full Change Workflow for changed
documentation, script, and skill artifacts.
