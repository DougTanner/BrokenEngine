<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-10T22:53:09.580Z","dependsOn":[]} -->
# Fix: /next-plan — per-citation verdict enumeration main never uses

## Context
Observed symptom. `.agents/skills/next-plan/SKILL.md:76-78` bounds the
preparation dispatch brief's verification evidence with "The brief bounds the
card's verification evidence: each acceptance item comes back as a `path:line`
citation plus a verdict, not verbatim source text, except an item whose purpose
is proposed replacement text." Applied in this session's `/next-plan` run, the
preparation handoff came back at 6,854 characters, a large part of it a
per-citation verdict enumeration for eight accurate citations, every one ending
in "Zero drift". Main used none of those eight rows: an accurate citation
requires no card correction and no implementation change, so only the count
carried information.

The same skill already bounds the parallel case in the opposite direction.
`.agents/skills/next-plan/SKILL.md:172-176` bounds the preparation handoff to
"every contradiction and unresolved decision, the other verified Plan statements
whose result requires a card or implementation change, and one count of the
unaffected statements; it never asks for a per-statement enumeration of
unaffected results." So Plan statements return drifted items individually plus
one count, while acceptance-item citations return an individual verdict for every
item, accurate ones included. The citation rule is the outlier.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: a38b9189-20c7-4333-b93f-26bac1ac2874
- Worktree/branch UUID: 8c2d32c8-71d6-4490-a4dc-9a4257501f8c
- Session branch: claude/8c2d32c8-71d6-4490-a4dc-9a4257501f8c
- Worktree: .claude\worktrees\BrokenEngine\8c2d32c8-71d6-4490-a4dc-9a4257501f8c
- Landing ref: branch `claude/8c2d32c8-71d6-4490-a4dc-9a4257501f8c`, the session
  branch above, which lands this Plan with this session's change
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
The symptom is fully diagnosed from the current tree — the two rules quoted in
`## Context` are the whole evidence — so no transcript is needed and
`/next-plan-review` is not part of this fix.

Author's recommendation, and the smallest fix the author sees: reword the
citation sentence at `.agents/skills/next-plan/SKILL.md:76-78` so a preparation
handoff returns each drifted or unverifiable acceptance-item citation
individually, as `path:line` plus what it found, plus one count of the accurate
citations, and never a per-item enumeration of accurate results — phrased in
parallel with the statement-count rule at `:172-176` so the two read as one
policy. Rationale: the returned rows main acts on are exactly the drifted ones,
the parallel rule already proves the shape is workable, and a reworded sentence
keeps the rule in its single owning location rather than adding a second one.

The exception for an item whose purpose is proposed replacement text stays: such
an item's verbatim text is what main consumes.

If root-causing shows the fix must reach the `## Handoff` field list, the shared
handoff form in `.agents/references/subagent-handoff.md`, or another skill,
surface it for re-planning instead of expanding scope.

## Critical files
- `.agents/skills/next-plan/SKILL.md` — the preparation-dispatch citation
  sentence at `:76-78`, and the `## Handoff` statement-count sentence at
  `:172-176` as the parallel wording to match

## In scope
- The citation-evidence sentence at `.agents/skills/next-plan/SKILL.md:76-78`,
  reworded as `## Design` recommends

## Out of scope
- The statement-count sentence at `.agents/skills/next-plan/SKILL.md:172-176`,
  which is the model, not a target
- The declared `## Handoff` field list and the shared handoff form in
  `.agents/references/subagent-handoff.md`
- The feasibility-estimate sentence that follows at `:79-83`
- Every other step of `/next-plan`; unrelated skills/scripts; any transcript path
  or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior — one skill's dispatch contract);
escalate if the fix reaches the shared handoff form or another skill package.
Invariants: exactly one owning location for the citation-evidence rule; the
proposed-replacement-text exception survives; no transcript path or home path is
embedded.

## Acceptance criteria
- `.agents/skills/next-plan/SKILL.md` states, exactly once, that a preparation
  handoff returns drifted acceptance-item citations individually plus one count
  of the accurate ones, and asks for no per-item enumeration of accurate results
- The proposed-replacement-text exception is still stated
- The static-check runner, invoked as `.agents/references/static-checks.md`
  documents it, reports the `markdown-links` row passing and the
  `validate-skill` row passing for the `next-plan` package
