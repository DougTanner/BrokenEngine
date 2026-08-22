<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-22T12:40:18.756Z","dependsOn":[]} -->
# Add a table of contents to the oversized /finalize-changes script reference

## Context

The fresh Step 6 `/progressive-disclosure-review` found a pre-existing
documentation debt in
`.agents/skills/finalize-changes/references/scripts.md`: review item 4 at
`.agents/skills/progressive-disclosure-review/SKILL.md:53-59` says that a
reference file over 2,000 `bt-token-v1` needs a table of contents, while the
reference has no table of contents. The current measurement is 5,436
`bt-token-v1`, 21,743 bytes, and 250 lines. Its major headings are `##
Invocation` at line 9, `## Contracts` at line 50, and `## Fixture suites` at
line 209; no TOC entries occur before or between them.

This gap predates the active change. The baseline blob at
`774a1def513887aae849cc1c7d4a380f9c983711:.agents/skills/finalize-changes/references/scripts.md`
was already 21,407 bytes. The session diff is only five added lines and one
removed line in lines 13-17 for owner-token explanation prose; it changes no
heading or navigation. The active Plan
`Documents/Plans/Skills/FinalizeChangesOwnerTokenPlaceholder.md` authorizes
that owner-token explanation only. The user-approved current-code evidence
substitutes for unavailable historical transcript proof, and the current
change passed its coherence and progressive-disclosure review. The missing
TOC is therefore outside the active boundary and is a documentation-debt
residual, not an in-scope acceptance failure.

## Design

Recommendation: add a compact Markdown table of contents immediately below
the title and before `## Invocation`, linking the existing `## Invocation`,
`## Contracts`, and `## Fixture suites` sections. This is the smallest useful
navigation aid for the measured reference; preserving the current headings,
ordering, command blocks, and prose avoids unrelated restructuring and keeps
the established anchors stable.

## Critical files

- `.agents/skills/finalize-changes/references/scripts.md` — add the TOC near
  the title and link the existing major section headings.

## In scope

- Add the compact TOC to
  `.agents/skills/finalize-changes/references/scripts.md` between the title
  and `## Invocation`.
- Link each existing major section: `## Invocation`, `## Contracts`, and `##
  Fixture suites`.
- Verify the TOC and the reference-size condition without changing existing
  script commands, contract prose, heading names, or section order.

## Out of scope

- The owner-token wording change in the active
  `FinalizeChangesOwnerTokenPlaceholder.md` work.
- Any bundled script, `/finalize-changes/SKILL.md`, progressive-disclosure
  threshold, or other reference/documentation file.
- Broad restructuring, section splitting, or unrelated prose cleanup.

## Risk tier and invariants

Expected future Change Workflow Tier 1: documentation-only navigation in one
reference file. The fix has no determinism/CRC, serialization/`.pack`/
`kiVersion`, replay, wire/protocol, affinity, threading, allocation, shader,
build, or live-verification exposure. Escalate if implementation requires a
heading, script, or skill behavior change beyond the TOC.

## Acceptance criteria

- The reference contains a useful TOC linking all three existing major
  sections (`Invocation`, `Contracts`, and `Fixture suites`) to their current
  headings.
- The documented `pwsh -NoProfile -File
  .agents/scripts/Measure-Tokens.ps1 -Path
  .agents/skills/finalize-changes/references/scripts.md` check and the
  progressive-disclosure review confirm that the over-2,000-reference TOC
  condition is satisfied.
- Existing commands, contract prose, heading names, section order, and
  behavior are unchanged outside the new TOC.
- `WorktreeCli.exe plan validate --repo <git-common-dir> --worktree
  <checkout>` exits 0 with `status: valid` and `code: ok`.

## Coordination

- None. No prerequisite or reciprocal coordination is evidenced for this
  Tier-1 documentation-only fix.

## Notes

- The originating condition is Step 6 progressive-disclosure review item 4;
  item 5 explicitly treats untouched excess as a residual.
- No dependency is recorded because the active owner-token Plan is a
  separate boundary and does not block adding navigation later.
