<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T22:14:32.422Z","dependsOn":[]} -->
# Add one owner for the instruction-authoring policy and enforce it

## Context

Instruction prose keeps growing: `.agents/references/change-workflow.md` is
4,356 words and root `AGENTS.md` 1,459 at baseline `591184a4`, and the planning
session measured weekly growth of `.agents/references` plus root `AGENTS.md`
over the last 15 weeks. No repository-wide rule tells an author to remove or
rewrite before adding, to prefer a script over prose, or to keep prose free of
model names. Local fragments exist and do not reach most authors:

- `.agents/skills/external-skill-creator/references/authoring.md` step 11 and
  `references/semantic-review.md` (the `phrase each rule positively` bullet)
  state the positive-form rule for skill packages only; authoring step 10
  removes duplication only after drafting.
- `.agents/skills/progressive-disclosure-review/references/worker.md` step 3
  flags skill prose that a script should compute, for skill prose only.
- `.agents/skills/next-plan-review/references/concerns.md` (the enforcement
  order ending "state it in prose only when the judgment cannot be encoded",
  and removal-first ranking) applies only to that review's recommendations.

Model names in instruction prose: the `### Delegation roles` table in
`change-workflow.md` routes roles to `.codex/agents/astra.toml`,
`astra-planner.toml`, `opus.toml`, and `sonnet.toml`; those files' names and
`developer_instructions` ("Follow repository instructions for Opus-assigned
work", "Fable-assigned work", "Sonnet-assigned work") name models, and
`opus.toml` does not even run the model its name suggests.
`.codex/agents/sol.toml` is routed by nothing. The Claude Code side already
names its role files by role (`.claude/agents/<role>.md`) with role-worded
bodies.

User decisions, in the user's words:

- "I asked for removals or re-writes to be prioritized over additions in
  whatever skill handles this? This clearly wasn't done or needs to be
  strengthened"
- "Scripts should ALWAYS be used over agent-instructions, isn't this a codebase
  policy already?"
- "Very little should be model-specific, this sounds like an anti-pattern that
  should be cleaned up/prevented from happening again"
- The workflow "is producing good results, so I don't want to go overboard".

User decisions on this draft, as the planning session recorded the user's
replies to the options it presented:

- Rename the Codex role definitions to one file per role and remove `sol.toml`:
  the user answered "Yes" (Design section 6).
- Trigger width: the user chose Option 1, widening the `/plan-simplicity-review`
  trigger to growth-only instruction edits and to root `AGENTS.md`, `.claude/`,
  and `.codex/` (Design section 3).
- Size measure: the user chose Option 1, bt-token-v1 through the existing
  counter (Design section 2, `instruction-size`).
- The `instruction-size` row: the user chose to keep it and give it one
  consumer, a one-sentence comparison in `/progressive-disclosure-review` step
  4 of the reported net size with the size the plan said it would add (Design
  section 4).

Triage of existing prohibition text is a separate Plan; this Plan sets the rule
for new text, adds its checks, and removes the model-named Codex role files.

## Design

Recommended: one short directive in root `AGENTS.md` `## Directives` owns the
policy, one existing review per stage enforces it, and the static-checks runner
does the mechanical part. Rationale: root `AGENTS.md` is loaded by every Claude
Code and Codex session and worker, so every planner, implementer, and reviewer
has the rule without an extra read; `authoring.md` reaches only
`/external-skill-creator` author mode, which most instruction edits never
enter. No new skill, step, or reference file.

### 1. The directive

Insert this bullet into root `AGENTS.md` `## Directives`, directly after the
`Progressive disclosure` bullet:

```markdown
- Instruction changes: in root `AGENTS.md` and the Markdown and role
  definitions under `.agents/`, `.claude/`, and `.codex/`, remove or rewrite
  existing text before adding any, and add only what that cannot cover. Make a
  rule a script, hook, or tool result wherever a program can decide it, leaving
  prose for judgment. State each rule as the action to take; keep a prohibition
  only where no positive form exists, paired with what to do instead, and
  prefer making the wrong action impossible. Name roles, not models; the
  static-checks `model-names` row owns the exemptions.
```

"Model-specific" means instruction prose that names a model, model family, or
model slug, or makes an instruction depend on which model runs; the
`model-names` row (section 2) owns the terms and exemptions. Host-specific text
(Claude Code versus Codex) is not model-specific. The plan-time requirement to
state the net size change lives only in `/plan-simplicity-review` question 3
(section 3).

### 2. Mechanical checks in `.agents/scripts/Invoke-StaticChecks.ps1`

Add two rows, always present like the existing four. The instruction-prose set
is: path `AGENTS.md`; a `.md` path under `.agents/` or `.claude/`; a `.toml`
path under `.codex/agents/` (ordinal, case-sensitive). No `Projects/Coretide/`
path is in the set.

- `instruction-size` (report only): triggered when an inventory entry's path
  or `oldPath` is in the set, deletions included. For each such entry, measure
  the baseline side (`oldPath` when present, else `path`; 0 when absent at the
  baseline or not in the set) and the head side (0 for a deletion or a path not
  in the set) exactly as `.agents/scripts/Measure-Tokens.ps1` measures a whole
  file: `Get-AgentNormalizedText` over the raw bytes, then
  `Measure-AgentTokenCount`. Detail: `netTokens` (sum of head minus baseline)
  and `files` (`path`, `baselineTokens`, `headTokens`). Status `pass`;
  `blocked` when the inventory is truncated; `skipped` when not triggered. Its
  one consumer is `/progressive-disclosure-review` step 4 (section 4).
- `model-names` (fails on a hit): triggered when a non-deleted set path
  changed. Terms are one fixed list in the script: the family names `opus`,
  `sonnet`, `haiku`, `astra`, `sol`, `luna`, and `fable`, plus `gpt-<digit>…`
  and `claude-<letters>-<digit>…` slugs. Match each term case-insensitively as
  a whole token (no adjacent letter, digit, underscore, or hyphen) on added
  lines only: `git diff -U0 -M` hunks between the baseline and the head, and
  every line of an untracked file, reusing the added-line approach of the GLSL
  indentation check. Skip any path
  under `.agents/skills/codex-bridge/`, and any line matching
  `^\s*model\s*[:=]` in a `.claude/agents/` or `.codex/agents/` file. Detail:
  `failures` (`path`, `line`, `term`). `blocked` when truncated.

Bump `schemaVersion` to `broken-engine-static-checks/v6`, change the "All four
rows" comment to six, and document both rows in the header comment. Exit-code
rules are unchanged: `instruction-size` never fails. Recommended: the fixed
list, the same names acceptance criterion 6 greps, because it is the smaller
mechanism: deriving terms from the role definitions' `model` values would need
frontmatter and TOML parsing and would still miss a name no definition
configures, such as the `Fable` in baseline `.codex/agents/astra.toml`.

### 3. Plan-time gate: `/plan-simplicity-review`

- `SKILL.md` `## When to use`: replace "or the skill or `.agents/references/`
  behavior defined in `### Trigger: when a skill or reference edit is
  behavior` below" with "or the instruction behavior defined in
  `### Trigger: when an instruction edit is behavior` below"; rename that
  heading to `### Trigger: when an instruction edit is behavior` and replace
  its paragraph with: "An edit to the files root `AGENTS.md` `## Directives`
  `Instruction changes` covers is behavior unless it only rewords, reorders,
  formats, retitles, or clarifies behavior that stays exactly as it was, and
  the plan states no net growth for it." This brings root `AGENTS.md`,
  `.claude/`, and `.codex/` edits and growth-only clarifications under the
  trigger, as the user chose (`## Context`); `/plan-alternatives` shares the
  trigger and fires for them too.
- `references/worker.md` question 3: append "For a step that adds instruction
  prose, the alternatives include those root `AGENTS.md`
  `Instruction changes` names; such a step whose plan does not state the net
  size change it expects the static-checks `instruction-size` row to report and
  what pays for each addition is a finding." This question is the one owner of
  that plan-time requirement.

`/prepare-change`, `/create-follow-up-plans`, `/save-plan`, and the
tooling-friction template stay unchanged: the directive is already in every
drafter's context, and this review runs on every such plan at save time and at
claim preparation.

### 4. Post-implementation gate: `/progressive-disclosure-review`

It already reviews every changed instruction-prose file, so it takes the
directive's judgment parts and the size comparison; the `model-names` row takes
model names.

- `SKILL.md`: in `description`, change "against the root AGENTS.md
  progressive-disclosure directive" to "against the root AGENTS.md
  progressive-disclosure and instruction-changes directives"; in `## Purpose`,
  name both `Progressive disclosure` and `Instruction changes` as the
  directives the skill enforces; in `## Inputs`, append after the
  `First-round findings` bullet:

  ```markdown
  - `Stated net size` — the net instruction size change the session's plan
    states; omitted when the session has no plan or the plan states none.
  ```

  and in `## Handoff`, extend the class list to
  `duplication | misplacement | size | instruction-change`. The plan file is
  the carrier, not the execution card: `/plan-simplicity-review` question 3
  already requires the plan to state the size, and the card template
  (`.agents/references/execution-card.md`) has no such field. A Tier-1 change
  has no plan, so step 4 compares nothing for it. Main supplies the input when
  it composes the brief from `## Inputs`
  (`.agents/references/subagent-reporting.md`), so the Change Workflow Apply
  the triggered cleanup step bullet stays unchanged.
- `references/worker.md`: in step 3, change "changed skill prose that computes"
  to "changed prose that computes"; insert a new step 4 and renumber the token
  step to 5:

  ```markdown
  4. Flag as `instruction-change` changed prose that breaks the root
     `Instruction changes` directive, leaving model names to the static-checks
     `model-names` row. When the dispatch supplies `Stated net size`, run the
     static-checks command of the Change Workflow Run targeted pre-review checks
     step and flag its `instruction-size` row's `netTokens` above that size as
     `instruction-change` naming both sizes. Done when every changed region the
     directive covers is checked and any stated size is compared.
  ```

  In `## Rules`, the precision guard becomes "every finding names the owning
  location, the exceeded threshold, or the replacing text or mechanism. No
  such name, no finding."

### 5. Remove the duplicated fragments

- `authoring.md`: delete step 11 and renumber steps 12-14 to 11-13 (no file
  cites these step numbers). Step 7 stays unchanged: root `AGENTS.md` is
  already in every author's context, so a pointer to the directive adds
  nothing.
- `semantic-review.md` `## Rules`: delete the `Recommended: phrase each rule
  positively` bullet; the `Progressive disclosure and body/reference size`
  bullet becomes "Progressive disclosure, the root `Instruction changes`
  directive, and body/reference size belong to
  `/progressive-disclosure-review`; report none of them here."

Kept, because each says more than the directive: `authoring.md` step 10
(speculative options and examples), `/progressive-disclosure-review`
`references/worker.md` step 3 (the concrete test for prose a script should
compute), and `next-plan-review` `references/concerns.md` (its enforcement
order and removal-first ranking also govern code fixes and control work).

### 6. Role-named Codex definitions

User decision (recorded in `## Context`): one Codex file per role, named and
`name`d exactly as the `subagent_type`, mirroring `.claude/agents/<role>.md`.
This removes every model name from Codex role prose and the table's
translation column, and makes "Delegate only by a `subagent_type` from the
table above" literally true on Codex. It is small (five small files out, eight
in) and preserves every role's current model and effort.

| New file | Settings source (baseline) | Handoff sentence |
| --- | --- | --- |
| `planner.toml` | `astra-planner.toml` | yes |
| `reviewer.toml` | `astra.toml` | yes |
| `implementer.toml` | `astra.toml` | yes |
| `researcher.toml` | `opus.toml` | yes |
| `locator.toml` | `sonnet.toml` | no |
| `builder.toml` | `sonnet.toml` | no |
| `runner.toml` | `sonnet.toml` | no |
| `mechanic.toml` | `sonnet.toml` | no |

Each new file: `name` = the role; `description` = the matching
`.claude/agents/<role>.md` `description`, verbatim; `developer_instructions` =
the matching Claude body's first sentence ("Follow repository instructions for
the assigned … role."), then "Never dispatch another agent; return delegated
work to the caller.", then, where the table says yes, the sentence "Author the
returned handoff per the field rules in `.agents/references/subagent-handoff.md`
`## Handoffs`."; `model` and `model_reasoning_effort` lines copied byte for
byte from the settings source. Delete `astra.toml`, `astra-planner.toml`,
`opus.toml`, `sonnet.toml`, and the unrouted `sol.toml`.

In `change-workflow.md` `### Delegation roles`, drop the `Claude Code
definition` and `Codex definition` columns, leaving `subagent_type` and `Work`,
and replace the paragraph's first two sentences with "This table owns the work
assignments. Each role is defined at `.claude/agents/<subagent_type>.md` for
Claude Code and `.codex/agents/<subagent_type>.toml` for Codex; the
definitions own model and effort settings." In
`.agents/skills/next-plan-review/references/measurement.md`
`## Verify execution-model routing`, change "the role table's Codex
definitions" to "the `.codex/agents/` role definitions".

## Net instruction size

Measured as the `instruction-size` row reports it (bt-token-v1), estimated from
the drafted text: about +580; the stated net size is at most +650.

- Additions: the directive (+147); the `/progressive-disclosure-review` step 4,
  `Stated net size` input, guard, and `SKILL.md` edits (+192); the
  `/plan-simplicity-review` trigger and question 3 (+85); the eight Codex files
  over the five removed (+348); the `measurement.md` phrase (+1).
- Paid by: removing `authoring.md` step 11 (-55), the `semantic-review.md`
  positive-form bullet net of its routing edit (-34), and the two definition
  columns of the role table net of the new paragraph (-104). The directive
  replaces the local fragments section 5 removes as the rule's owner; the
  copies section 5 keeps say more than it does.

## Critical files

- `AGENTS.md`
- `.agents/scripts/Invoke-StaticChecks.ps1`
- `.agents/skills/plan-simplicity-review/SKILL.md`,
  `.agents/skills/plan-simplicity-review/references/worker.md`
- `.agents/skills/progressive-disclosure-review/SKILL.md`,
  `.agents/skills/progressive-disclosure-review/references/worker.md`
- `.agents/skills/external-skill-creator/references/authoring.md`,
  `.agents/skills/external-skill-creator/references/semantic-review.md`
- `.agents/references/change-workflow.md`
- `.agents/skills/next-plan-review/references/measurement.md`
- `.codex/agents/*.toml`

## In scope

- `AGENTS.md` `## Directives`: the new `Instruction changes` bullet only.
- `.agents/scripts/Invoke-StaticChecks.ps1`: the header comment, the
  `schemaVersion` value, two new check functions with the helpers they need,
  and the `$checks` list with its row-count comment.
- `.agents/skills/plan-simplicity-review/SKILL.md`: the `Non-documentation
  behavior means` sentence in `## When to use` and the `### Trigger` heading
  and paragraph.
- `.agents/skills/plan-simplicity-review/references/worker.md`: question 3.
- `.agents/skills/progressive-disclosure-review/SKILL.md`: `description`,
  `## Purpose`, the new `Stated net size` bullet in `## Inputs`, and the class
  list in `## Handoff`.
- `.agents/skills/progressive-disclosure-review/references/worker.md`: step 3's
  "skill prose" phrase, the new step 4, the renumbered step 5, and the
  precision-guard rule.
- `.agents/skills/external-skill-creator/references/authoring.md`: step 11 and
  the renumbering of steps 12-14.
- `.agents/skills/external-skill-creator/references/semantic-review.md`: the
  two `## Rules` bullets named in Design section 5.
- `.agents/references/change-workflow.md` `### Delegation roles`: the
  paragraph's first two sentences and the table's header, separator, and
  definition cells.
- `.agents/skills/next-plan-review/references/measurement.md`: the one phrase
  named in Design section 6.
- `.codex/agents/`: the eight new files and the five deletions.

## Out of scope

- Rewriting existing prohibition or model-specific text beyond the files named
  above (the separate triage Plan), and rewrapping any paragraph not edited.
- `/prepare-change`, `/create-follow-up-plans` (including its tooling-friction
  template), `/save-plan`, `Documents/Plans/AGENTS.md`, and
  `.agents/skills/next-plan-review/references/concerns.md`.
- `/progressive-disclosure-review`'s trigger set (it stays without
  `.claude/` and `.codex/`; the two static rows cover those files).
- `.claude/agents/*.md`, `.codex/config.toml`, the `/codex-bridge` package and
  its presets, `/sweep` and its model listing, `Measure-Tokens.ps1`, and
  `AgentScriptCommon.psm1`.
- Bringing the Codex `reviewer` definition up to the Claude reviewer body's
  extra `BLOCKED` paragraph.
- `Documents/Features/Agents/CodexHostedReviewFastServiceTier.md` and
  `Documents/Investigations/ChangeWorkflow/MergingImplementerSteps.md`, which
  cite the removed file names: the Feature already names an unrouted file, and
  the Investigation is a dated record.
- Any new skill, Change Workflow step, reference file, or reviewer dispatch.

## Coordination

`.agents/references/change-workflow.md` `### Delegation roles` already carries
the never-rule triage: the `locator` row's Work cell ends at "links", and one
bullet beginning "Delegate only by a `subagent_type`" replaces the two bullets
that followed the table. This Plan keeps those edits, locating each shared
region by key phrase rather than line number: the paragraph that begins "This
table owns", the header row that begins "| `subagent_type` |", and each row by
its `subagent_type` cell. This Plan changes only that paragraph's first two
sentences and the definition columns, and leaves every `Work` cell unchanged.

## Risk tier and invariants

Tier 2 — scoped tool behavior in one subsystem (the Change Workflow tooling and
role definitions); no build/bootstrap coordination, no product surface.

Invariants:

- The static-checks runner stays read-only and writes nothing to disk; every row
  is always present; exit codes keep their meanings, and `instruction-size`
  never fails.
- Every role keeps its current Codex model and effort exactly.
- No `Projects/Coretide/` path enters either new row.

## Acceptance criteria

1. Root `AGENTS.md` `## Directives` holds the `Instruction changes` bullet with
   the Design section 1 text, directly after `Progressive disclosure`.
2. `git grep -n "prohibition only where"` matches root `AGENTS.md` and nothing
   under `.agents/`.
3. `git grep -n "Trigger: when a skill or reference edit is behavior"` matches
   nothing; the new heading and its one in-file reference match.
4. `authoring.md` steps run 1-13 without a gap; `/progressive-disclosure-review`
   worker steps run 1-5, its `## Inputs` holds `Stated net size`, and its class
   list includes `instruction-change`.
5. `.codex/agents/` holds exactly `planner.toml`, `reviewer.toml`,
   `implementer.toml`, `researcher.toml`, `locator.toml`, `builder.toml`,
   `runner.toml`, and `mechanic.toml`; each `name`
   equals its file stem, and each `model` and `model_reasoning_effort` line
   equals the settings source's line at the baseline
   (`git show <baseline>:.codex/agents/<source>.toml`).
6. `git grep -n -i -w -E "opus|sonnet|haiku|astra|sol|luna|fable" -- AGENTS.md
   .agents/references .codex`, whose names equal the `model-names` row's family
   list, matches only `model =` lines in `.codex/agents/*.toml`.
7. `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1
   -RepositoryRoot '<worktree root>' -Baseline <baseline SHA>
   -IncludeUntracked` exits 0 with `schemaVersion`
   `broken-engine-static-checks/v6` and six rows; `instruction-size` is
   triggered and `pass` with `netTokens` at most +650, and its `AGENTS.md`
   `headTokens` equals the `Tokens` that
   `pwsh -NoProfile -File .agents/scripts/Measure-Tokens.ps1 -Path AGENTS.md`
   reports; `model-names` and `validate-skill` are triggered and `pass`.
8. With one temporary line naming `Fable` added to
   `.agents/references/risk-tiers.md`, the runner's `model-names` row fails
   naming that path, line, and term; with the same line added only to
   `.agents/skills/codex-bridge/SKILL.md`, the row passes. Both temporary lines
   are removed and criterion 7 is rerun.
9. `pwsh -NoProfile -File .agents/scripts/Write-SessionWorkflowContext.ps1
   -Part 1` exits 0.
