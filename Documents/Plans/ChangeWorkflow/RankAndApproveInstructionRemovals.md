<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T13:28:39.715Z","dependsOn":[]} -->
# Rank instruction-removal candidates and apply only the removals the user approves

## Context
The user read Anthropic articles on multi-agent design and context engineering.
The context-engineering article reports cutting over 80% of Claude Code's
system prompt with no loss, says rules added to prevent worst cases can often
go because newer models use judgement, and warns that rules pile up one
incident at a time until they overlap or conflict. The user asked for one change
in two parts: 1. consolidate the agent instruction prose where possible; 2. rank
suggested removals, which the user approves one by one.

When that work was claimed, the user chose to split it into two stages. Stage 1
(Part 1, meaning-preserving consolidation: every fact stated in more than one
in-scope place moved to its owning layer, each move logged) runs in the session
that wrote this Plan, and this Plan lands with it. This Plan is stage 2: Part 2.
It stands alone; nothing in it relies on the stage 1 Plan file, which is deleted
when stage 1 lands.

At the `/plan-alternatives` step of that claim, the user picked this two-part
approach over a ranking measured from session transcripts and over retiring the
end-of-run checkpoint.

User constraint: tokens are limited. There are no replay evals and no throwaway
worktrees that re-run old Plans.

Load facts the scores below rest on: root `AGENTS.md` reaches every main session
and every subagent. `.agents/references/change-workflow.md` and
`.agents/references/risk-tiers.md` reach every main session at start and no
subagent: Claude Code and Codex through the three `SessionStart` hook commands
in `.claude/settings.json` and `.codex/config.toml`, each running
`.agents/scripts/Write-SessionWorkflowContext.ps1 -Part N` (part 3 appends
`risk-tiers.md`); OpenCode through the agent prompt in `.opencode/opencode.json`.

## Design
Recommended order of work, with the rationale that the ranking must see the
consolidated text and the removals must follow the user's answers: the ranking
step finishes before the presentation starts, and the apply step starts only
after the user's last answer. These run one after another, not as parallel
slices.

### Ranking (an `implementer`)
Measure the in-scope token total before any edit (see
`## Acceptance criteria`), then build the ranked list in
`Temp/InstructionRemovalRanking.md`, a scratch file this Plan's executing
session creates in its own worktree (`Temp/` is gitignored; confirm with
`git check-ignore`). It is never tracked.

A candidate is a sentence, rule, or block in an in-scope file whose deletion
changes agent behavior. These are never candidates:
- text a script parses or copies — a pattern a script matches (for example the
  card field patterns in `.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1`
  and the handoff vocabulary lines
  `.agents/skills/external-skill-creator/scripts/Validate-Skill.ps1` matches) or
  wording a script copies as a fixed string. The author's reading:
  `Write-SessionWorkflowContext.ps1` injecting all of `change-workflow.md` and
  `risk-tiers.md` does not make their text script-copied, because it reads no
  wording except the three `session-context-part` marker lines, which are never
  candidates;
- text whose removal would break an invariant in `## Risk tier and invariants`.

Each candidate sits under its own `##` heading in the ranking file, so the main
session can read one entry at a time. Each entry holds:
- path and heading;
- a short quote;
- what it guards;
- why it may be removable;
- the risk if removed;
- its size in tokens, from
  `pwsh -NoProfile -File .agents/scripts/Measure-Tokens.ps1 <path> -StartLine <n> -EndLine <m>`
  on its line range;
- its score;
- every out-of-scope tracked file that cites the text or its heading (`none`
  when none does);
- every script that parses or copies the entry's text or its surrounding
  wording, searched for each entry (`none` when no script does; an entry whose
  own text a script reads is dropped from the list as not a candidate).

Score, the author's recommended formula (rationale: token cost, so rank highest
what is loaded most often and is most likely unneeded): load weight × size ×
removability.
- Load weight: 4 for root `AGENTS.md`; 3 for `change-workflow.md`,
  `risk-tiers.md`, `.agents/references/subagent-handoff.md`,
  `.agents/references/subagent-reporting.md`, and `.claude/agents/*.md`; 2 for
  an in-scope `SKILL.md`; 1 for any other in-scope file.
- Removability: 3, 2, or 1 for high, medium, or low likelihood that the text is
  no longer needed. High: it guards a mistake current models do not make,
  repeats what a script or hook already enforces, or conflicts with another
  rule. A rule enforced only by a Claude Code hook is still not redundant,
  because Codex and OpenCode rely on its prose. Medium: it answers a one-off
  incident. Low: none of these clearly holds.

The list is sorted by score, highest first.

### Presentation (the main session)
The main session presents the candidates to the user in rank order, each with
its entry's context in plain words per `.agents/references/change-workflow.md`
`### User Interaction`, and asks for a yes, no, or stop on each candidate. When
an entry names an out-of-scope citing file, the presentation says that file's
citing line would change too. A yes approves that removal with its mirrors and
any citing-line update the presentation named. A no drops that candidate. Stop
drops every candidate not yet answered. The main session collects the answers
and does not write the ranking file.

Presentation form — a question the executing session puts to the user at this
Plan's approval, not decided here:
- one candidate per message; or
- small batches of about 5 candidates per message, each candidate answered
  individually.

Approving the procedure approves no removal; each removal needs its own yes.

### Apply (a fresh `implementer`)
A fresh `implementer`, dispatched with a continuation capsule carrying the
user's answers, records each answer in the ranking file, marks every candidate
never presented as dropped, and applies each approved removal:
- in its in-scope file;
- with the same edit in its mirrors: a `.claude/agents/<role>.md` removal also
  in `.opencode/agent/<role>.md`, and in each `.codex/agents/*.toml` carrying
  the same rule;
- with the citing-line update in each out-of-scope file the presentation named.

No script is ever edited, so the citing-line exception never reaches a script.

## Critical files
- `AGENTS.md`
- `.agents/references/*.md`, especially `change-workflow.md` (`### Steps`,
  `### User Interaction`) and `risk-tiers.md`
- The in-scope skills' `SKILL.md` and `references/`
- `.claude/agents/*.md`, `.opencode/agent/*.md`, `.codex/agents/*.toml`
- `.agents/scripts/Measure-Tokens.ps1` (run only)
- `.agents/scripts/Write-SessionWorkflowContext.ps1` (run only; splits
  `change-workflow.md` at its three `session-context-part` markers and checks
  each part against a 9,500-character budget)

## In scope
- Root `AGENTS.md`
- Every file under `.agents/references/`
- `SKILL.md` and every file under `references/` of these skills under
  `.agents/skills/`: `adversarial-review`, `agent-harness`,
  `code-style-review`, `coherence-review`, `comment-review`, `compile`,
  `create-follow-up-plans`, `external-diagnose-bug`, `external-grill-plan`,
  `external-skill-creator`, `finalize-changes`, `glsl-review`,
  `implement-plan`, `next-plan`, `over-engineering-review`,
  `plan-alternatives`, `plan-audit`, `plan-simplicity-review`,
  `prepare-change`, `progressive-disclosure-review`, `repo-code-review`,
  `resolve-findings`, `update-affected-code`, `update-claude-docs`,
  `update-vcxproj`, `verify-acceptance`, `verify-external-claims` (the skills
  `change-workflow.md` `### Steps` names, including `/next-plan`, which that
  section names as the owner of Plan claims)
- `.claude/agents/*.md`, with the same edit in `.opencode/agent/*.md` and in
  the `.codex/agents/*.toml` text carrying the same rule
- In those files: only the removals the user approved one by one
- In an out-of-scope tracked file other than a script: only the citing-line
  update a presented candidate named and the user approved
- `Temp/InstructionRemovalRanking.md` (untracked)

## Out of scope
- Subsystem `AGENTS.md` files under `Engine/`, `Common/`, `Projects/`,
  `Tools/`, and `DataPacker/`, except an approved citing-line update
- Skills not in the `## In scope` list, including `/sweep` and
  `/next-plan-review`, except an approved citing-line update
- Any script, C++, shader, or settings change
- Adding any new rule, and any rewording beyond the approved removals and
  their citing-line updates
- Any removal the user did not approve one by one
- Further consolidation (stage 1 owned it)
- Replays, evals, or throwaway worktrees

## Risk tier and invariants
Tier 2, trigger: removals change agent-workflow tool behavior within the
instruction files of one subsystem (agent tooling); no determinism, wire,
serialization, threading, or trust-boundary surface changes, and the
session-start context budget is held by an existing script check. Approved
removals change skill behavior, so `/plan-simplicity-review`
`### Trigger: when a skill edit is behavior` applies at plan review.

Invariants:
- Every citation of removed text still resolves, including path-plus-heading
  citations in backticks and citations from out-of-scope files.
- Each `change-workflow.md` Step keeps its outline: every bullet keeps its role,
  skill, and trigger.
- `change-workflow.md` keeps its three `session-context-part` markers, and each
  part stays within the 9,500-character budget
  `Write-SessionWorkflowContext.ps1` checks.
- `.claude/agents`, `.opencode/agent`, and `.codex/agents` stay parallel.
- No new rule is added.

## Acceptance criteria
- Baseline: this Plan's own session measures the in-scope token total after
  stage 1 has landed and before its first edit, with `Measure-Tokens.ps1 -Json`
  over every in-scope path, invoked in the array form root `AGENTS.md`
  `## Directives` prescribes; stage 1 changes that total, so no earlier figure
  is current. The same measurement on the finished tree over the same paths is
  taken at the end. Both totals are recorded in the landing handoff.
- `Temp/InstructionRemovalRanking.md` holds every candidate the ranking step
  listed, with all the entry fields, sorted by score descending, the user's
  answer on each presented candidate, and every remaining candidate marked
  dropped.
- The diff holds only the removals the user approved, with their mirrors and
  each approved citing-line update, checked by `/coherence-review` against the
  ranking file's answers.
- For every removed heading or block, a repository-wide search of tracked files
  finds no path-plus-heading citation or link left pointing at removed text
  (`Invoke-StaticChecks.ps1` `markdown-links` checks only inline Markdown links,
  so backtick citations need this search).
- `pwsh -NoProfile -File .agents/scripts/Write-SessionWorkflowContext.ps1 -Part 1`,
  and the same with `-Part 2` and `-Part 3`, each exit 0 on the finished tree.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing.
- Each `.claude/agents/<role>.md` body is identical to
  `.opencode/agent/<role>.md`, and each front-matter `description:` line
  matches its counterpart; every changed shared rule text matches in each
  `.codex/agents/*.toml` carrying it, and each `.codex/agents/*.toml`
  description carries the same meaning as the role definitions it mirrors
  (meaning checked in `/coherence-review`).
- Each `change-workflow.md` Step keeps every bullet's role, skill, and trigger
  (diff read).
- No added sentence states a rule absent at the baseline (diff read).

## Notes
- If the user answers no or stop on every candidate, the change has no tracked
  diff; the ranking file and the token totals are its record.
- The Codex agent files are named by model (`opus`, `sol`, `sonnet`) and mirror
  rule text, not files.
