<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T22:14:33.753Z","dependsOn":[]} -->
# Compute the Change Workflow dispatches with a step script

## Context

`.agents/references/change-workflow.md` carries the Change Workflow. At
baseline `591184a4dbf802ac2f6d3c2cf9de168916e4714d` it holds 4,356 words
(`wc -w`) and reaches every main session at start in four parts through
`.agents/scripts/Write-SessionWorkflowContext.ps1`, each part under that
script's `$script:MaximumPartCharacters` budget. The file is edited several
times a week.

Much of its Implement and propagate through Verify the acceptance table steps
(Steps 4-8) is a lookup main performs by reading prose: trigger conditions
("when the change touches C++", "when a `cpp`-class file changed
(`overEngineeringReview`)", "when the session changed any file in a
`.agents/skills/*/` package"), tier gates ("Tier 3 always"), stage gates ("for
a stage completing without landing"), and `Order:` paragraphs. The changed-file
half of that lookup is already computed in code:
`.agents/scripts/Get-SessionChangeInventory.ps1` `Get-RoutingTrigger` returns
one boolean per trigger (`coherenceReview`, `repoCodeReview`,
`overEngineeringReview`, `glslReview`, `codeStyleReview`, `commentReview`,
`updateAffectedCode`, `updateVcxproj`, `updateClaudeDocs`, `validateSkill`,
`progressiveDisclosureReview`) plus `vcxprojCandidates`, and
`Invoke-StaticChecks.ps1` and the `-Landing` acceptance skeleton already read
those booleans. Main never runs the inventory; it re-derives the same triggers
from prose.

The user's fixed decisions, in their words:

- "Scripts should ALWAYS be used over agent-instructions".
- The workflow "is producing good results, so I don't want to go overboard
  here, but targeted re-writes/consolidations/clearer directives are something
  we should do".

So this Plan keeps the same steps, roles, triggers, and order, and changes only
who computes them: a script computes the dispatch list from the tier, the
stage's landing state, the execution card's `runner` assignment, and the
inventory's triggers, and main follows its output.

## Design

### The script

The author recommends one new script,
`.agents/scripts/Get-ChangeWorkflowSteps.ps1`, rather than a fifth mode of
`Get-SessionChangeInventory.ps1`. The inventory answers "what changed and what
artifact type is each change" for a dozen script and skill consumers under
a versioned JSON schema; the tier, the landing state, and the `runner`
assignment are not changed-file facts, and adding them would put workflow
policy into that contract. The new script composes the inventory the way
`Invoke-StaticChecks.ps1` `Get-InventoryDocument` does, through
`Invoke-AgentProcess` from `AgentScriptCommon.psm1`.

Parameters:

- `-RepositoryRoot` (mandatory) and `-Baseline` (mandatory, full SHA) — passed
  to the inventory unchanged.
- `-Tier` (mandatory, `ValidateSet(1, 2, 3)`).
- `-Landing` (switch) — the stage lands in this session.
- `-HarnessChecks` (switch) — the execution card's `Roles` field names the
  `runner`.
- `-Head` (optional commit) — passed to the inventory instead of
  `-IncludeUntracked`, as `Invoke-StaticChecks.ps1` does; the acceptance runs
  below use it on historical commits.

Run: the inventory runs with `-RepositoryRoot`, `-Baseline`, and `-Head <head>`
when given, otherwise `-IncludeUntracked`. A nonzero exit or a `status` other
than `pass` exits 2 with one stderr line naming the inventory's `code` and
`message`.

Output: plain text on stdout, written as raw UTF-8 bytes the way
`Write-SessionWorkflowContext.ps1` writes, because its only consumer is the
main session reading it. Exit codes: 0 printed, 2 inventory unavailable, 1
unexpected error (one stderr line).

Shape — one heading line per step that has rows, each followed by its
unnumbered rows; nothing else:

```text
Implement and propagate
- implementer /update-affected-code: the slices' changed C++ and GLSL; after the slices
Run targeted pre-review checks
- main: pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot '<root>' -Baseline <sha> -IncludeUntracked; after /update-affected-code
- mechanic /code-style-review: changed C++; after /update-affected-code
- builder /compile: each Build required handoff; brief states the /agent-harness trigger of /compile ## Inputs; after /code-style-review, in parallel with the static checks
Review and resolve correctness
- fresh reviewer /over-engineering-review finder: changed cpp-class files; after the Run targeted pre-review checks rows
...
```

The static-checks row prints the actual root and baseline the script received
(and `-Head <sha>` in place of `-IncludeUntracked` when `-Head` was given).
Each row's `after` clause names the predecessors its table `After` cell lists
that are present, by skill name (the static-checks row as "the static checks",
a whole step by its name), leaving out each clause whose row is absent; a row
whose every predecessor is absent prints no `after` clause.

### The dispatch table

The script owns this table as one ordered array; each row is emitted when its
condition holds. `T.x` is the inventory's `triggers.x`. `combined` is
`Tier 1 and T.coherenceReview and not T.repoCodeReview and not T.glslReview`
(the baseline "at Tier 1 with no changed C++ or GLSL" condition).

| Step | Condition | Role and skill | Scope text | After |
| --- | --- | --- | --- | --- |
| Implement and propagate | `T.updateAffectedCode` | `implementer` `/update-affected-code` | the slices' changed C++ and GLSL | the slices |
| Run targeted pre-review checks | always | `main` static-checks command | (the command) | `/update-affected-code` |
| Run targeted pre-review checks | `T.codeStyleReview` | `mechanic` `/code-style-review` | changed C++ | `/update-affected-code` |
| Run targeted pre-review checks | always | `builder` `/compile` | each `Build required` handoff; with `-HarnessChecks` the brief states the `/agent-harness` trigger of `/compile` `## Inputs` | `/code-style-review`, in parallel with the static checks |
| Review and resolve correctness | `T.overEngineeringReview` | fresh `reviewer` `/over-engineering-review` finder | changed `cpp`-class files | Run targeted pre-review checks rows |
| Review and resolve correctness | `T.overEngineeringReview` | fresh `implementer` `/over-engineering-review` validator | the finder's removal candidates; skipped when it returns none | `/over-engineering-review` finder |
| Review and resolve correctness | `T.repoCodeReview` | fresh `reviewer` `/repo-code-review` | changed C++; its implementation handoff is the validator's, or the finder's when the validator is skipped | `/over-engineering-review` validator, or the finder when the validator is skipped |
| Review and resolve correctness | `T.commentReview` | `mechanic` `/comment-review` | changed C++ and GLSL | `/over-engineering-review` validator, or the finder when the validator is skipped |
| Review and resolve correctness | `T.glslReview` | fresh `reviewer` `/glsl-review` | changed shaders | Run targeted pre-review checks rows |
| Review and resolve correctness | `T.coherenceReview` | fresh `reviewer` `/coherence-review`, combined mode when `combined`, otherwise coherence mode | changed non-C++ artifacts; in combined mode also `also carries:` each of `/external-skill-creator validate mode` (when `T.validateSkill`), `/progressive-disclosure-review` (when `T.progressiveDisclosureReview`), and `/verify-acceptance, read-only apart from its ## Handoff Temp/ file` (when not `-Landing`) | Run targeted pre-review checks rows |
| Review and resolve correctness | Tier 3 | `reviewer` `/adversarial-review` | every changed artifact type | every other row of this step |
| Apply the triggered cleanup | `T.updateVcxproj` | `mechanic` `/update-vcxproj` | the count of `vcxprojCandidates` paths | Review and resolve correctness converged |
| Apply the triggered cleanup | `T.validateSkill` and not `combined` | fresh `reviewer` `/external-skill-creator` validate mode | changed `.agents/skills/*/` packages | same |
| Apply the triggered cleanup | `T.updateClaudeDocs` | `implementer` `/update-claude-docs` | changed C++ and GLSL | same |
| Apply the triggered cleanup | `T.progressiveDisclosureReview` and not `combined` | fresh `reviewer` `/progressive-disclosure-review` | changed instruction docs | `/update-claude-docs` |
| Verify the acceptance table | `-HarnessChecks` | `runner` `/agent-harness` | every `/agent-harness` check on the execution card, including those marked `before the first edit`, on the edited tree, cited in the form `/agent-harness` `## Inputs` defines with no step sequence of the brief's own; main reads no check; with `-Landing`, before `/finalize-changes` | Apply the triggered cleanup |
| Verify the acceptance table | not `-Landing` and not `combined` | fresh read-only `reviewer` `/verify-acceptance` | this stage's acceptance table | `/agent-harness` |

The review-fix round rules and the rerun rule are not script output; they stay
as one paragraph of the Review and resolve correctness step (`## In scope`
item 5).

### Trigger ownership

One owner per fact, so prose and script cannot disagree:

- Changed-file triggers — `Get-SessionChangeInventory.ps1`
  `Get-RoutingTrigger`, which already feeds the landing acceptance skeleton
  and the static checks.
- Which dispatch each trigger, tier, landing state, and `runner` assignment
  causes, its role, scope, and order — the new script's table.
- The review-fix round rules and when main reruns the script — one paragraph
  of the Review and resolve correctness step.
- `change-workflow.md` Steps 4-8 keep no trigger condition; one instruction
  runs the script.
- Skill `## When to use` sections hand ownership to the scripts: the
  `/over-engineering-review` bullet already names the inventory's
  `overEngineeringReview` trigger and stays as is; the
  `/progressive-disclosure-review` first bullet stops restating the path list
  (which duplicates `Test-InstructionDocPath`) and names the inventory's
  `progressiveDisclosureReview` trigger; the `/coherence-review` combined-mode
  bullet, which points at a workflow bullet this Plan deletes, points at the
  script's combined-mode row.
- `/plan-simplicity-review` keeps owning its trigger: it is a judgment on plan
  content ("adds new code or modifies non-documentation behavior", with "when
  unsure, dispatch it") at the Prepare and Plan review steps, before any file
  changes, so no script can compute it from a diff.

### Coherence trigger classes

The baseline prose routes `/coherence-review` for every non-C++ artifact
("other Tier-2+ artifacts"), but `$script:CoherenceReviewClasses` in the
inventory lists only `doc`, `plan`, `script`, `skill`, and `vcxproj`, so a
change to an `other`-class file (`.claude/settings.json`, `.codex/*.toml`,
`.opencode/opencode.json`, `.svg`, a gitlink) or a `binary` file fires no
coherence trigger. The prose was trusted (authority order: documents over code
behavior). The user chose the option the draft recommended: add `binary` and
`other` to `$script:CoherenceReviewClasses`, so the script and the landing
acceptance skeleton both follow the prose. The deleted-Plan and
`RefactorCandidates.md` exclusions stay.

### Coherence dispatch count

The baseline says "Main dispatches one fresh `reviewer` per changed artifact
type", while `/coherence-review` `## Purpose` reviews "the non-C++ artifact
types a ... dispatch owns" (plural). The author recommends one
`/coherence-review` row covering every changed non-C++ class, as the table
above shows.

### Card `Roles` and the instruction

Main still reads the execution card's `Roles` field alone to decide the
`runner`, and passes `-HarnessChecks` from it; the rule that `Roles` names the
`runner` exactly when at least one acceptance check is an `/agent-harness`
check moves from the Verify the acceptance table step into the instruction.
A change without an execution card has no `Roles` field and so no
`-HarnessChecks`, which replaces the baseline sentence deriving that from
Tier 1. Before implementation, main states the Steps 4-8 roles in a
preparation brief as "the step script's rows after the slices" plus the
`runner` when it applies; `/prepare-change` and `/next-plan` text ("as main
states them from the workflow") stays true unchanged.

### Steps 1-3 and the pre-edit card search stay out

This Plan is one stage covering every Steps 4-8 trigger, tier gate, landing
gate, and order, because Step 4's `/update-affected-code` and Step 8's
`runner` and `/verify-acceptance` gates are the same lookup over the same
inputs; splitting Steps 5-7 from them would leave half of each `Order:` chain
in prose. Steps 1-3 stay prose: their deciding triggers are judgment (the
tier, the `/plan-simplicity-review` content trigger, the claims an audit
raises) and run before any changed file exists. The Step 4 search for checks
marked `before the first edit` stays prose here because it reads the execution
card, a different input. Both are later stages under `## Out of scope`.

### Expected effect

The replaced regions hold about 1,010 words and their replacements about 360,
so `change-workflow.md` loses about 650 words net, from 4,275 to about 3,625 if
no other edit lands first. The Steps 4-6 and Steps 7-9 session-context parts
shrink accordingly. The four `session-context-part` markers stay where they
are.

## Critical files

- `.agents/scripts/Get-ChangeWorkflowSteps.ps1` (new)
- `.agents/scripts/Get-SessionChangeInventory.ps1`
- `.agents/references/change-workflow.md`
- `.agents/skills/coherence-review/SKILL.md`
- `.agents/skills/coherence-review/references/worker.md`
- `.agents/skills/progressive-disclosure-review/SKILL.md`
- Read only: `.agents/scripts/Invoke-StaticChecks.ps1` (`Get-InventoryDocument`
  pattern), `.agents/scripts/AgentScriptCommon.psm1` (`Invoke-AgentProcess`),
  `.agents/scripts/Write-SessionWorkflowContext.ps1` (raw stdout pattern).

## In scope

Regions are named by baseline line number and a key phrase; locate each by its
key phrase (`## Coordination`).

1. `.agents/scripts/Get-ChangeWorkflowSteps.ps1` — new file: a header comment
   stating purpose, parameters, output shape, exit codes, and that the script
   owns the Steps 4-8 dispatch table while the inventory's
   `Get-RoutingTrigger` owns the changed-file triggers; the parameters, the
   inventory call, the dispatch table, and the output formatter, all as
   `## Design` states.
2. `.agents/scripts/Get-SessionChangeInventory.ps1` — the
   `$script:CoherenceReviewClasses` assignment (line 41) and its comment (line
   40): add `binary` and `other` (the user's choice, `### Coherence trigger
   classes`). No other line.
3. `.agents/references/change-workflow.md`, `#### Step 4 — Implement and
   propagate`:
   - `Order:` paragraph (lines 256-265, "Order: the acceptance checks the
     execution card marks"): delete only the clause "`/update-affected-code`
     runs after them" with its preceding semicolon, ending the first sentence
     at "then the slices run in parallel."; rewrap the paragraph.
   - delete the bullet "`implementer` runs `/update-affected-code` — after any
     C++ or GLSL change." (line 278).
   - insert after the `/implement-plan` bullet (line 277), before "Main splits
     the work into disjoint slices", this paragraph:

     ```text
     When the slices return, main runs
     `pwsh -NoProfile -File .agents/scripts/Get-ChangeWorkflowSteps.ps1 -RepositoryRoot '<worktree root>' -Baseline <baseline SHA> -Tier <tier>`,
     adding `-Landing` when the stage lands in this session and `-HarnessChecks`
     when the execution card's `Roles` field names the `runner`, which it does
     exactly when at least one acceptance check is an `/agent-harness` check; main
     reads that field alone. Main dispatches the rows the output lists, in their
     order, through the Verify the acceptance table step, and reruns the script
     as the Review and resolve correctness step states. The script owns which
     dispatches the tier and the changed files trigger, their roles, scopes, and
     order; each brief carries what its skill's `## Inputs` requires.
     ```

4. `change-workflow.md`, `#### Step 5 — Run targeted pre-review checks`:
   replace the `Order:` paragraph and the three bullets (lines 285-306, from
   "Order: the full applicable static pass" through "(Step 8 `runner`
   bullet).") with:

   ```text
   Focused implementation self-checks remain inside the implementation slices.
   Main has an `implementer` rewrap every `line-length` failure the
   static-checks runner reports, whether or not the change added the line.
   ```

   The paragraphs from "Full builds and runtime or harness scenarios" to the
   end of the step stay unchanged.
5. `change-workflow.md`, `#### Step 6 — Review and resolve correctness`:
   delete the `Order:` paragraph (lines 322-343, "Order: the per-artifact-type
   reviews") and all ten bullets (lines 345-369); in their place put:

   ```text
   At any tier main may add a `reviewer` running `/adversarial-review` on the
   trigger its `## When to use` states.

   Main runs `/verify-external-claims`, dispatching one `locator`, on the
   external claims a review or `/resolve-findings` handoff raises, before
   deciding the dependent finding or resolution. A separate `implementer` runs
   `/resolve-findings` for each finding main accepts; then only the affected
   regions are re-reviewed and retested, each re-review brief carrying the
   first-round findings its skill's `## Inputs` requires, and a reviewer with no
   accepted finding whose region a fix changed reruns as a fresh first-round
   pass. A second round needs a reproducible blocker. Before any further
   `/resolve-findings` attempt on a cause whose fix failed its retest, an
   `implementer` runs `/external-diagnose-bug`; its proven cause returns to
   `/resolve-findings` or a plan. When the `/over-engineering-review` validator
   or a round changes C++, `/code-style-review` over the newly changed ranges,
   then `/compile` of its `Build required`, run before `/repo-code-review` and
   `/comment-review` start (after a round, as its re-reviews); no other review
   waits for them. `/over-engineering-review` runs once per stage; a later
   round's re-review covers its own fix. Main reruns the step script after
   `/update-affected-code`, `/update-claude-docs`, the validator, and each round
   that changed files, and dispatches each row a rerun first lists.
   ```

   In the closing paragraph (lines 371-388) delete its first sentence, "Main
   dispatches one fresh `reviewer` per changed artifact type, ... scoped to
   the changed bytes and the rules they touch.", and rewrap that paragraph; the
   rest of it stays word for word.
6. `change-workflow.md`, `#### Step 7 — Apply the triggered cleanup`: replace
   the `Order:` paragraph ("Order: all run in parallel") and the four bullets
   (lines 393-406) with:

   ```text
   Its dispatches are the step script's rows for this step (Implement and
   propagate step).
   ```

7. `change-workflow.md`, `#### Step 8 — Verify the acceptance table`: delete
   the `Order:` paragraph (lines 410-413, "Order: after Step 7 the `runner`
   runs first"), the `runner` bullet (lines 415-425), and the
   `/verify-acceptance` bullet (lines 426-430). The `/create-follow-up-plans`
   bullet and the `Leftovers:` paragraph stay unchanged.
8. `.agents/skills/coherence-review/SKILL.md` `## When to use`, first bullet:
   replace "under the condition the `/coherence-review` combined-pass bullet in
   the Change Workflow Review and resolve correctness step states" with
   "whenever `.agents/scripts/Get-ChangeWorkflowSteps.ps1` lists
   `/coherence-review` in combined mode"; rewrap the bullet.
9. `.agents/skills/coherence-review/references/worker.md` `## Steps` step 5,
   first sentence: replace "every reviewer-role check the Change Workflow Apply
   the triggered cleanup step lists — today the `/external-skill-creator`
   findings-only validate mode and `/progressive-disclosure-review`, each on
   the trigger that step states" with "every reviewer-role check the brief
   lists as carried — the `/external-skill-creator` findings-only validate
   mode and `/progressive-disclosure-review` when the step script's combined
   row names them"; rewrap the step.
10. `.agents/skills/progressive-disclosure-review/SKILL.md` `## When to use`,
    first bullet: replace "whenever the session changed `AGENTS.md`,
    `.agents/skills/**/*.md`, `.agents/references/**/*.md`, or
    `Documents/Architecture/**/*.md`." with "whenever the session change
    inventory's `progressiveDisclosureReview` trigger fires."; rewrap the
    bullet.

## Out of scope

- `Write-SessionWorkflowContext.ps1`, its budget, and the four
  `session-context-part` markers.
- Steps 1-3, Step 9, `### Convergence`, `### Subagents`,
  `### Delegation roles`, and `## Main-session conduct` of
  `change-workflow.md`; every Steps 4-8 sentence not named under `## In scope`.
- Every other inventory trigger, the inventory's schema version, its
  `-Landing` skeleton logic beyond the class list, and
  `Invoke-StaticChecks.ps1`.
- Other skills' `## When to use` and every skill's frontmatter `description`,
  including `/over-engineering-review`, `/glsl-review`, `/repo-code-review`,
  `/comment-review`, `/code-style-review`, `/update-claude-docs`,
  `/update-affected-code`, `/verify-acceptance`, and
  `/plan-simplicity-review`; `/prepare-change` and `/next-plan` brief text;
  `.agents/references/execution-card.md`.
- Line-number citations in `Documents/Investigations/`.
- Later stage — pre-edit card search: an `-ExecutionCard <plan file path>`
  input that finds the checks marked `before the first edit` in the card's
  `Acceptance checks` field and reads whether `Roles` names the `runner`,
  replacing the Step 4 `Order:` paragraph's line-search sentences, the Step 4
  `builder` and `runner` pre-edit bullets, and the `-HarnessChecks` switch. It
  is independent of this stage and needs its own Plan.
- Later stage — plan-stage routing: a plan-stage mode listing the Approve and
  classify, Prepare and explore alternatives, and Plan review dispatches from
  the tier and a main-supplied `/plan-simplicity-review` trigger answer
  (`/plan-alternatives` axes by tier, `/plan-audit` at Tier 2+,
  `/external-grill-plan` at Tier 3, the difference review). The author does
  not recommend it now: those steps' deciding triggers are judgment and their
  tier gates are four short bullets.

## Coordination

One sibling Plan edits the same Steps 4-8 text:
`ChangeWorkflowTargetedConsolidation` lands after this Plan. Locate each region
by its key phrase; a region this Plan deletes needs no edit from the later
Plan.

## Risk tier

Tier 2 — one subsystem's tool behavior (the agent workflow's routing and its
inventory script); no determinism, wire, serialization, save, threading,
trust-boundary, or build/bootstrap surface.

Invariants:

- For each tier, landing state, `runner` assignment, and trigger set, the
  script lists exactly the dispatches, roles, scopes, and order the baseline
  prose of Steps 4-8 routes, apart from the added `binary` and `other`
  coherence classes and the single `/coherence-review` row.
- `Write-SessionWorkflowContext.ps1 -Part 1` through `-Part 4` still exit 0.
- The inventory's output schema and trigger keys are unchanged.

## Acceptance criteria

The pre-change baseline for criteria 1-5 is a read of
`.agents/references/change-workflow.md` Steps 4-8 at baseline
`591184a4dbf802ac2f6d3c2cf9de168916e4714d`; the expected rows below record
that read applied to each shape. Each run is
`pwsh -NoProfile -File .agents/scripts/Get-ChangeWorkflowSteps.ps1 -RepositoryRoot '<worktree root>' -Baseline <full SHA of commit^> -Head <full SHA of commit> -Tier <n> [-Landing] [-HarnessChecks]`.
Every historical landing commit also carries the generated
`.agents/skills/code-quality-metrics/references/history/*` files, so
`validateSkill` fires in every case; the baseline prose ("the session changed
any file in a `.agents/skills/*/` package") routes it the same way. Each run
exits 0 and its output is compared row by row (role, skill, mode, `after`)
with the list given.

1. Docs-only Tier 1 — `d55e39d2` (a deleted Feature document and its deleted
   Plan), `-Tier 1`: rows static checks; `/compile`; `/coherence-review`
   combined mode carrying `/external-skill-creator` validate mode and
   `/verify-acceptance` (read-only). No other rows.
2. C++ Tier 2 — `fd483a02` (ModelViewer C++, `Projects/ModelViewer/AGENTS.md`,
   deleted Plan), `-Tier 2`: `/update-affected-code`; static checks;
   `/code-style-review`; `/compile` after `/code-style-review`;
   `/over-engineering-review` finder then validator; `/repo-code-review` and
   `/comment-review` after the validator; `/coherence-review` coherence mode;
   `/external-skill-creator` validate mode; `/update-claude-docs`;
   `/progressive-disclosure-review` after `/update-claude-docs`;
   `/verify-acceptance`. No `/glsl-review`, `/adversarial-review`,
   `/update-vcxproj`, or `runner`.
3. Skill-package Tier 2 landing — `eb8c453a` (`change-workflow.md`, two
   `SKILL.md` files, deleted Plan), `-Tier 2 -Landing`: static checks;
   `/compile`; `/coherence-review` coherence mode; `/external-skill-creator`
   validate mode; `/progressive-disclosure-review` with no `after`. No
   `/verify-acceptance`, no `runner`, no C++ rows.
4. C++ Tier 3 with project membership — `fb42355e`,
   `-Tier 3 -HarnessChecks`: every row of case 2 plus `/adversarial-review`
   after every other Review and resolve correctness row, `/update-vcxproj`
   with 2 candidate paths, the `/compile` brief stating the `/agent-harness`
   trigger, and the `runner` before `/verify-acceptance`.
5. Shader Tier 2 — `843dd03d` (GLSL, `ShaderLayoutsBase.h`, C++), `-Tier 2`:
   the rows of case 2 plus `/glsl-review`, with `/comment-review` scoped to C++
   and GLSL.
6. `-Baseline 0000000000000000000000000000000000000000` exits 2 with one
   stderr line naming the inventory's `inventory.baseline-unresolved` code.
7. `pwsh -NoProfile -File .agents/scripts/Write-SessionWorkflowContext.ps1 -Part <n>`
   exits 0 for each `n` of 1-4.
8. `change-workflow.md` holds at least 500 fewer words (`wc -w`) than it held
   immediately before this Plan's edits (expected about 650 fewer).
9. In `change-workflow.md` Steps 4-8, a search for `when the change touches`,
   `` `cpp`-class file changed``, `when the session changed`,
   `for changed C++`, `after C++ or GLSL`, and `Tier 3 always` finds nothing,
   and `Order:` appears only in Steps 2, 3, and 4.
10. `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot '<worktree root>' -Baseline <session baseline> -IncludeUntracked`
    passes.
11. A code read of `$script:CoherenceReviewClasses` shows `binary` and `other`
    added (the user's choice) and the two exclusions in `Get-RoutingTrigger`
    unchanged.
