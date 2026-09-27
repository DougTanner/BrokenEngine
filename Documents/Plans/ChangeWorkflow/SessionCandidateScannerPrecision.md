<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T19:34:54.990Z","dependsOn":[]} -->
# Fix: /code-style-review scanner — stop flagging comments, strings, namespace-scope .cpp constexpr and double literals

## Context
`.agents/scripts/Find-SessionCandidates.ps1` emits `style-rule-<n>` candidate
rows that the `/code-style-review` worker adjudicates
(`.agents/skills/code-style-review/references/worker.md` step 10). The
whole-file scanner sweep of `Common/`, `DataPacker/` and `Tools/`
(`Documents/Plans/Engine/StyleGuideScannerRuleSweepCommonDataPackerTools.md`)
had to reject a large share of rows by hand as permitted forms:

1. Rule 27 and `double`. Style guide rule 27 (`Documents/C++StyleGuide.txt`,
   reworded with user approval) appends `f` only to `float` constants; a
   `double` constant is written unsuffixed (`double fSeconds = 1.0;`). The
   `style-rule-27` pattern (`Find-SessionCandidates.ps1:57`) flags every
   unsuffixed decimal literal, and worker.md step 10's permitted-form list
   (`worker.md:100-113`) names no rule 27 form, so a correct `double` literal is
   indistinguishable from a missing `f` in the adjudication rules. The literal's
   type is off the line, so the scanner cannot decide it.
2. Comments and strings. The `style-rule-15`, `-25`, `-27`, `-28` and `-52`
   patterns (`:54`, `:77`, `:57`, `:58`, `:63`) match inside `//` comments and
   string literals. Measured over the final tree of that sweep, 89 of 834
   emitted hits in `Common/`, `DataPacker/` and `Tools/` disappear when
   comments and strings are blanked (rule 27 41, rule 52 23, rule 15 15,
   rule 25 9, rule 28 1). `style-rule-34` (`:83`) already avoids them with a
   prefix that walks the line past string literals, character literals and
   comments.
3. Namespace-scope `constexpr` in `.cpp` files. `style-rule-25` flags a
   column-0 `constexpr` in a `.cpp` file, which worker.md step 10 already
   rejects as a permitted form (rule 25 governs only function scope and header
   global scope); 49 such hits in the same measurement.

## Design
The author's recommendation:
1. worker.md step 10: add to the permitted-form list "rule 27, a decimal
   literal without `f` whose type is `double`", beside the existing rule 25
   form. Leave the rule 27 pattern flagging unsuffixed literals, since a
   `float` literal missing its `f` looks the same on the line.
2. Script: move the `style-rule-34` line-walk prefix into one shared script
   variable, and prefix the `style-rule-15`, `-25`, `-27`, `-28` and `-52`
   patterns with it as `style-rule-34` already is, so only a match in code
   before any comment is reported, and add `style-rule-34`'s block-comment
   continuation exception (`^\s*\*(?:\s|/|$)`) to their `Except`. Where a
   pattern starts with a `^` alternative (rule 27's `(?:^|[^\w.])`), express it
   as a lookbehind so it still applies after the prefix.
3. Script: pass the line's path into `Test-CandidatePattern` (`:261`) and skip
   `style-rule-25` for a `.cpp` path whose line starts with `constexpr` at
   column 0, as `Test-Rule61Line` already receives the path.
4. Update the script's kind-table comment (`:36-44`) only where it would
   otherwise describe the old matching.

Rationale: the shared prefix reuses the one lexical walk the script already
trusts; the path check reuses the existing per-line path; and the `double`
form belongs to the adjudication rules because only the worker can see the
literal's type.

## Critical files
- `.agents/scripts/Find-SessionCandidates.ps1`
- `.agents/skills/code-style-review/references/worker.md`

## In scope
- The `style-rule-15`, `-25`, `-27`, `-28`, `-34` and `-52` entries of
  `$script:CandidatePatterns`, a shared prefix variable, and
  `Test-CandidatePattern` and its one call
- The permitted-form list in worker.md step 10

## Out of scope
- Every other scanner kind, including those that use `$script:CommentOrQuote`
- `Documents/C++StyleGuide.txt`, and the step 7 hand-read list
- Any C++ change

## Risk tier and invariants
Tier 2 (scoped behavior): trigger is one skill's tool behavior — the
candidates `/code-style-review` receives. The scanner's JSON shape, exit codes
and caps stay unchanged, and no hit on code outside comments and strings may be
lost. Skill package changes need `/external-skill-creator` validation.

## Acceptance criteria
- A whole-file scan of the tracked `Common/`, `DataPacker/` and `Tools/`
  `*.h`/`*.cpp` files, compared with a scan by the unchanged script, drops or
  changes the kind of only rows whose old match lay in a comment or string
  literal or was a column-0 `constexpr` line in a `.cpp` file
- worker.md step 10 lists the rule 27 `double` form

## Notes
Originating record: the scanner sweep's adjudication, its coherence review of
the rule 27 rewording, and a researcher measurement over that sweep's final
tree.
