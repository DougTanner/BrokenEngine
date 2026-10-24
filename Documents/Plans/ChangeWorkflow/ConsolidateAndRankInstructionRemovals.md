<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T12:28:37.658Z","dependsOn":[]} -->
# Consolidate instruction prose, then rank removal candidates for one-by-one user approval

## Context
The user read Anthropic articles on multi-agent design and context engineering.
The context-engineering article reports cutting over 80% of Claude Code's
system prompt with no loss. It says rules added to prevent worst cases can often
go, because newer models use judgement. It also warns that rules pile up one
incident at a time until they overlap or conflict.

At this Plan's baseline, the repository's instruction prose is about 1 MB and
holds about 600 occurrences of "never". The user asked for one Plan with two
parts: 1. consolidate where possible; 2. rank suggested removals, which the user
approves one by one. The user also noted that
`.agents/references/change-workflow.md` is injected only into the main session
by the `SessionStart` hook, not into subagents.

User constraint: tokens are limited. There are no replay evals and no throwaway
worktrees that re-run old Plans.

## Design
Scope, the author's recommendation (rationale: token cost, so rank what is
loaded often): root `AGENTS.md`; `.agents/references/**`; the `SKILL.md` and
`references/` of each skill the Change Workflow dispatches (listed in
`## In scope`); and `.claude/agents/*.md`, with `.opencode/agent/*.md` and the
`.codex/agents/*.toml` text that mirrors them kept parallel.

### Part 1 — Consolidate
Meaning-preserving only. Applied without asking.
- A fact stated in more than one in-scope place moves to its owning layer, per
  the progressive-disclosure directive in root `AGENTS.md` `### Directives`.
  Every other place cites it by path plus `##` heading.
- Rare-case detail in `change-workflow.md` moves into the skill that owns the
  case. Each Step keeps its outline: every bullet keeps its role, skill, and
  trigger. A block moves only when the session that acts on the case loads the
  destination skill at that moment; otherwise it stays. Two examples to test
  against that rule: the `#### Step 5 — Run targeted pre-review checks`
  temporary-source-edit save and restore procedure, and the
  `#### Step 8 — Verify the acceptance table` Leftovers `do now` rules.
- No rule's meaning changes. Every move is recorded in a `Temp/` move ledger:
  source path and heading, destination path and heading, and the moved text,
  so reviewers can check that meaning is kept.

### Part 2 — Rank removal candidates
After Part 1, build a ranked list of removal candidates in the `Temp/` file
`Temp/InstructionRemovalRanking.md` (not tracked). A candidate is a sentence,
rule, or block whose deletion changes behavior.

Each entry has: path and heading; a short quote; what it guards; why it may be
removable; the risk if removed; its size in tokens from
`.agents/scripts/Measure-Tokens.ps1` on its line range; and its score.

Score, the author's recommended formula: load weight × size × removability.
- Load weight: 4 for root `AGENTS.md` (every main session and every subagent);
  3 for `change-workflow.md`, `.agents/references/risk-tiers.md` (both injected
  into every main session), `.agents/references/subagent-handoff.md`,
  `.agents/references/subagent-reporting.md`, and `.claude/agents/*.md`; 2 for
  an in-scope `SKILL.md`; 1 for any other in-scope file.
- Removability: 3, 2, or 1 for high, medium, or low likelihood that the text is
  no longer needed. High means it guards a mistake current models do not make,
  repeats what a script or hook already enforces, or conflicts with another
  rule. A rule enforced only by a Claude Code hook is not redundant, because
  Codex and OpenCode rely on its prose. Medium means it answers a one-off
  incident. Low means none of these clearly holds.
The list is sorted by score, highest first.

The executing main session presents the candidates to the user one at a time,
in rank order, each with its entry's context. An approved removal is applied in
this change, with its mirrors. A declined candidate is dropped. When the user
stops, every remaining candidate is dropped. The user's answer to each
presented candidate is recorded in the ranking file.

## Critical files
- `AGENTS.md`
- `.agents/references/*.md`
- `.agents/references/change-workflow.md` — `### Steps`: the Run targeted
  pre-review checks and Verify the acceptance table steps
- The in-scope skills' `SKILL.md` and `references/`
- `.claude/agents/*.md`, `.opencode/agent/*.md`, `.codex/agents/*.toml`
- `.agents/scripts/Write-SessionWorkflowContext.ps1` (run only; it splits
  `change-workflow.md` at its three `session-context-part` markers)
- `.agents/scripts/Measure-Tokens.ps1` (run only)

## In scope
- Root `AGENTS.md`
- Every file under `.agents/references/`
- `SKILL.md` and `references/` of these skills under `.agents/skills/`:
  `adversarial-review`, `agent-harness`, `code-style-review`,
  `coherence-review`, `comment-review`, `compile`, `create-follow-up-plans`,
  `external-diagnose-bug`, `external-grill-plan`, `external-skill-creator`,
  `finalize-changes`, `glsl-review`, `implement-plan`, `next-plan`,
  `over-engineering-review`, `plan-alternatives`, `plan-audit`,
  `plan-simplicity-review`, `prepare-change`, `progressive-disclosure-review`,
  `repo-code-review`, `resolve-findings`, `update-affected-code`,
  `update-claude-docs`, `update-vcxproj`, `verify-acceptance`,
  `verify-external-claims`. These are the skills `change-workflow.md`
  `### Steps` names, including `/next-plan`, which that section names as the
  owner of Plan claims.
- `.claude/agents/*.md`, with the same edit in `.opencode/agent/*.md` and in
  the `.codex/agents/*.toml` text that carries the same rule
- Part 1 moves and the user-approved Part 2 removals in those files only
- The `Temp/` move ledger and `Temp/InstructionRemovalRanking.md`

## Out of scope
- Subsystem `AGENTS.md` files under `Engine/`, `Common/`, `Projects/`,
  `Tools/`, and `DataPacker/` (code documentation)
- Skills not in the `## In scope` list, including `/sweep` and
  `/next-plan-review`
- Any script, C++, shader, or settings change
- Adding any new rule
- Any removal the user did not approve one by one
- Replays, evals, or throwaway worktrees

## Risk tier and invariants
The executing session classifies the tier; Part 2 removals change skill
behavior. Invariants:
- Part 1 changes no rule's meaning, and every move is in the ledger.
- Every citation of moved or removed text still resolves.
- Each `change-workflow.md` Step keeps its outline: role, skill, and trigger
  per bullet.
- `change-workflow.md` keeps its three `session-context-part` markers, and
  each part stays within the budget `Write-SessionWorkflowContext.ps1` checks.
- `.claude/agents`, `.opencode/agent`, and `.codex/agents` stay parallel.
- No new rule is added.

## Acceptance criteria
- The `Temp/` move ledger lists every Part 1 move, and each move keeps the
  rule's meaning.
- `Temp/InstructionRemovalRanking.md` holds every candidate with all the entry
  fields, sorted by score, and the user's answer for each presented candidate.
- The diff beyond the Part 1 moves holds only the removals the user approved,
  with their mirrors.
- `pwsh -NoProfile -File .agents/scripts/Write-SessionWorkflowContext.ps1 -Part 1`,
  and the same with `-Part 2` and `-Part 3`, each exit 0.
- The token totals of the in-scope files before and after the change, from
  `Measure-Tokens.ps1`, are recorded in the handoff.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing.
