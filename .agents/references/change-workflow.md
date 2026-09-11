<!-- session-context-part: Context management, delegation, and main-session conduct -->
This file, `.agents/references/change-workflow.md`, carries the Change Workflow, the delegation rules, and the main-session conduct rules for the main session, delivered at session start.

## IMPORTANT: Context management and agent selection

### Subagents

- Main session is manager; subagents execute work to keep main context clean.
- Subagents must not spawn subagents. Only main-session skills request delegation; a subagent needing delegated work returns the requirement to its caller.
- Give subagents only the instructions and context their task needs; they return a short handoff that main routes. Handoff format: `.agents/references/subagent-handoff.md`. Task brief and worker interruption/recovery: `.agents/references/subagent-reporting.md`.
- While workers run, continue useful independent work when available; avoid unnecessary status checks, duplicating their investigation or implementation, and context-heavy work unless it advances a useful independent task. Waiting and recovery mechanics: `.agents/references/subagent-reporting.md`.

### Delegation roles

This table is the authoritative spawned-agent routing policy; role definitions and the Codex TOMLs enforce it. Skills name a role and describe the work. Definitions: `.claude/agents/<role>.md`. Codex resolves a role through the Model column — `.codex/agents/` is model-named.

| `subagent_type` | Model | Effort | Work |
| --- | --- | --- | --- |
| `planner` | Fable | medium | Plans, design |
| `reviewer` | Opus | medium | Every independent findings-only review and audit except `/comment-review`; adversarial review that tries to disprove the change |
| `implementer` | Opus | medium | Preparation, implementation, propagation, docs, plans, harness, finalization |
| `researcher` | Opus | medium | Research requiring judgment; approach options for /plan-alternatives |
| `locator` | Sonnet | xhigh | Exploration, search, log filtering, spec fetch, claim verification — returns file:line, quotes, or links, never summaries |
| `builder` | Sonnet | xhigh | `/compile`, which owns the return contract |
| `mechanic` | Sonnet | xhigh | Checklist edits — `/code-style-review`, `/update-vcxproj`; the findings-only checklist review `/comment-review` |

- Delegate by `subagent_type`; an ad-hoc `model:` cannot lock in effort. On Claude only, a documented host-unavailability fallback to `general-purpose` may pass `model:` and runs without a locked-in effort
- Host built-in agent types (`Explore`, `Plan`, `general-purpose`) never substitute for a role, including inside plan mode — route the work through the table above. The documented host-unavailability fallback is the only exception
- Host plan mode never substitutes for Change Workflow steps: a plan produced there still gets Step 3's `/plan-audit` (and the Tier-3 additions) before implementation
- Every independent findings-only review or audit the table above assigns to `reviewer` runs as that subagent (`.claude/agents/reviewer.md`), including `/next-plan-review`, which runs directly in one fresh reviewer. `/comment-review` remains the `mechanic` exception; same-context `/implement-plan` and `/update-claude-docs` audits remain with their implementer; and `/coherence-review` may make only the narrow caller-authorized meaning-preserving wording and formatting fixes its worker contract allows, followed by that contract's self-check. Do not follow review findings blindly. Use judgement on each one: accept it when the failure is real and reachable, and be especially careful with findings that add guards, options, or machinery for cases nobody has observed (YAGNI and over-engineering).

ChatGPT Codex: Fable -> Astra (gpt-6-astra medium); Opus -> Sol (gpt-5.6-sol medium); Sonnet -> Luna (gpt-5.6-luna max).

## Main-session conduct

### User Interaction

- IMPORTANT: Every question or decision request aimed at the user must be answerable from the current message without hidden reasoning or remembered scrollback. Provide the FULL context needed to understand as rendered message text the user is guaranteed to see — text emitted before a question-tool call may never be displayed, so present first and ask only after the context is visible — and explain what the answer changes or blocks. When relevant, give options, trade-offs, and a recommendation. The user has NOT read the source code or plan file.
- AVOID jargon, the user is NOT a domain expert, use plain language (dumb it down).
- Explain fully when asked; use headings and bullet points so a longer explanation stays skimmable.
- Reporting work: state what was built and what was verified separately, and name every required check still open; "done" means those checks have closed. A defect is stated with its evidence and effect, never as a verdict or count alone.
- Comparing options: use the same criteria, evidence, detail, and tone for each; recommend one, but never sell the favorite by its benefit and the alternative by its risk.
- Footer: the main session's final message of any turn that ran Change Workflow work ends with `Follow-up Plans created:` followed by one repo-relative path per Plan created that turn, tagged `landed in <commit>`, `unlanded in worktree`, or `executed in this change` for a Plan folded in by the Leftovers do-now rule, or the single word `none`. Each path also answers whether the Plan is worth doing in this session while its context is fresh: `do now: yes` when it is Tier 1 or 2, bounded, and every file and fact it needs was already read or changed this session, so doing it adds little new context; otherwise `do now: no`, with the failed condition in a few words. One line per Plan: `Documents/Plans/<area>/<Plan>.md — unlanded in worktree — do now: no (needs files not read this session)`. The verdict is an answer, never a question; the user decides, and it never displaces a pending landing question or the session-complete marker. Only the `/next-plan` session-complete marker may follow it.

### Resolving Ambiguity

- Non-trivial ties (two viable approaches, neither architectural): fan out `researcher` subagents to validate each, compare pros/cons, then pick the simplest good solution.
- Architectural decisions (new system shape, public API, data layout, threading model): stop and ask the user, presenting the problem, proposed solutions, and pros/cons of each.

<!-- session-context-part: Change Workflow, Steps 1-5 (continues in the next part) -->
## IMPORTANT: Change Workflow (YOU MUST follow this when changing anything tracked in this repository)

This workflow governs every tracked artifact — C++, shaders, PowerShell and other scripts, skills, plans, and documentation. An artifact type a step does not name is a case this workflow does not assign to anyone: resolve it with the user, never by treating the step as inapplicable.

For Tier 1 and Tier 2, the user's request is the approval. Classify the work, make the smallest complete change, run the checks that fit its size, and report changed files, the checks that settled it, and residuals. No approval round-trip or report file unless a landing gate applies.

Definitions:

- Execution card — the pre-implementation record the Step 2 preparation drafts (`/prepare-change`, or `/next-plan` for a claimed Plan) and `/plan-audit` audits; after approval it carries the approved scope before any dispatch cites the card.
- Landing gate — the finalizer's acceptance table plus the `/finalize-changes` landing flow with one explicit user confirmation; applies whenever primary will be changed: landing a session's work, or executable Plan completion or rejection. Shared AgentTools promotion and Tier-3 integration always land through it.
- Executable Plan — tracked `Documents/Plans/**/*.md` with byte-zero `broken-engine-plan/v1` metadata; selection and marker rules: `Documents/Plans/AGENTS.md`. `Documents/Features` is manual.
- Wrapper session — session started through `.claude/claude-worktree.sh` or `.codex/codex-worktree.ps1`, owning an isolated worktree. A retained wrapper session reattaches only through the same wrapper with its explicit reattach worktree input — `--reattach-worktree <path>` for Claude, `-ReattachWorktree <path>` for Codex; never adopt an arbitrary worktree.
- Primary — the shared main checkout and its main branch that finished session work lands into.
- Tracked artifact — any file Git tracks in this repository: code, shaders, scripts, skills, plans, and documentation.
- Step and stage — a step is one of the nine numbered Change Workflow steps below; outside this file a step is cited by its heading name (`the Plan review step`), never by its number, so renumbering here changes nothing elsewhere; a stage is one approved unit of session work that can complete or land independently.
- Residual — a known leftover problem reported at the end of a task instead of fixed inside it.

Risk tiers: `.agents/references/risk-tiers.md`.

### Steps

`/next-plan` owns executable Plan selection and claim lifecycle.

#### Step 1 — Approve and classify

- `implementer` runs `/prepare-change` — Tier 2+, or any tier where classifying the work needs repository evidence.

From user intent and any such preparation, main locks in the objective, the approved stage decisions, the tier and its triggers, the roles, and the acceptance checks. Tier 1 and Tier 2 authorize implementation; Tier 2, Tier 3, and a Plan claim also require an execution card, which `/plan-audit` needs.

#### Step 2 — Prepare and explore alternatives

Order: `/prepare-change` first at Tier 2+, because the alternative investigations need the drafted plan's objective and scope; at Tier 1 there is no plan file, so main briefs from the request. Then `/plan-alternatives` when its trigger fires. A chosen alternative returns to `/prepare-change` for a redraft before Step 3; the skill owns the claimed-Plan and Tier-1 routes.

- `implementer` runs `/prepare-change` to prepare the plan — Tier 2+.
- main dispatches one `researcher` per axis for `/plan-alternatives`, concurrently and blind — every tier, on `/plan-simplicity-review`'s trigger (that skill's `## When to use`): Tier 1 axis 1 (Reuse), Tier 2 axes 1-2 (adds Remove the need), Tier 3 axes 1-3 (adds Reshape). Each brief is the shared task-brief form with `Skill: /plan-alternatives`, the objective, the plan's `## In scope`/`## Out of scope` quoted in full as text (main's intended change at Tier 1), evidence paths, the tier, fixed user decisions, one line naming the drafted mechanism as candidate zero, and the assigned axis — never the plan's rationale, and never the plan file's path, in any field and in any form, positive or negative. Main compares the handoffs per that skill's `## Handoff` and asks the user only when a candidate is clearly better.

#### Step 3 — Plan review

Order: `/plan-audit` and `/plan-simplicity-review` in parallel; then `/verify-external-claims` for the requests the audit handoff raises; then, at Tier 3, `/external-grill-plan` rounds until the plan is decision-complete, with `/verify-external-claims` between rounds.

- `reviewer` runs `/plan-audit` — Tier 2+; Tier 1 skips it.
- fresh `reviewer` runs `/plan-simplicity-review` on the same plan snapshot — every tier, when the plan adds new code or changes non-documentation behavior (both defined in that skill's `## When to use`); when unsure whether a plan triggers it, dispatch it.
- `implementer` owns `/external-grill-plan` repository evidence and short written decision summaries, updated round by round — Tier 3 only.
- main runs `/verify-external-claims`, dispatching one `locator` as its evidence worker — Tier 2+, for the external claims a `/plan-audit` handoff raises, and at Tier 3 also for the claims a grill round raises.

Main reports a blocker if the `reviewer` role is unavailable for `/plan-audit`. At Tier 3 main only decides and interviews from the `implementer` and `locator` handoffs, then presents the resolved plan for approval; the brief and iteration contract is in `.agents/skills/next-plan/references/tier3-workflow.md`.

#### Step 4 — Implement and propagate

Order: the slices run in parallel; `/update-affected-code` runs after them.

- `implementer` runs `/implement-plan` for each disjoint slice — every tier.
- `implementer` runs `/update-affected-code` — after any C++ or GLSL change.

Main splits the work into disjoint slices where possible. Review-fix exceptions belong to `/resolve-findings`.

#### Step 5 — Run targeted pre-review checks

Order: the full applicable static pass and `/code-style-review` run after propagation; each `Build required` handoff compiles as it arrives and may run in parallel with the static pass, except that when the change touches C++ the pre-review build waits for `/code-style-review`. Focused implementation self-checks remain inside the implementation slices.

- `implementer` runs the full applicable static pass in `.agents/references/static-checks.md` after propagation — every tier, when the change touches C++ or GLSL; main cites that reference in the brief's `Governing paths` and the `implementer` reads it there. Otherwise main runs the one documented command in that reference itself.
- `mechanic` runs `/code-style-review` — for changed C++; a later `/resolve-findings` round that changes C++ re-runs it over the newly changed ranges before that round's `Build required` handoff compiles.
- `builder` runs `/compile` — every `Build required` handoff, before the covered work advances.

Full builds and runtime or harness scenarios remain acceptance-table work.

<!-- session-context-part: Change Workflow, Steps 6-9, Convergence, and Risk tiers (continued from the previous part) -->
#### Step 6 — Review and resolve correctness

Order: the per-artifact-type reviews run in parallel; `/adversarial-review` runs after them; `/verify-external-claims` runs whenever a review or `/resolve-findings` handoff raises requests, before main decides the dependent finding or resolution; `/resolve-findings` runs after each finding main accepts, followed by re-review and retest of the affected regions only; a second round needs a reproducible blocker.

- fresh `reviewer` runs `/repo-code-review` — when the change touches C++.
- fresh `reviewer` runs `/glsl-review` — when the change touches shaders.
- `mechanic` runs `/comment-review` — when the change touches C++ or GLSL.
- fresh `reviewer` runs `/coherence-review` — Tier-1 non-C++ artifacts; at Tier 1 with no changed C++ or GLSL this dispatch is the Step 6 combined pass and also carries Steps 7 and 8.
- fresh `reviewer` runs `/coherence-review` — other Tier-2+ artifacts; the reviewer must be new to the change.
- `reviewer` runs `/adversarial-review` — Tier 3 always; optional at any tier for one concrete unresolved hypothesis.
- main runs `/verify-external-claims`, dispatching one `locator` as its evidence worker — for the external claims a review or `/resolve-findings` handoff raises.
- separate `implementer` runs `/resolve-findings` — whenever main accepts a finding.

Main dispatches one fresh `reviewer` per changed artifact type, plus the `mechanic` for `/comment-review`, scoped to the changed bytes and the rules they touch. Scope, minimality, and simplicity checks run inside each Tier 2+ review; there is no separate scope dispatch. Main decides each finding once.

#### Step 7 — Apply the triggered cleanup

Order: all run in parallel, except `/progressive-disclosure-review` runs after `/update-claude-docs` so the prose that step generates is in scope.

- `mechanic` runs `/update-vcxproj` — for changes to file membership or to which executable a whole file belongs to.
- fresh `reviewer` runs `/external-skill-creator` in findings-only validate mode — when the session changed any file in a `.agents/skills/*/` package that has a `SKILL.md`; where the Step 6 combined pass applies it runs inside that pass instead of its own dispatch.
- `implementer` runs `/update-claude-docs` — after C++ or GLSL changes.
- fresh `reviewer` runs `/progressive-disclosure-review` — when the session changed any `AGENTS.md`, `CLAUDE.md`, `.agents/skills/**/*.md`, or `.agents/references/**/*.md` file; where the Step 6 combined pass applies it runs inside that pass like the `/external-skill-creator` validate mode.

#### Step 8 — Verify the acceptance table

Order: `/verify-acceptance` runs after Step 7; `/create-follow-up-plans` runs when a leftover is proven, per the Leftovers paragraph below.

- fresh read-only `reviewer` runs `/verify-acceptance` — for a stage completing without landing; a stage landing in the same session gets this from Step 9's landing table instead, and where the Step 6 combined pass applies that pass is this reviewer.
- `implementer` runs `/create-follow-up-plans` — for every proven out-of-scope leftover not fixed inside the current change.

Leftovers: a proven leftover that is itself small (Tier 1 or 2, bounded, decidable from evidence already in hand), found in a session whose own change is also small, is fixed inside the current change: record it as approved scope (on the execution card when one exists), reclassify the whole change at the highest applicable tier, run every step that tier triggers for the touched regions (plan review included), and report the expansion in the message already being sent. When the turn footer marks a follow-up Plan `unlanded in worktree` as `do now: yes` and the user directs it before the landing confirmation, main executes that Plan as approved scope of the current change regardless of the current change's tier, with the same recording, reclassification, and triggered steps; its Plan file is deleted from the worktree instead of landing, so nothing is claimed or completed. Every other proven out-of-scope leftover, including one the user directs to defer, goes through `/create-follow-up-plans`. One proven before the Step 6 dispatch is authored first, with the Plan path recorded as approved scope, so that round's reviews cover it; one proven later routes here unchanged.

#### Step 9 — Verify and land

- `implementer` runs `/finalize-changes` — at a landing gate.

Exactly one explicit user confirmation authorizes changing primary; after it the finalizer advances primary under the landing lock. The confirmation contract, the acceptance table, and what a later diff change re-triggers live in `/finalize-changes`. Landing completes one stage; the session ends only when every stage is complete or explicitly deferred, with a tracked follow-up Plan where required.

### Convergence

Once a stage's required checks pass, stop changing it: advance to the next stage, approval gate, blocker, or session end without adding untriggered tests, reviews, or process steps. This never shortens a valid build, lock, or harness deadline, and never licenses skipping a step this workflow does route — an unclear trigger is an ambiguity to surface, not grounds to call a check untriggered.
