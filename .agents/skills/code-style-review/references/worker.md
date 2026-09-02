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
   - Done when that object is in hand.
3. Select the session-changed C++ ranges: the object's `regions` rows whose
   path carries the `class` `cpp` or `dual-language-header` in `entries`. Never
   enumerate these ranges inline. Done when the range list exists without an
   inline enumeration.
4. Confirm the inventory run is usable. Only `status` `pass` (exit 0) is
   usable; `blocked` (exit 2) or `error` (exit 1) means the ranges are
   unavailable — report that instead of proceeding. Done when the status is
   `pass` or the unavailability is reported.
5. Confirm the ranges are complete.
   - They are usable only when `truncation.entries` and `truncation.regions`
     each report an emitted count equal to the full count; if either falls
     short report the ranges unavailable instead of proceeding.
   - Done when both counts match or the unavailability is reported.
6. Read `Documents/C++StyleGuide.txt` first; it is authoritative. Inspect every
   applicable rule, not only the grep-friendly examples in step 7. Done when
   the applicable rules are in hand.
7. Search the selected ranges for violations.
   - High-value checks include:
     - Hungarian notation and complete names (Rules 3, 14, 56, 57);
     - `auto`, template, float, null, and override rules (15, 19, 27-29);
     - container access and types (16, 21, 32);
     - namespace and member access rules (41, 49);
     - pointer conditions, argument layout, initializers, preprocessor form,
       braces, and early-return guards (50-52, 58, 61, 62);
     - comments describing the present code rather than change history (64).
   - Done when every selected range has been searched.
8. Auto-fix only when the resulting C++ meaning is demonstrably unchanged.
   Examples include whitespace, argument layout, an exact deduced type replacing
   disallowed `auto`, and `NULL` replaced where it is a null pointer constant.
   Done when every applied fix is meaning-preserving.
9. Do not auto-fix a proposed finding that requires changing container type or
   access semantics, public API, class/struct access or layout, control flow,
   overload resolution, or numeric behavior.
   - Report it for caller classification and the applicable domain review.
   - Done when each such finding is listed under `Routed Findings`.
10. Rename an identifier only when it is a meaning-preserving style correction
    and all code references can be propagated, searching the old identifier
    across the repository before editing. Done when that search covers every
    reference.
11. Propagate every reference the rename breaks in C++ and shader sources,
    including references outside the selected ranges. Applying the shader-side
    reference updates is part of the rename. Done when no broken reference to
    the old identifier remains.
12. Route stale `AGENTS.md` references to `/update-claude-docs`, and list
    ordinary documentation and plan references as caller residuals. Done when
    each stale reference is routed or listed.
13. Return the exact affected build targets for every rename; a rename is not
    verified without those builds. Done when `Build required` names those
    targets.
14. Run the session-added residue scanner: `pwsh -NoProfile -File
    .agents/scripts/Find-SessionDebugResidue.ps1 -RepositoryRoot <absolute
    repository toplevel> -Baseline <full 40-character SHA>`,
    - with optional `-Head <commit>` and the `-IncludeUntracked` switch, which
      makes the scanner enumerate every untracked file itself and include those
      files in the scan.
    - Done when one `broken-engine-session-debug-residue/v1` object with `hits`
      rows of `path`, `line`, `kind`, and `text`, plus `counts` and
      `truncated`, is in hand.
15. Confirm the scan is usable.
    - Only `status` `pass` (exit 0) is usable; `blocked` (exit 2) or `error`
      (exit 1) means the session-added distinction is unavailable — report it
      rather than reconstructing these scans inline, and treat `truncated`
      `true` as hits the run did not list.
    - Done when the status is `pass` or the unavailability is reported.
16. Remove confirmed temporary debug instrumentation added during the session,
    including temporary `LOG`, `printf`, `DEBUG_BREAK()`, `assert(false)`,
    `// FIXME`, and `// HACK` lines, taking the added-versus-pre-existing
    distinction from the scanner.
    - Done when a search for their exact text or existing unique debug tag
      returns zero remaining matches in session-added C++.
17. In selected C++ comments, remove `AGENTS.md` or `CLAUDE.md` navigation text
    only when the remaining technical statement stays complete, taking the same
    added-versus-pre-existing distinction from the scanner's hits of that kind.
    - Delete a comment whose sole content is the pointer; otherwise preserve
      its technical content and repair punctuation.
    - Done when every such hit is deleted or preserved.
18. Classify the selected changed comments by
    [`comment-classification.md`](comment-classification.md). Done when each
    one is preserved, rewritten, or routed as that reference directs.

## Rules

- Run inside one delegated `mechanic`; never delegate. Review C++ only. Style
  review is not a landing gate (defined in root `AGENTS.md`).
- Shader style is out of scope; do not review or route it. The only shader
  edits are the reference updates that propagate a C++ rename (steps 10-13).
- Rule 15 permits `auto` for XMVECTOR/XMMATRIX results, a type obvious from a
  template parameter on the right, iterators, structured bindings, and a
  lambda expression assigned directly to the variable. It remains forbidden
  in plain range-based loops.
- Rule 49 forwarding findings are routed, not auto-fixed — see
  `/repo-code-review` (`../../repo-code-review/SKILL.md`).
- The inventory's entries list is capped at 500 rows and the regions table at
  400, and the ranges are derived from both, which is why step 5 reads
  `truncated`. An untracked file appears only when the caller supplies it with
  `-IncludeUntracked <comma-separated paths>`, and `counts.unlistedUntracked`
  reports how many untracked files the run did not list.
- The residue scanner scans added lines only and reports candidates only: it
  never edits a file, never decides whether a hit is temporary or intentional,
  and never writes to disk, so every judgment and removal in steps 16-18 stays
  here.
- Never add a debug tag merely to defer cleanup, and do not alter pre-existing
  intentional debug logs. Never touch strings or non-comment code.
