# Save Plan Worker

The body, review, classification, and write steps, and the rules the runner
applies. The source input, the landing pre-approval, and the report form live in
[`../SKILL.md`](../SKILL.md).

## Steps

1. Read `Documents/AGENTS.md` and the selected tree's guidance. Done when both
   have been read.
2. Require a decision-complete body: context, smallest design, critical files,
   out-of-scope boundary, applicable risk trigger/invariants, and observable
   acceptance criteria where the change needs them.

   - The saved plan is a file one model writes for another model to pick up: a
     different implementing model with no access to the planning conversation
     executes it verbatim.
   - So the body must be the final actionable plan — self-contained, with no
     unresolved options, TBDs, or decisions deferred to the implementer beyond
     trivial naming and local detail.

   Done when the body carries each of those and leaves no unresolved option.
3. Keep the body picking exactly one design.

   - Phrase an agent-made choice in the supplied body as the author's
     recommendation with its rationale.
   - Use binding language only where
     [`../../../references/authority-order.md`](../../../references/authority-order.md)
     permits it.

   Done when the body names one design and every agent-made choice is phrased
   that way.
4. Require an explicit scope contract in the body:

   - in-scope entries under a required `## In scope` heading, and the boundary
     under a required `## Out of scope` heading;
   - the listed scope is both target and ceiling: the implementer makes the
     smallest complete change and adds no abstractions, configuration,
     refactors, or fixes to adjacent code it encounters;
   - scope is limited inside files, not only across them — in-scope entries name
     the specific functions, members, or regions to change, and naming a file
     grants no permission to touch anything in it beyond the named regions plus
     the mechanical necessities (includes, declarations) the named change
     requires;
   - every file a `## Coordination` clause obliges the implementer to edit must
     itself appear in the in-scope list with its named regions; a coordination
     obligation is not a substitute for scope.

   Done when both headings are present with named regions and every
   coordination-obliged file is listed in scope.
5. Ask the user for a meaningfully missing decision — including ambiguity, a
   missing scope ceiling, or file-granularity-only scope — rather than invent or
   patch it. Done when no such gap is left unanswered.
6. Once the body is final, and before classifying and writing it, dispatch one
   fresh `reviewer` for `/plan-simplicity-review` on that final body snapshot:

   - Dispatch whenever the body adds new code or modifies non-documentation
     behavior per the trigger in
     [`../../plan-simplicity-review/SKILL.md`](../../plan-simplicity-review/SKILL.md)
     `## When to use`.

   Done when the trigger is evaluated and, where it fired, the review has
   returned.
7. Route the findings by disposition, not by finding class: accepted `simplify`
   findings revise the body, and every `user-judgment` finding goes to the user,
   whatever class it carries. Done when every returned finding is dispositioned.
8. Present the complete revised body for explicit user approval before saving
   whenever accepted findings change its meaning, because this skill persists
   exactly the client-supplied proposal and invoking it pre-approves landing the
   saved file.

   - A clean review, or accepted findings that change only sub-semantic wording,
     keeps that pre-approval in force.
   - This save-time review does not waive the prep-time
     `/plan-simplicity-review` that `/next-plan` runs when the plan is later
     prepared for implementation.

   Done when a meaning-changing revision has explicit user approval, or no such
   revision was made.
9. Classify into `Documents/Plans/` for executable engine debt or
   `Documents/Features/` for manual capability planning. Done when the tree is
   chosen.
10. Select an existing area and a concise PascalCase filename matching
    `^[A-Z][A-Za-z0-9]*\.md$`. Done when the area and filename are chosen.
11. Search live Plans for duplicate root cause and implementation boundary
    before writing. Done when the duplicate search has run.
12. For an executable Plan, write the plan body to a file. Done when that body
    file exists.
13. Create the Plan with the repository-owned `.agents/scripts/New-PlanFile.ps1`:

    ```powershell
    pwsh -NoProfile -File .agents/scripts/New-PlanFile.ps1 -Area <existing area> -Name <PascalCase.md> -Body <body file path> -DependsOn <plan paths as one comma-separated token>
    ```

    - Its parameters, the `-DependsOn` single-token rule, its result shape, and
      its exit handling are in
      [`../../../references/new-plan-file.md`](../../../references/new-plan-file.md).
    - Only the created outcome there permits reporting the plan as saved.

    Done when the script reports the created outcome.
14. Record the folded result's invalid-plan diagnostics and stale-edge notices:
    existing dependency paths must remain executable to block a child, and
    missing paths are intentionally stale satisfied edges. Done when those
    diagnostics and notices are recorded.

## Rules

- Features are ordinary Markdown. Do not prepend scheduler metadata unless the
  user explicitly makes it an executable Plan, and never create claim records,
  request files, score fields, queue rows, or publication inputs for Features.
- Do not read local claims or use validation to choose, claim, or reorder work.
