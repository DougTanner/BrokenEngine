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
   style-rule judgment once: `pwsh -NoProfile -File
   .agents/scripts/Test-StyleRuleJudgment.ps1 -RepositoryRoot <absolute
   repository toplevel> -Baseline <full 40-character SHA>`, with the same
   optional `-Head <commit>` and `-IncludeUntracked` switch as step 8; the run
   sends each changed block's text and identifier list to the TypeSafe service
   through `Invoke-Jev.ps1`.
   - The result is usable only when `status` is `ok`, including the
     `judgment.no-blocks` `ok` with zero rows, recorded as `Judgment: none`.
   - Any other status (the script's header names the halting cases), or no
     result document at all, halts the review here: run no later step and
     return the `Status: BLOCKED` handoff with its `Judgment` row
     ([`../SKILL.md`](../SKILL.md) `## Handoff`); the exception text fills
     that row when there is no document.
   - The gated rules are 14, 16 (including its vector `.at()` clause), 21, 49,
     51, 62, and the "always write `std::`" half of 41. The script still emits
     `rule3` and `rule56` entries; ignore them — no `Judgment` row, no
     candidate — until the next test in
     `Documents/Investigations/JevStyleRuleJudgment.md` is run.
   - When the scope is caller-supplied (no baseline) or the `Jev` input is
     `skip`, the script is not run and step 7's fallback applies.
   - Done when an `ok` result is in hand, the fallback is recorded, or the
     BLOCKED handoff is returned.
7. Read `Documents/C++StyleGuide.txt`; it is the authority every step-10
   adjudication is decided against. Hand-read the selected ranges for every
   Rule 2 form the narrow scanner does not emit, and for rules 3 and 56 in
   every review. For the gated rules (step 6), when step 6 returned `ok`, do
   not hand-read: every `flagged` entry for one of them is a step-10
   candidate, and a block the result does not flag is not read for those
   rules. Fallback, applying only when step 6 did not run the script:
   hand-read the selected ranges for the gated rules too.
   Those rules and the rules the scanner's `style-rule-<n>` kinds cover are
   this review's whole style mandate; every other guide rule is outside it.
   Done when the guide is in hand and either the flagged entries are listed
   for step 10 with rules 3 and 56 read, or the fallback has been read across
   every selected range.
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
10. Adjudicate every `style-rule-<n>` row and every step-6 gated-rule flagged
    entry against rule n of the guide, reading the surrounding code; the rows
    and entries are a starting list, not the finding set. For Rule 2,
    surrounding code must reject declaration-shaped text inside a block comment
    or raw string opened on an earlier line. Rule 29 needs the base class, which
    is off the line, so look it up.
    - The rows carry their own rule number, so this step covers whatever kinds
      the run emits; the gated rules arrive as step 6's flagged entries, or
      through step 7's fallback hand read, and rules 3 and 56 arrive from
      step 7's hand read.
    - Record each step-6 gated-rule entry as one `Judgment` row
      ([`../SKILL.md`](../SKILL.md) `## Handoff`); a confirmed entry is a
      finding for steps 11-17 exactly as a scanner row is. A
      gated-rule violation seen while adjudicating a flagged block for a
      different rule is an ordinary finding with no `Judgment` row; `Judgment`
      records only the script's gated-rule entries.
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
- The judgment script's default thresholds were measured against the corpus
  at [`style-rule-judgment/cases.json`](style-rule-judgment/cases.json).
- A step-6 halt is never bypassed inside the worker.
- Never add a debug tag merely to defer cleanup, and do not alter pre-existing
  intentional debug logs. Never touch strings or non-comment code.
