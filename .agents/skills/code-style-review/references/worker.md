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
6. Read `Documents/C++StyleGuide.txt`; it is the authority every step-9
   adjudication is decided against. Hand-read the selected ranges for the rules
   the scanner does not cover: 3, 14, 16 (including its vector `.at()` clause),
   21, 49, 51, 56, 61, 62, and the "always write `std::`" half of 41. Those
   hand-read rules and the rules the scanner's `style-rule-<n>` kinds cover are
   this review's whole style mandate; every other guide rule is outside it.
   Done when the guide is in hand and those rules have been read across every
   selected range.
7. Run the session-added candidate scanner once: `pwsh -NoProfile -File
   .agents/scripts/Find-SessionCandidates.ps1 -RepositoryRoot <absolute
   repository toplevel> -Baseline <full 40-character SHA>`,
   - with optional `-Head <commit>` and the `-IncludeUntracked` switch, which
     makes the scanner enumerate every untracked file itself and include those
     files in the scan; pass the switch when the caller supplied any untracked
     path.
   - Done when one `broken-engine-session-candidates/v1` object with `hits`
     rows of `path`, `line`, `kind`, and `text`, plus `counts` and `truncated`,
     is in hand.
8. Confirm the scan is usable.
   - Only `status` `pass` (exit 0) is usable; `blocked` (exit 2) or `error`
     (exit 1) means both the style candidates and the added-versus-pre-existing
     distinction are unavailable — report that rather than reconstructing
     either scan inline, and treat `truncated` `true` as hits the run did not
     list.
   - Done when the status is `pass` or the unavailability is reported.
9. Adjudicate every `style-rule-<n>` row against rule n of the guide, reading
   the surrounding code; the rows are a starting list, not the finding set.
   Rule 29 needs the base class, which is off the line, so look it up.
   - The rows carry their own rule number, so this step covers whatever kinds
     the run emits; the mandate's remaining rules are hand-read in step 6.
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
    - Done when a search for their exact text or existing unique debug tag
      returns zero remaining matches in session-added C++.

## Rules

- Run inside one delegated `mechanic`; never delegate. Review C++ only. Style
  review is not a landing gate (defined in root `AGENTS.md`).
- Shader style is out of scope; do not review or route it. The only shader
  edits are the reference updates that propagate a C++ rename (steps 12-15).
- Rule 49 forwarding findings are routed, not auto-fixed — see
  `/repo-code-review` (`../../repo-code-review/SKILL.md`).
- The untracked rule differs per script: the step-2 inventory covers an
  untracked file only when `-IncludeUntracked <comma-separated paths>` lists it,
  and its `counts.unlistedUntracked` reports how many it did not list; the
  step-7 scanner takes `-IncludeUntracked` as a switch and enumerates the
  untracked files itself.
- Comment content — what a comment says and whether it should exist — is
  `/comment-review` work; this review touches a comment only as the step-16
  residue removal directs.
- Every judgment in steps 9 and 16 stays here, because the scanner's
  contract (`.agents/scripts/Find-SessionCandidates.ps1`) is read-only and
  candidates-only.
- Never add a debug tag merely to defer cleanup, and do not alter pre-existing
  intentional debug logs. Never touch strings or non-comment code.
