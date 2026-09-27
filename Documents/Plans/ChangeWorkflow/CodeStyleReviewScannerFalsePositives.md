<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T19:27:36.781Z","dependsOn":[]} -->
# Fix: /code-style-review scanner — rule 61 attribute and wrapped-condition false positives, and comment/string hits for rules 15, 25, 27

## Context
`.agents/scripts/Find-SessionCandidates.ps1` reports lines that are not
violations for four kinds. The `Projects/` whole-file sweep
(`Documents/Plans/Game/StyleGuideScannerRuleSweepProjects.md`) adjudicated
every such hit as not a violation, one row at a time; the same hits recur in
every session scan and every sibling sweep.

- Rule 61 (31 hits under `Projects/`). `Test-Rule61Line` (:200-230) takes the
  text after the condition's closing `)` as `$rest` and returns `$true` when it
  is not empty and does not start with `{`. A `[[likely]]` or `[[unlikely]]`
  attribute after the condition is therefore reported as a statement, although
  the body is braced on the next line, for example
  `Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientPacketFaultFixture.cpp:133`
  (`if (sArmedPacketFault.empty()) [[likely]]`). A condition whose `(` does
  not close on the `if` line leaves `$rest` empty, so the next line — the
  condition's own continuation — is tested for `{`, `&&` or `||` and reported
  when it starts with an operand, for example
  `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:231`,
  whose line ends in `||` and whose body brace is at :233.
- Rules 15, 25 and 27 match inside comments and string literals. The
  `style-rule-15` (:54), `style-rule-25` (:77) and `style-rule-27` (:57)
  entries have no comment or string exclusion. Examples: rule 15 on `auto` in
  comments (`Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp:26`)
  and in a `LOG` string (`HudScreen.cpp:109`); rule 25 on a comment mentioning
  `constexpr` (`Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.h:4`);
  rule 27 on comment text (`Frame/Collections/Blasters/BlastersUpdate.cpp:183`,
  `W=1.0`) and on `"127.0.0.1"` and `"%.3f"` literals
  (`Projects/BrokenEngineSandbox/Source/Game.cpp:641`, `HudScreen.cpp:347`).
- Rule 50 (:62) reports a pointer-named operand of `sizeof`:
  `Projects/BrokenEngineSandbox/Source/Agent/Commands/RegistryFixture.cpp:106`
  (`if (iScratchBytes > static_cast<int64_t>(sizeof(pScratch)))`) compares two
  integers.

The rules themselves (`Documents/C++StyleGuide.txt` rules 15, 25, 27, 50, 61)
are not in question; only the scanner's candidate matching is.

## Design
Recommended changes, each keeping the kind's real matches:

1. `Test-Rule61Line`: remove a leading `[[likely]]` or `[[unlikely]]` from
   `$rest` before its tests, so a braced body after the attribute is not
   reported, and a statement after the attribute still is. When the
   condition's parentheses do not close on the `if` line, continue the depth
   count over the following head-side lines (`Get-NewSideLine`) until they
   close, then apply today's tests to the text after the closing `)` and to
   the next non-blank line. Recommended over returning `$false` for an
   unclosed condition, which would hide a wrapped condition with an unbraced
   body.
2. Rules 15, 25 and 27: match these three kinds against the line with any
   `//` comment tail removed and string and character literal contents
   blanked, and skip a line that is entirely comment (starting with `//`,
   `/*` or `*`). Recommended over adding `$script:CommentOrQuote` to their
   `Except`, the whole-line exclusion rules 2, 5 and 6 use: `auto` and float
   literals routinely share a line with a trailing comment or a string
   argument, so a whole-line exclusion would drop real hits.
3. Rule 50: add an `Except` for an operand that is the argument of `sizeof(`.

Update the kind-list comment (:33-44) only where it would otherwise misstate
which kinds carry a comment or string exclusion.

## Critical files
- `.agents/scripts/Find-SessionCandidates.ps1`

## In scope
- `Test-Rule61Line`
- The `style-rule-15`, `style-rule-25`, `style-rule-27` and `style-rule-50`
  entries of `$script:CandidatePatterns`, and whatever helper applies the
  comment and literal stripping to those three kinds
- The kind-list comment above `$script:CandidatePatterns`, as
  the last paragraph of `## Design` bounds

## Out of scope
- Every other kind's pattern, `Except`, or order
- `Test-Rule22Line`, `Test-Rule59Line`, the result schema, caps, and the
  session and whole-file line selection
- `.agents/skills/code-style-review/**` and `Documents/C++StyleGuide.txt`
- Permitted forms that are not comment, string, attribute, continuation or
  `sizeof` matches, such as the rule 15 deducing-this accessors; those belong
  to `Documents/Plans/ChangeWorkflow/CodeStyleReviewPermittedForms.md`
- Any C++ source change

## Risk tier and invariants
Expected Change Workflow Tier 2. Trigger: one tool's behavior changes (the
scanner's candidate set) inside one subsystem, with its result format
unchanged (`.agents/references/risk-tiers.md`).

- The result document schema and every other kind's hits are unchanged.
- A rule 61 `if`/`else` with a statement, not a brace, after the condition or
  attribute is still reported, including one whose condition wraps.
- A float literal, `auto`, or non-`static`/`inline` `constexpr` in code on a
  line that also carries a comment or string is still reported.

## Acceptance criteria
- A whole-file run (`-Path`) over the Projects files cited in `## Context`
  no longer reports those cited lines for the cited kind.
- The same run over `Projects/BrokenEngineSandbox/Source` reports no other
  change from the baseline run except the removal of rule 61 attribute and
  wrapped-condition hits and rule 15/25/27 hits inside comments or literals.
- A scratch C++ file with `if (a) [[likely]] return;`, a wrapped condition
  followed by an unbraced statement, `auto x = F(); // note`, and
  `float f = 1.; // 2.0` still reports rule 61 twice, rule 15 once, and
  rule 27 once.

## Notes
The pending sibling sweeps
`Documents/Plans/Engine/StyleGuideScannerRuleSweepEngine.md` and
`Documents/Plans/Engine/StyleGuideScannerRuleSweepCommonDataPackerTools.md`
run this scanner; landing this first shrinks their not-a-violation rows, but
neither order is required. `Documents/Plans/ChangeWorkflow/CodeStyleReviewHandReadMandate.md`
edits only the "complement" sentence of the same kind-list comment. A change
to this script triggers `/external-skill-creator` validation of the
`code-style-review` package.
