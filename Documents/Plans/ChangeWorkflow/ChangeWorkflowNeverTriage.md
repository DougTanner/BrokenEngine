<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T22:14:33.089Z","dependsOn":[]} -->
# Triage the "never" rules in the Change Workflow reference

## Context

`.agents/references/change-workflow.md` is delivered to every main session at
start, in four parts, by `.agents/scripts/Write-SessionWorkflowContext.ps1`
(the SessionStart hooks in `.claude/settings.json` and `.codex/config.toml`).
At baseline `591184a4dbf802ac2f6d3c2cf9de168916e4714d` the file holds 28
occurrences of `\b[Nn]ever\b` on 26 lines.

The user's fixed decisions, in their words:

- "28 "never"s -> big oof -> let's triage these into keep, re-state-positive,
  or try to make 'impossible' to happen".
- "Scripts should ALWAYS be used over agent-instructions".
- The workflow "is producing good results, so I don't want to go overboard
  here, but targeted re-writes/consolidations/clearer directives are something
  we should do".

The user's answers on the reviewed draft:

- Asked whether to block the host built-in agent types (`Explore`, `Plan`,
  `general-purpose`) through `.claude/settings.json` `permissions.deny`, the
  user answered "No".
- Asked whether to create a follow-up Plan for a hook rejecting plan paths in
  a `/plan-alternatives` brief, the user chose no follow-up Plan.
- Asked about keeping the `L313` prohibition and restating `L262` and `L306`
  without a script that extracts the execution card's marked acceptance checks
  or `Roles` field, the user answered "Keep".

The classes are the author's recommendation:

- restate where a positive sentence nearby or in the same sentence already
  carries the rule, or a positive form is no longer and no less clear — when
  the "never" clause only repeats its own sentence's positive half, the
  restatement is deletion;
- keep where the prohibition is the clearest form;
- impossible where a host setting or hook refuses the action — no row is in
  this class, because the user declined both mechanisms proposed for it.

Totals: restate 22, keep 6, impossible 0. The 22 restated occurrences are this
Plan's support file
`Documents/Plans/ChangeWorkflow/ChangeWorkflowNeverTriage/Triage.txt`: one row
per occurrence with its reason, edit, and meaning risk, and the exact text of
every edit (`E1` to `E17`). The 6 kept clauses are listed in Design step 2.

## Design

1. Apply every edit `E1`–`E17` from `Triage.txt` verbatim to
   `.agents/references/change-workflow.md`. Each edit's old text is exact
   apart from line breaks and indentation; after each edit, rewrap only the
   edited paragraph (or table row, which stays one line) at about 80 columns,
   per the root `AGENTS.md` prose-wrapping directive. `E2` merges two bullets
   into one and `E8` merges the race and hand-edited-file endings into one new
   bullet, as their new text shows.
2. Leave these kept clauses (baseline line and key phrase) untouched:
   - `L80` "never review status, departure from a drafted plan" — an exclusion
     list of observed failures in the `Comparing options` bullet; "invariants
     kept" sits in its positive list, so "only" alone would not stop counting
     a touched invariant surface as a cost.
   - `L203` "never the plan's rationale", `L204` "never the plan's
     `## In scope`/`## Out of scope` sections", `L205` "never the plan file's
     path", `L206` "`Governing paths:` never names a path under `Temp/`" — the
     Step 2 brief's blinding exclusions; the brief's positive field list does
     not exclude content placed inside a field, so no positive form is shorter
     and as strict.
   - `L313` "Never propose a new harness command or a hand-edited file only to
     observe it" — the user's "Keep"; it binds proposals by planners and
     reviewers, which the code-reading sentence before it does not carry.

Expected result: the file drops from 28 to 6 occurrences of "never" and from
4,356 to 4,275 whitespace-separated words (net −81; rewrapping changes no word
count). Measured part sizes after the edits, before rewrapping, are 7,776,
7,236, 8,509, and 6,997 characters (baseline 8,015, 7,357, 8,639, 7,023), so
every part stays inside the `$script:MaximumPartCharacters` budget of
`Write-SessionWorkflowContext.ps1`; rewrapping moves each by only a few
characters.

## Coordination

The sibling drafts `InstructionAuthoringPolicy` (the `### Delegation roles`
table columns) and `ChangeWorkflowStepChecklistScript` (Steps 4-8 prose) edit
the same file; the second depends on this Plan, so this Plan lands first.
Locate each edit by its old text's key phrase rather than its line number, and
drop an edit whose old text is already gone, reporting it as a residual against
acceptance criteria 2-4.

## Critical files

- `.agents/references/change-workflow.md` — the edited reference.
- `Documents/Plans/ChangeWorkflow/ChangeWorkflowNeverTriage/Triage.txt` — the
  ledger holding every edit's exact text.
- `.agents/scripts/Write-SessionWorkflowContext.ps1` — read only; its exit
  code enforces the four markers and the part budget.

## In scope

- `.agents/references/change-workflow.md`, only these regions:
  - `### Delegation roles`: the `locator` table row's Work cell (`E1`); the
    `Delegate by` and `Host built-in agent types` bullets (`E2`); the last two
    sentences of the `Do not follow review findings blindly` bullet (`E3`).
  - `### User Interaction`: the `IMPORTANT: Every question` bullet (`E4`); the
    `Reporting work` bullet's last sentence (`E5`); the `Comparing options`
    bullet's last sentence (`E6`); the `Footer` bullet's last sentence (`E7`).
  - `### Resolving Ambiguity`: the `Races` bullet's last sentence and the
    `Hand-edited files` bullet (`E8`); the `Questions a build` bullet (`E9`).
  - `## IMPORTANT: Change Workflow`: the opening paragraph (`E10`); the
    `Wrapper session` definition (`E11`); the `Step and stage` definition
    (`E12`).
  - `#### Step 3 — Plan review`: the last bullet, the difference
    `/plan-simplicity-review` (`E13`).
  - `#### Step 4 — Implement and propagate`: the opening paragraph (`E14`).
  - `#### Step 5 — Run targeted pre-review checks`: the static-checks bullet
    (`E15`) and the `builder` bullet (`E16`).
  - `### Convergence` (`E17`).

## Out of scope

- The kept clauses of Design step 2 and the whole Step 2 researcher brief
  paragraph holding `L203`–`L206`.
- Every other paragraph of `change-workflow.md`, including rewrapping any
  paragraph no edit touched; the session-context marker lines.
- `.claude/settings.json` and any hook or deny rule for a row (the user's "No"
  and no-follow-up decisions above); a script extracting the execution card's
  marked acceptance checks or `Roles` field (the user's "Keep").
- `.claude/agents/locator.md` (its description keeps "never summaries"),
  `.codex/config.toml`, and every `.codex/agents/` file.
- The same triage for `.agents/skills/next-plan/SKILL.md` (17 occurrences) and
  root `AGENTS.md` (16). Recommendation: both are worthwhile as separate Tier-1
  Plans, root `AGENTS.md` first — it reaches every session and every worker,
  and several of its "never" rules (directory changes, absolute script paths,
  computed deletions, bare `git stash`) are already refused by
  `Invoke-PreToolUseHook.ps1`, so their rows are cheap restatements. This Plan
  does not create them.

## Risk tier

Tier 1 — documentation: prose edits to one reference file, with no tool,
setting, or script change.

Invariants:

- `change-workflow.md` keeps exactly four session-context-part marker lines,
  and each of the four parts stays inside the
  `Write-SessionWorkflowContext.ps1` budget.
- No restated rule loses meaning: each `Triage.txt` row records its meaning
  risk, and none is above low.

## Acceptance criteria

1. `pwsh -NoProfile -File .agents/scripts/Write-SessionWorkflowContext.ps1 -Part <n>`
   exits 0 for each of `1`, `2`, `3`, `4`.
2. `change-workflow.md` holds exactly 6 matches of `\b[Nn]ever\b`, all inside
   the kept clauses of Design step 2.
3. Its whitespace-separated word count is 4,275 (baseline 4,356).
4. Every new text from `E1`–`E17` appears in the file, compared with
   whitespace normalized.
5. `Triage.txt` holds 22 rows; their line labels plus the 6 labels of Design
   step 2 name each of the 28 baseline occurrences exactly once.
6. `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot '<worktree root>' -Baseline <baseline SHA> -IncludeUntracked`
   reports no failure.
