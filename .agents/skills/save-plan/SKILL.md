---
name: save-plan
description: Save an explicitly supplied complete plan-mode proposal into the correct Broken Engine planning tree. Documents/Plans receives Git-backed metadata; Documents/Investigations and Documents/Features remain manual.
argument-hint: [PascalCase.md]
allowed-tools: [Read, Write, Edit, Glob, Grep, PowerShell, Bash]
disable-model-invocation: true
---

# Save Plan

## Purpose

Persist exactly one complete client-supplied proposal.

## Inputs

Resolve the explicitly supplied complete proposal as follows:

- For Codex, use the latest complete `<proposed_plan>` body.
- For Claude, use the explicitly supplied absolute plan-file path.
- Do not discover or infer a source from a client-local plan store.

## Steps

1. Read `Documents/AGENTS.md` and the selected tree's guidance. Done when both
   have been read.
2. Check the body for decision-completeness: context, smallest design, critical
   files, out-of-scope boundary, applicable risk trigger/invariants, and
   observable acceptance criteria where the change needs them.

   - A different implementing model with no access to the planning conversation
     executes it verbatim.
   - Keep it self-contained, with no unresolved options, TBDs, or decisions
     deferred beyond trivial detail, apart from the open parts
     `Documents/Plans/AGENTS.md` `## Plan files` allows; queue any missing,
     undecidable (e.g., risk trigger or acceptance criteria), or unresolved gap
     for step 8 instead of blocking.

   Done when every listed element is present or queued as a gap for step 8.
3. Keep the body picking one design, or queue competing designs for step 8.

   - Phrase an agent-made choice as the author's recommendation with rationale;
     use binding language only where
     [`../../references/authority-order.md`](../../references/authority-order.md)
     permits it.

   Done when choices are phrased that way and any competing design is queued.
4. Require an explicit scope contract in the body:

   - in-scope entries under `## In scope` and boundaries under `## Out of scope`;
   - scope is target and ceiling: smallest complete change, no added
     abstractions, configuration, refactors, or adjacent fixes;
   - scope is bounded inside files to named functions, members, or regions (plus
     mechanical necessities like includes/declarations);
   - every file a `## Coordination` clause obliges editing appears in
     `## In scope` with named regions, unless those regions depend on an
     unresolved scope decision;
   - if a scope ceiling or `## Out of scope` is missing, scope is
     file-granularity-only, or `## Coordination` regions depend on an open
     decision, queue the gap and affected `## Coordination` obligations for
     step 8, bound `## In scope` to open-decision resolution on claim, and
     (when missing) add a provisional `## Out of scope` deferring all
     implementation edits until open decisions are resolved.

   Done when both headings exist (including any provisional `## Out of scope`)
   and every coordination-obliged file has named regions in scope or is queued
   as a bounded gap.
5. Classify into `Documents/Plans/`, `Documents/Investigations/`, or
   `Documents/Features/` by `Documents/AGENTS.md` `## Planning Trees`. Done when
   the tree is chosen.
6. Select the owning area and a concise PascalCase filename matching
   `^[A-Z][A-Za-z0-9]*\.md$`. Done when both are chosen.
7. Search live Plans for duplicate root cause and implementation boundary before
   writing (or proven symptom for an investigation portion per
   `Documents/Plans/AGENTS.md` `## Plan files`). Done when the search has run.
8. Route gaps queued in steps 2-4 without halting to ask or inventing answers
   (`Documents/Plans/AGENTS.md` `## Plan files`), and write the body file for an
   executable Plan:

   - Executable Plan with gaps: embed them and drafted regions as proposed scope
     in `## Unresolved decisions`, narrow `## In scope` to open-decision
     resolution, and defer `## Coordination` edits until resolved. For an
     executable Plan with a blocking `dependsOn` in `WorktreeCli`'s inventory
     (`## Git-backed scheduler` there), first create a companion Plan via
     `/create-follow-up-plans`; besides `## Plan files` write-back, require it
     to write resolved scope, replace any provisional boundary, and reconcile
     deferred `## Coordination` before completing.
   - `Documents/Features/` or `Documents/Investigations/` with gaps: first
     create the companion Plan via `/create-follow-up-plans`.

   Done when any companion exists and any executable Plan body file is written.
9. Save the document:

   - Executable Plan (`Documents/Plans/` or
     `Projects/Coretide/Documents/Plans/`): run
     `.agents/scripts/New-PlanFile.ps1`:

     ```powershell
     pwsh -NoProfile -File .agents/scripts/New-PlanFile.ps1 -Area <area> -Name <PascalCase.md> -Body <body file path> -DependsOn <plan paths as one comma-separated token>
     ```

     For a prerequisite-blocked executable Plan with a step-8 companion, pass
     its normalized path with existing prerequisites in this single `-DependsOn`
     call (`completion.plan-cited` in
     [`claim-results.md`](../next-plan/references/claim-results.md)).
     Parameters, single-token rule, result shape, and exit handling live in
     [`../../references/new-plan-file.md`](../../references/new-plan-file.md);
     report saved only on `created`.
   - `Investigations/` or `Features/` per step 5: write the document directly
     to its chosen path as ordinary Markdown without `New-PlanFile.ps1` or
     overwriting an existing file.

   Done when created or written.
10. For an executable Plan from `New-PlanFile.ps1`, record the folded
    invalid-plan diagnostics and stale-edge notices: existing dependency paths
    must remain executable to block a child, and missing paths are intentionally
    stale satisfied edges. Done when recorded, or N/A for direct Markdown.

## Handoff

Report source, selected path, duplicate outcome, tier trigger, dependency
decision, validation command/result where applicable, and changed files. A saved
Plan is tracked Git content, not a queue change and not a claim.

### Landing pre-approval

Invoked after a proposal has been drafted and presented, `/save-plan` activates
standing landing confirmation for every saved destination, over the paths
[`../finalize-changes/SKILL.md`](../finalize-changes/SKILL.md)
`### Landing confirmation` defines for it.

## Rules

- Features are ordinary Markdown. Do not prepend scheduler metadata unless the
  user explicitly makes it an executable Plan, and never create claim records,
  request files, score fields, queue rows, or publication inputs for Features.
- Do not read local claims or use validation to choose, claim, or reorder work.
