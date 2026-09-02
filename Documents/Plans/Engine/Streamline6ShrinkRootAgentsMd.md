<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-01T18:38:55.579Z","dependsOn":[]} -->
# Streamline 6: Shrink the root AGENTS.md to routing and invariants

## Context

`AGENTS.md` is 31,539 bytes and is loaded into every session before any code
is read. Its own progressive-disclosure directive (line ~104) says each fact
lives once at its owning layer and the root carries only "the constraints,
invariants, and routing every session needs". The file violates that:

- Step 8 (`AGENTS.md:85`) is one paragraph of roughly 600 words covering
  lease mechanics, rebase re-review rules, and confirmation re-asking, most of
  which `/finalize-changes` restates.
- Step 2 (`AGENTS.md:79`) holds a single 150-word sentence defining when a
  skill edit counts as behavior (partly rewritten by Streamline 3).
- Steps 5-7 (`AGENTS.md:82-84`) each carry Tier-1 carve-outs that exist only
  because `.agents/references/tier1-combined-review.md` folds three steps into
  one dispatch; the normal path is the exception and the fast path is the
  rule that needs its own reference.
- The Directives say "Do not add unit tests" (`AGENTS.md:102`) while
  `.agents` holds twenty `Test-*.ps1` fixture scripts totalling roughly 9,500
  lines that the skills require to run on change.
- Definitions, the Convergence section, and the Delegation-roles bullets
  restate rules that `subagent-reporting.md`, `/codex-review`, and
  `/next-plan` own.

Streamlines 1-5 remove content from Steps 2, 5, 7, and 8; this Plan does the
final consolidation once those have landed so it rewrites the end state, not
a moving target.

## Design

Author's recommendation: target about 12 KB, same section skeleton.

1. Make the combined pass the default shape at every tier: Step 5 becomes
   "one fresh reviewer dispatch per changed artifact type, each carrying every
   Step 6 reviewer check and, when no landing follows, the Step 7 evidence
   map". Delete the Tier-1 carve-out clauses from Steps 5, 6, and 7 and
   delete `.agents/references/tier1-combined-review.md`; the one sentence
   that survives goes in Step 5. Update `validate-skill/SKILL.md:10` and
   `finalize-changes` references to it.
2. Rewrite Step 8 to at most five sentences: finalizer prepares the commit
   and the acceptance table; main presents the summary; one explicit
   confirmation; locked advance with rollback; a changed diff after review
   re-asks. Everything else moves to `/finalize-changes` if not already
   there, or is deleted if it already is.
3. Rewrite Step 2 to at most three sentences that point at `/plan-audit` for
   the trigger definition.
4. Replace the unit-test directive with "Do not add C++ unit tests." and
   nothing more; fixture decisions for repository scripts stay with the
   skills that own those scripts.
5. Trim the Definitions list to terms the steps still use, delete the
   Convergence section into one sentence at the top of Steps ("Once a step's
   checks pass, move on; add nothing untriggered"), and shorten each
   Delegation-roles bullet to one line that points at its owning skill or
   reference.
6. Leave the project description, Environment, Directives (other than the
   unit-test line), User Interaction, Resolving Ambiguity, Diagnosis
   Discipline, Directory Structure, Static Analysis, Client/Server Targets,
   and Key Patterns sections as they are apart from wording that references
   deleted steps.

## Critical files

- `AGENTS.md`
- `.agents/references/tier1-combined-review.md` (delete)
- `.agents/skills/validate-skill/SKILL.md` — line 10
- `.agents/skills/finalize-changes/SKILL.md`, `references/workflow.md` —
  any Step 8 mechanics that must move in

## In scope

- `AGENTS.md` sections: Definitions, Change Workflow Steps 2, 5, 6, 7, 8,
  Convergence, Delegation-roles bullets, and the unit-test directive line.
- Whole-file deletion of `tier1-combined-review.md` and its inbound links.
- Paragraphs added to `finalize-changes` references only where a Step 8 rule
  is moved rather than deleted.

## Out of scope

- Risk-tier definitions, Step 1, Step 3, Step 4, and the role table.
- Every subsystem `AGENTS.md`.
- Skill bodies other than the single reference lines named above.
- Any change to what a step requires; this Plan changes where rules live
  and how long they are, not which checks run.

## Risk tier and invariants

Expected Tier 1: documentation restructuring with no behavior change, on the
condition that every rule removed from `AGENTS.md` either already exists in
its owning skill or is moved there in the same change. A reviewer escalates
to Tier 2 if any check, gate, or trigger disappears.

Invariants:

- Every check the Change Workflow required before this Plan is still
  required after it, and each is stated exactly once.
- `AGENTS.md` still names every skill a session must route through and the
  tier that triggers it.

## Acceptance criteria

- `wc -c AGENTS.md` reports at most 14,000 bytes.
- `rg -n "tier1-combined-review" AGENTS.md .agents` returns no hits.
- `/progressive-disclosure-review` passes with no finding that a rule is
  stated in two places or in none.
- `/validate-skill` passes for every changed `SKILL.md`.
