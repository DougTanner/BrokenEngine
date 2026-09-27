# Code Style Review Worker

The numbered steps and the judgment rules for `/code-style-review`. The public
contract main reads is [`../SKILL.md`](../SKILL.md).

## Steps

1. Fix the review scope.
   - When the caller supplies a cleanup scope, use exactly those C++ files and
     ranges; otherwise use the `.cpp` and `.h` ranges changed in this session,
     taken from the implementation handoff and conversation edits.
   - Done when the scope is fixed and stated as session-changed or
     caller-supplied.
2. For a session-changed scope, derive those ranges from the read-only
   inventory: `pwsh -NoProfile -File
   .agents/scripts/Get-SessionChangeInventory.ps1 -RepositoryRoot <absolute
   repository toplevel> -Baseline <full 40-character SHA> -Regions`.
   - It writes no file and prints one
     `broken-engine-session-change-inventory/v1` object.
   - When the caller supplied untracked paths, add
     `-IncludeUntracked <comma-separated paths>` to that command.
   - Done when that object is in hand.
3. Select the session-changed C++ ranges: the object's `regions` rows whose
   path carries the `class` `cpp` or `dual-language-header` in `entries`. Never
   enumerate these ranges inline. Done when the range list exists without an
   inline enumeration.
4. Confirm the inventory run is usable. Only `status` `pass` is usable; any
   other status means the ranges are unavailable — report that instead of
   proceeding. Done when the status is `pass` or the unavailability is
   reported.
5. Confirm the ranges are complete.
   - They are usable only when `truncated` is false; when it is true the run
     emitted a short list, so report the ranges unavailable instead of
     proceeding.
   - Done when `truncated` is false or the unavailability is reported.
6. For a session-changed scope when the `Jev` input is absent, run the
   style-rule judgment once and keep its result: `pwsh -NoProfile -File
   .agents/scripts/Test-StyleRuleJudgment.ps1 -RepositoryRoot <absolute
   repository toplevel> -Baseline <full 40-character SHA> -OutputPath
   Temp/code-style-review-judgment.json`, with the same optional
   `-Head <commit>` and `-IncludeUntracked` switch as step 8; the run sends
   each changed block's text and identifier list to the TypeSafe service
   through `Invoke-Jev.ps1`.
   - Create `Temp/` if absent, in a call of its own before the run: it is
     gitignored, so a fresh worktree lacks it, and the script does not create
     it.
   - The run writes the result document to that file and prints one summary
     line naming its status, code, and message. After this run prints that
     line, read `status` and every `flagged` entry from the file; steps 7 and
     10 take the flagged entries from it, and the script is never run a second
     time in one review.
   - The result is advisory and usable only when `status` is `ok`, including
     `judgment.no-blocks` (zero rows, `Judgment: none`) and `blocks.partial`
     (`Judgment: partial — <message>`; failed blocks: `error`, no `flagged`).
   - Any other status, or no result document at all — the run printed no
     summary line — is recorded as `Judgment: not run — <code>: <message>`
     ([`../SKILL.md`](../SKILL.md) `## Handoff`), with the exception text as
     the message when there is no document, and the review continues.
   - Jev asks about rule 49. The script also emits `rule3` and
     `rule56` entries; ignore them — no `Judgment` row, no candidate — until
     the next test in `Documents/Investigations/JevStyleRuleJudgment.md` is
     run.
   - When the scope is caller-supplied (no baseline) or the `Jev` input is
     `skip`, the script is not run.
   - Done when an `ok` result is in hand, or the `Judgment` row records why
     there is none.
7. Read `Documents/C++StyleGuide.txt`; it is the authority every step-10
   adjudication is decided against. Hand-read the selected ranges for every
   Rule 2 form the narrow scanner does not emit, and in every review for
   rules 3, 4, 7, 8, 12, 13, 14, 16 (including its vector `.at()` clause), 21,
   24, 31, 38, 42, 48, 49, 51, 56 and 62, and for these halves of rules split
   with another owner:
   - rule 5: a mutex locked and unlocked by hand instead of through a RAII
     lock owner;
   - rule 18: a read-only reference or pointer parameter, or a range-for
     reference, missing `const`;
   - rule 20: brace initialization of a class that is neither an aggregate nor
     initializer-list constructed;
   - rule 25: `static constexpr` at function scope and `inline constexpr` at
     global header scope, not the other way round;
   - rule 29: an override declared without `virtual` and without `override`;
   - rule 36: a class member initialized in a `.cpp` file instead of by an
     inline initializer in the header;
   - rule 41: the "always write `std::`" half;
   - rule 47: include grouping and order.

   Step 6's `flagged` entries for rule 49 are extra step-10 candidates; check
   them first. The hand-read rules and the rules the scanner's
   `style-rule-<n>` kinds cover are this review's whole style mandate; a rule
   is on both lists when each covers a different form. Every other guide rule
   has another owner: `/repo-code-review` owns rules 9, 53 and 60 and rule
   47's external-header half
   ([`../../repo-code-review/references/checks.md`](../../repo-code-review/references/checks.md)),
   `/comment-review` owns rule 64, and the compiler owns rules 43 (RTTI off,
   warnings as errors) and 63 (the rotate poison in `Common/ExternalHeaders.h`).
   Done when the guide is in hand, step 6's flagged entries are listed for
   step 10, and the hand read covers every selected range.
8. Run the session-added candidate scanner once: `pwsh -NoProfile -File
   .agents/scripts/Find-SessionCandidates.ps1 -RepositoryRoot <absolute
   repository toplevel> -Baseline <full 40-character SHA>`,
   - with optional `-Head <commit>` and the `-IncludeUntracked` switch, which
     makes the scanner enumerate every untracked file itself and include those
     files in the scan; pass the switch when the caller supplied any untracked
     path.
   - Done when one `broken-engine-session-candidates/v1` object with `hits`
     rows of `path`, `line`, `kind`, and `text`, plus `counts` and `truncated`,
     is in hand.
9. Confirm the scan is usable.
   - Only `status` `pass` (exit 0) is usable; `blocked` (exit 2) or `error`
     (exit 1) means both the style candidates and the added-versus-pre-existing
     distinction are unavailable — report that rather than reconstructing
     either scan inline, and treat `truncated` `true` as hits the run did not
     list.
   - Done when the status is `pass` or the unavailability is reported.
10. Adjudicate every `style-rule-<n>` row and every step-6 flagged entry for
    rule 49 against rule n of the guide, reading the surrounding
    code; the rows and entries are a starting list, not the finding set. For
    Rule 2, surrounding code must reject declaration-shaped text inside a
    block comment or raw string opened on an earlier line. Rule 29 needs the
    base class, which is off the line, so look it up.
    - These kinds' permitted forms are off the line, so reject a row that is
      one: rule 5, ownership handed straight to a RAII owner; rule 6, a
      preprocessor guard `if constexpr` cannot replace — around an `#include`,
      a namespace-scope declaration, or code naming a symbol declared only in
      that configuration — or a `kb*` definition block in `Pch.h`; rule 11, a
      macro argument or a function type; rule 17, a container that is not a
      `std::vector`, or an integer type the value's consumer requires, such as
      a serialized field type or an API parameter type; rule 22, an enum body
      or a function body, which is not an initializer list; rule 25, a
      namespace-scope `constexpr` variable in a `.cpp` file, since rule 25
      governs only function scope and header global scope; rule 26, an enum
      declared in the file's own header — the file itself when it is a header,
      or for a `.cpp` the header declaring the functions or class it defines;
      rule 27, a literal with both digits and no suffix whose destination or
      other operand is double; rule 36, a class type with a constructor, an
      out-parameter filled on the next line, or a static member whose
      initializer names types the header only forward-declares; rule 39, a
      `(void)name;` discard of a lambda capture held only for lifetime;
      rule 40, a local, member, cast, or template argument rather than a
      parameter, a parameter type an external callback signature fixes, or a
      `const char*` or `const wchar_t*` parameter that a caller can pass as
      null, or whose value reaches an API that needs null termination; rule 44,
      storage that is not an aligned type; rule 54, a non-handle `Vk` struct
      type, or a local rather than a member; rule 55, an index-and-count enum.
    - The rows carry their own rule number, so this step covers whatever kinds
      the run emits; step 7's hand read supplies the other hand-read rules'
      findings.
    - Record each step-6 flagged entry for rule 49 as one
      `Judgment` row ([`../SKILL.md`](../SKILL.md) `## Handoff`): `confirmed`
      only when adjudication against rule n of the guide finds the flagged
      construct is a real violation of that rule and the violation involves a
      session-changed line; otherwise `false flag`, including a construct
      the rule does not cover and a real violation in unchanged code. A
      confirmed entry is a finding for steps 11-17 exactly as a
      scanner row is. A violation the hand read finds without a flagged entry
      is an ordinary finding with no `Judgment` row; `Judgment` records only
      the script's flagged entries.
    - Done when every style row and flagged entry is accepted as a finding or
      rejected.
11. Auto-fix only when the resulting C++ meaning is demonstrably unchanged.
    Examples include whitespace, argument layout, an exact deduced type
    replacing disallowed `auto`, and `NULL` replaced where it is a null pointer
    constant. Done when every applied fix is meaning-preserving.
12. Do not auto-fix a proposed finding that requires changing container type or
    access semantics, public API, class/struct access or layout, control flow,
    overload resolution, or numeric behavior.
    - Report it for caller classification and the applicable domain review.
    - Done when each such finding is listed under `Routed Findings`.
13. Rename an identifier only when it is a meaning-preserving style correction
    and all code references can be propagated, searching the old identifier
    across the repository before editing. Done when that search covers every
    reference.
14. Propagate every reference the rename breaks in C++ and shader sources,
    including references outside the selected ranges. Applying the shader-side
    reference updates is part of the rename. Done when no broken reference to
    the old identifier remains.
15. Route stale `AGENTS.md` references to `/update-claude-docs`, and list
    ordinary documentation and plan references as caller residuals. Done when
    each stale reference is routed or listed.
16. Return the exact affected build targets for every rename; a rename is not
    verified without those builds. Done when `Build required` names those
    targets.
17. Remove confirmed temporary debug instrumentation added during the session,
    including temporary `LOG`, `printf`, `DEBUG_BREAK()`, `assert(false)`,
    `// FIXME`, and `// HACK` lines, taking the added-versus-pre-existing
    distinction from the scanner.
    - Done when a search for their exact text or existing unique debug tag
      returns zero remaining matches in session-added C++.

## Rules

- Run inside one delegated `mechanic`; never delegate. Review C++ only. Style
  review is not a landing gate (a Change Workflow definition).
- Shader style is out of scope; do not review or route it. The only shader
  edits are the reference updates that propagate a C++ rename (steps 13-16).
- Rule 49 forwarding findings are routed, not auto-fixed — see
  `/repo-code-review` (`../../repo-code-review/SKILL.md`).
- The untracked rule differs per script: the step-2 inventory covers an
  untracked file only when `-IncludeUntracked <comma-separated paths>` lists it,
  and its `counts.unlistedUntracked` reports how many it did not list; the
  step-8 scanner takes `-IncludeUntracked` as a switch and enumerates the
  untracked files itself.
- Comment content — what a comment says and whether it should exist — is
  `/comment-review` work; this review touches a comment only as the step-17
  residue removal directs.
- Every judgment in steps 10 and 17 stays here, because the scanner's
  contract (`.agents/scripts/Find-SessionCandidates.ps1`) is read-only and
  candidates-only.
- The judgment script's rule 3 threshold and rule 56 name threshold were
  measured against the corpus at
  [`style-rule-judgment/cases.json`](style-rule-judgment/cases.json).
- Never add a debug tag merely to defer cleanup, and do not alter pre-existing
  intentional debug logs. Never touch strings or non-comment code.
