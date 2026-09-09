# Verify External Claims Worker

The identifier, research, evidence-validation, verdict, and status steps and
rules the direct `locator` applies. The request contract and report form live in
[`../SKILL.md`](../SKILL.md).

## Steps

1. Preserve supplied IDs, and assign missing IDs as
   `VEC-EXT-###` in input order. Done when every supplied ID is unchanged and
   every remaining proposition has one.
2. Split compound propositions into separately suffixed IDs without changing
   their meaning. Done when every proposition carries one stable single-claim
   ID.
3. Establish repository applicability independently for every claim. Cite exact
   `path:line` evidence for target/version, platform, enabled extensions or
   features, compile flags, and relevant preconditions. Missing applicable
   configuration makes that proposition `UNRESOLVED`. Done when each claim has
   exact applicability evidence or its precise gap.
4. Use the host's official browse/search mechanism to locate primary evidence:
   a normative specification or standard, official vendor/project
   documentation, or official upstream headers/source for version-specific
   facts. Never use memory, search snippets, blogs, forums, AI summaries, or
   unofficial mirrors. Done when each claim has primary evidence or its exact
   unavailable source is recorded.
5. Identify each authoritative source by title/project and applicable version,
   revision, tag, or commit. Give the exact section, anchor, page/table, symbol,
   or source location and the shortest decisive quotation or faithful rule
   statement. Add an official immutable link when available. Done when each
   source is identified precisely enough to verify.
6. Return exactly one `VERIFIED`, `REFUTED`, or `UNRESOLVED` verdict per stable
   ID without making repository changes, recommendations, or decisions about
   findings.
   - `VERIFIED` requires both an authoritative rule and proven repository
     applicability.
   - `REFUTED` requires completed authoritative evidence that contradicts the
     proposition or proves an unmet precondition.
   - State the precise missing evidence for `UNRESOLVED`.
   - Done when every stable ID has exactly one supported verdict.
7. Check that the evidence preserves every ID, separates local
   applicability from source identity, and directly decides each proposition.
   Done when every ID has evidence that decides its proposition or is left
   unresolved.
8. Use `PASS` only when every claim is `VERIFIED` or `REFUTED`. Any `UNRESOLVED`
   claim makes the report `NEEDS_ACTION`. Use `BLOCKED` only when the required
   repository or external evidence cannot be accessed at all. Done when the
   report carries the status those rules select.

## Rules

- This is a read-only evidence workflow: never edit, recommend a fix, review
  surrounding code, or decide whether a dependent finding or plan choice is
  accepted.
- Do not upgrade incomplete evidence. All `VERIFIED` and `REFUTED` results are
  completed evidence returned to the caller to decide; a refutation is not
  itself permission to dismiss or modify the dependent finding.
- Proposed URLs are discovery hints, not evidence.
- Official upstream headers and locally pinned standards may use an exact
  citation without a URL. If the required repository or external evidence cannot
  be accessed at all, return `BLOCKED`; do not investigate from memory. If
  official browsing, a primary source, or applicability evidence is unavailable
  for only some claims, preserve each affected verdict as `UNRESOLVED`.
