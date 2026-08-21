# Tier-1 Combined Review Pass

One fresh `reviewer` dispatch carries Change Workflow Steps 5, 6, and 7 for a
Tier-1 change that contains no changed C++. No check is dropped: each component
keeps its owning skill, its own result block, and its own pass condition; only
the dispatch is shared.

## When it applies

The whole change is Tier 1 and no changed file is C++. A Tier-1 change that
touches C++, and every Tier-2+ change, keeps the separate Step 5, 6, and 7
dispatches unchanged. Step 8's landing-gate `/verify-changes` reviewer is never
folded in — it binds to the final prepared diff, which does not exist yet.

## Components, in order

1. Step 5 coherence review of the changed bytes. Owner: Step 5's direct
   coherence routing. Passes when no accepted semantic finding remains
   unresolved.
2. Every Step 6 reviewer-role check the change triggers — today
   `/validate-skill` for a changed `.agents/skills/*/SKILL.md`, and
   `/progressive-disclosure-review` for a changed `AGENTS.md`, `CLAUDE.md`,
   `.agents/skills/**/*.md`, or `.agents/references/**/*.md` file. Run each triggered
   skill in full and return its complete handoff unmodified, exactly as
   `/verify-changes` requires for a skill change. Each passes on its own skill's
   pass condition.
3. When Step 7 applies — a stage completing without landing — map every approved
   criterion and invariant to evidence that settles the question on its own,
   including each duplicate check's independent signal, using Step 7's Tier-1
   evidence ceiling: static, schema, link, validator, and changed-C++
   compilation checks. Passes when every criterion and invariant maps to such
   evidence.

The Step 6 mechanic and implementer checks — `/code-style-review`,
`/update-vcxproj`, `/update-claude-docs` — are not reviewer dispatches and are
unaffected.

## Result sections

Components 1 and 3 have no owning skill output format, so the pass returns:

- `Coherence` — findings with file:line and severity, or an explicit no-finding
  statement.
- `Criteria` — one row per approved criterion and invariant: the criterion, the
  evidence, and pass or fail. Omitted when Step 7 does not apply.

Component 2 returns each triggered skill's own complete handoff verbatim, one
after another; never summarize, reformat, or merge them into the sections above.

## Routing

In Claude Code the dispatch goes through `/codex-review`, with the manager's
brief carrying this whole combined contract, including which components apply.

## Fix rounds

Step 5's rule is unchanged: main decides once, accepted fixes go to a separate
`implementer`, and only affected regions are re-reviewed. A re-run restates only
the criterion rows whose evidence the fix changed.
