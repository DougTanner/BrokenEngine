# Verify External Claims Worker

The identifier, delegation, and result-handling steps, and the rules the runner
applies. The request contract and the report form live in
[`../SKILL.md`](../SKILL.md).

## Steps

1. Preserve supplied IDs, and before delegation assign missing IDs as
   `VEC-EXT-###` in input order. Done when every supplied ID is unchanged and
   every remaining proposition has one.
2. Split compound propositions into separately suffixed IDs without changing
   their meaning. Done when every proposition carries one stable single-claim
   ID.
3. Dispatch exactly one self-contained `locator` with all external-claim
   requests, applicable repository instructions, the checkout path, and the
   minimum local read/search scope. Do not dispatch one agent per claim or ask
   the locator to inspect unrelated code. Done when exactly one such locator is
   dispatched with the step 4 brief, or the dispatch is reported blocked.
4. Brief the locator to:

   1. Establish repository applicability independently for every claim. Cite
      exact `path:line` evidence for target/version, platform, enabled
      extensions or features, compile flags, and relevant preconditions. Missing
      applicable configuration makes that proposition `UNRESOLVED`.
   2. Use the host's official browse/search mechanism to locate primary
      evidence: a normative specification or standard, official vendor/project
      documentation, or official upstream headers/source for version-specific
      facts.
      - Never use memory, search snippets, blogs, forums, AI summaries, or
        unofficial mirrors.
   3. Identify each authoritative source by title/project and applicable
      version, revision, tag, or commit. Give the exact section, anchor,
      page/table, symbol, or source location and the shortest decisive quotation
      or faithful rule statement. Add an official immutable link when available.
   4. Return exactly one `VERIFIED`, `REFUTED`, or `UNRESOLVED` verdict per
      stable ID.
      - `VERIFIED` requires both an authoritative rule and proven repository
        applicability.
      - `REFUTED` requires completed authoritative evidence that contradicts the
        proposition or proves an unmet precondition.
      - State the precise missing evidence for `UNRESOLVED`.
   5. Make no repository changes, recommendations, or decisions about findings.

   Done when that one locator has returned or the dispatch is reported blocked.
5. Check that the returned evidence preserves every ID, separates local
   applicability from source identity, and directly decides each proposition.
   Done when every ID has evidence that decides its proposition or is left
   unresolved.
6. Use `PASS` only when every claim is `VERIFIED` or `REFUTED`. Any `UNRESOLVED`
   claim makes the report `NEEDS_ACTION`. Use `BLOCKED` only when the required
   locator cannot be dispatched or its result cannot be obtained at all. Done
   when the report carries the status those rules select.

## Rules

- This is a read-only evidence workflow: never edit, recommend a fix, review
  surrounding code, or decide whether a dependent finding or plan choice is
  accepted.
- Do not upgrade incomplete evidence. All `VERIFIED` and `REFUTED` results are
  completed evidence returned to the caller to decide; a refutation is not
  itself permission to dismiss or modify the dependent finding.
- Proposed URLs are discovery hints, not evidence.
- Official upstream headers and locally pinned standards may use an exact
  citation without a URL. If the host cannot dispatch the locator, return
  `BLOCKED`; do not investigate from memory. If the locator runs but official
  browsing, a primary source, or applicability evidence is unavailable, preserve
  the affected verdict as `UNRESOLVED`.
