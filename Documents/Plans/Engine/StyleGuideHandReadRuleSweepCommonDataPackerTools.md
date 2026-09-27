<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T13:29:10.401Z","dependsOn":[]} -->
# Cleanup: Common, DataPacker and Tools — fix existing code that breaks the style guide rules the /code-style-review hand read checks

## Context
`Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md` routes
style guide rules whose violations need type, scope, or several lines of
context to the `/code-style-review` step 7 hand-read list
(`Documents/Plans/ChangeWorkflow/CodeStyleReviewHandReadMandate.md`). That
Plan reviews only session-changed ranges and leaves existing code as it is
(its `## Out of scope`), so code written before it lands still breaks those
rules. The user asked for a follow-up that cleans up the existing
rule-breaking code. This Plan does that for first-party C++ under `Common/`,
`DataPacker/` and `Tools/`; sibling Plans cover `Engine/`
(`Documents/Plans/Engine/StyleGuideHandReadRuleSweepEngine.md`) and
`Projects/` (`Documents/Plans/Game/StyleGuideHandReadRuleSweepProjects.md`).

Rules swept, with the half each hand read covers as that Plan lists it: 4, 5
(locking half), 7, 8, 12, 13, 18 (const-contract half), 20 (class half), 24,
25 (scope half), 29 (no-`virtual` half), 31, 36 (member half), 38, 42, 47
(order half) and 48.

`/code-style-review` accepts a caller-supplied cleanup scope
(`.agents/skills/code-style-review/SKILL.md` `## Inputs`, `Scope`), and its
worker hand-reads the selected ranges for every step 7 rule and auto-fixes
only meaning-preserving violations, routing the rest
(`.agents/skills/code-style-review/references/worker.md` steps 1, 7, 11-12).

Size at `1a719473e24b903d7810f80d885122c838c595aa`: `Common/` 47 tracked
`*.h`/`*.cpp` files and about 5,400 lines, `DataPacker/` 52 files and about
12,400 lines, `Tools/` 18 files and about 5,200 lines.

## Design
The author's recommendation:
1. Dispatch `/code-style-review` once per batch with `Scope` set to the whole
   files of one batch and `Baseline` set to the sweep session's own baseline
   (`.agents/skills/code-style-review/SKILL.md` `## Inputs`), since its worker
   runs the scanner with `-Baseline` on every review (`worker.md` step 8): the
   scanner's hits then cover only the lines the sweep itself added, while the
   hand read covers the whole files. One batch each for `Common/`,
   `DataPacker/` and `Tools/`. Run them one after another, `Common/` first, so
   a rename it propagates is in place before the next batch reads its files.
2. Keep every fix the review applies, including fixes for step 7 rules that
   already had an owner (2, 3, 14, 16, 21, 41, 49, 51, 56, 62): the skill has
   no per-rule filter, and each is a guide violation the review would report on
   any later change to that line.
3. Take each `Routed Findings` row (a fix outside the step 11 bound: container
   type or access semantics, public API, layout, control flow, overload
   resolution, or numeric behavior) as a residual for the session's follow-up
   routing; do not apply it here.
4. Build every target the handoffs' `Build required` fields name.

Rationale: the hand-read rules need judgment a search cannot make, and the
skill that will enforce them on new code already owns the meaning-preserving
fix boundary and rename propagation.

## Critical files
- First-party C++ under `Common/`, `DataPacker/` and `Tools/` (`*.h`, `*.cpp`)
- `.agents/skills/code-style-review/SKILL.md` (invocation contract, read only)

## In scope
- The fixes `/code-style-review` applies in `*.h` and `*.cpp` under `Common/`,
  `DataPacker/` and `Tools/` over the batches in `## Design`
- The C++ and shader reference updates its renames propagate, wherever they
  live

## Out of scope
- `ThirdParty/`, and shader style
- `Engine/` and `Projects/` batches (sibling Plans), except rename propagation
- Routed findings (step 3 of `## Design`); scanner-kind rules (the scanner
  sweep Plans) and `/repo-code-review` rules 9, 53 and 60
  (`Documents/Plans/Engine/StyleGuideRepoReviewRuleSweep.md`)
- `.agents/`, `Documents/C++StyleGuide.txt`, `.clang-tidy`, `.editorconfig`
- Any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 3 (invariant/integration): trigger is a change spanning independently
owned subsystems (`.agents/references/risk-tiers.md`): rule 7 renames in
shared utilities, the asset packer and the tools, propagated to every C++ and
shader reference wherever it lives, reach `Engine/` and `Projects/`, with no
runtime behavior change. The tier also covers a rename that reaches a `.pack`
or `.manifest` name, a serialized field, or build/bootstrap coordination
through the Tools projects. Sim output stays bit-identical. Never embed
transcript paths or home paths.

## Acceptance criteria
- Every batch's `/code-style-review` handoff returns with no unresolved
  residual other than its `Routed Findings`
- `/compile` passes for every target the handoffs name, and at least
  DataPacker, Client and Server in Debug and Release (a rename can reach
  configuration-conditional code a Debug build never compiles), plus Profile
  when the sweep edits code conditional on `BT_PROFILE`, and each Tools
  project whose files changed (DataPacker and both tools build Release only)
- An `/agent-harness` replay determinism check passes

## Notes
Originating record:
`Documents/Investigations/ChangeWorkflow/StyleGuideRuleCoverage.md`.
