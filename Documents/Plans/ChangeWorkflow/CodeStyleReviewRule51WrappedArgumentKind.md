<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T22:30:44.658Z","dependsOn":[]} -->
# Fix: /code-style-review scanner — add a style-rule-51 kind for wrapped call and declaration arguments

## Context
`Documents/C++StyleGuide.txt` rule 51 (:211) keeps every argument of a call and
every parameter of a declaration on one line, except a lambda or struct literal
argument whose `{` goes on the next line per rule 2.
`.agents/scripts/Find-SessionCandidates.ps1` has no `style-rule-51` kind, and
`Test-StyleRuleJudgment.ps1` leaves rule 51 out on purpose (its header comment,
:9), so rule 51 is covered only by the hand-read list in
`.agents/skills/code-style-review/references/worker.md` step 7 (:67-97, rule 51
at :71). Detection therefore depends on attention alone.

Observed gap: a whole-`Projects/` cleanup sweep ran five `/code-style-review`
batches, each hand-reading 2,000-8,000 lines for about 25 hand-read rules,
rule 51 among them. Afterward a line-based check still found wrapped-argument
calls at these pre-fix line numbers (the fix joined them, so they now point at
other code): `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:99`,
`:105`, `:131`, `:302`, `:339` (`std::tie` member lists),
`Frame/Frame.cpp:685`, `:751` (`LogDifferences(` with its argument on the next
line), `:803` (`XMVectorSubtract`), `Frame/StatusChange.h:97` (`std::tie(`) and
`Graphics/Camera.cpp:69` (a `LOG` call), and
`Ui/Screens/TweaksScreen/TweaksScreen.cpp:26`, `:28` (a designated-initializer
struct literal opened on the call line with its fields wrapped, which rule 51
forbids because a struct-literal argument breaks only with its `{` on the next
line). One batch joined a wrapped `LOG` in `Spaceships.cpp` yet missed the
`Players.h` and `Frame.cpp` sites. The session that found this fixed those
sites, so this Plan covers only the detection gap.

The check that found them: strip the `//` comment; report a line with more `(`
than `)` that ends in `,`, `(`, `&&`, `||` or an arithmetic operator, does not
end in `{`, and whose next line does not start with `{` or `[` (the rule 51
struct-literal and lambda exception). It reported no false positive over all
of `Projects/`.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the gap — the session
`/next-plan-review` must reach — while the `Landing ref` line names a ref whose
tree actually contains this Plan:
- Client: claude
- Conversation session ID: 643f62f7-a135-40b5-ad87-ae65aefadc76
- Worktree/branch UUID: 1870685c-e0be-43e4-b862-62b69c69c821
- Session branch: claude/1870685c-e0be-43e4-b862-62b69c69c821
- Worktree: .claude\worktrees\BrokenEngine\1870685c-e0be-43e4-b862-62b69c69c821
- Landing ref: claude/1870685c-e0be-43e4-b862-62b69c69c821 (the observing
  session records and lands this Plan itself; the branch survives exactly as
  long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CodeStyleReviewRule51WrappedArgumentKind.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
The root cause is in the current tree (no scanner kind, Jev excluded, hand-read
only); the transcript is not needed. The author's recommendation:

1. Add `Test-Rule51Line([string] $Path, [int] $Line, [string] $Text)`, shaped
   like `Test-Rule22Line`: skip a line starting with `#`, `/*` or `*`; blank
   string and character literals and drop the `//` tail as `Test-Rule62Line`
   does; then return `$true` when the remaining code has more `(` than `)`,
   ends in `,`, `(`, `&&`, `||`, `+`, `-`, `*`, `/` or `%`, and the next
   non-blank head-side line starts with neither `{` nor `[`. It needs the next
   line, so it is a function, not a table entry.
2. Decide it after the table, beside `Test-Rule62Line`
   (`if ($null -eq $kind -and ...)`), so it hides no table kind, and add its
   `counts` row. Rationale: this matches the placement already chosen for
   rule 62; a session-added wrapped `LOG` still reports as `log`, which the
   worker reads in full anyway, while a whole-file sweep, which drops the
   residue kinds, reports it as `style-rule-51`.
3. Update the kind-list comment above `$script:CandidatePatterns` to name
   `Test-Rule51Line`.
4. worker.md step 7: move rule 51 from the whole-rule hand-read list to the
   split-rule bullets, keeping by hand only the Boolean-expression half (the
   140-column join, split, and alignment rules for a condition, assignment or
   return), since the scanner covers only unbalanced-parenthesis argument wraps.
5. worker.md step 10: name rule 51's off-line permitted form — a lambda or
   struct-literal argument whose braces the rule 2 exception places on later
   lines — beside the existing per-rule permitted forms.

## Critical files
- `.agents/scripts/Find-SessionCandidates.ps1`
- `.agents/skills/code-style-review/references/worker.md`

## In scope
- A new `Test-Rule51Line` function, its one call after the table, its
  `counts` row, and the kind-list comment above `$script:CandidatePatterns`
- worker.md step 7's rule 51 entry and step 10's permitted-forms list

## Out of scope
- Every other scanner kind's pattern, `Except`, function, or order
- The result schema, caps, and session or whole-file line selection
- `Test-StyleRuleJudgment.ps1` (rule 51 stays out of the Jev questions)
- Detecting the Boolean-expression half of rule 51
- `Documents/C++StyleGuide.txt` and any C++ source change

## Risk tier and invariants
Expected Change Workflow Tier 2. Trigger: one tool's behavior changes — the
candidate set `/code-style-review` receives — inside one subsystem, with the
result format unchanged apart from one new `kind` value
(`.agents/references/risk-tiers.md`).

- No other kind's hits change: the new function runs only when no other kind
  matched the line.
- A call whose only multi-line argument is a lambda or struct literal whose
  `{` opens the next line is not reported.

## Acceptance criteria
- A scratch C++ file holding each shape from `## Context` — a `std::tie(` with
  arguments on the next line, a call ending in `,` after its first argument,
  an `XMVectorSubtract(a,` wrap, a designated-initializer argument opened on
  the call line with its fields wrapped, and a wrapped `LOG(` in a whole-file
  run — reports each as `style-rule-51`; the same file's lambda-argument call
  and `{`-on-next-line struct literal argument report nothing for rule 51.
- A whole-file run (`-Path`) over the tracked `Projects/BrokenEngineSandbox/Source`
  `*.h`/`*.cpp` files reports only `style-rule-51` rows that are rule 51
  violations on reading.
- worker.md step 7 lists only rule 51's Boolean-expression half by hand.

## Notes
`Documents/Plans/ChangeWorkflow/CodeStyleReviewScannerFalsePositives.md` and
`Documents/Plans/ChangeWorkflow/SessionCandidateScannerPrecision.md` also edit
the kind-list comment in the same script, and
`Documents/Plans/ChangeWorkflow/CodeStyleReviewPermittedForms.md` and
`SessionCandidateScannerPrecision.md` also edit worker.md step 10's
permitted-forms list; none shares this Plan's root cause, and any order works.
A change to the script or worker.md triggers `/external-skill-creator`
validation of the `code-style-review` package.
