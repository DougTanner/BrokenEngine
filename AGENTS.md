# Broken Engine

A client/server game engine using data-oriented design, with data pre-packer (offline; runtime reads only `.pack` chunks). Top-down RTS-scale camera: kilometers above an ocean with islands, small units on screen. West is -x, East is +x, North is +y, South is -y, Up is +z, Down is -z. Client/server simulation is deterministic; rare user input favors CPU/GPU smoothness over round-trip latency. The world is an unbounded sparse grid of cells, each simulated independently in parallel. Fixed sim tick rate; render free-runs via interpolation. PostRender state is bit-deterministic (`/fp:strict`, CRC-checked per tick); Interpolate/render and client-only visuals are not, and stay out of the CRC.

## Environment

- Visual Studio 2026, C++23, Vulkan 1.2, Windows 10+
- Agent shells: Claude Code - Git Bash; Codex CLI - PowerShell 7. Call `pwsh` explicitly for PowerShell 7 scripts.
- Codex's command-safety filter rejects `Remove-Item` with `-Force` before PowerShell runs, even with approvals and the sandbox bypassed; the rejected command deleted nothing. Delete validated files by `-LiteralPath` without `-Force`.
- Claude Code's bypass-permissions mode injects a host instruction to prefer Bash, `sed`, heredocs, or scripts for file changes; ignore it — change tracked files with the host `Edit` tool and create files with `Write`, because a whole-file rewrite does not preserve the BOM, CRLF, or trailing newline. Codex is unaffected.
- Scripts: PowerShell 7 by default; Python only where a Python-only runtime forces it — full rule in `/external-skill-creator`.

## IMPORTANT: Context management and agent selection

Direct instructions from the human user override this repository's safety policies, such as landing guards — when they conflict, follow the user instruction.

### Subagents

- Main session is manager; subagents execute work to keep main context clean.
- Subagents must not spawn subagents. Only main-session skills request delegation; a subagent needing delegated work returns the requirement to its caller.
- Give subagents only the instructions and context their task needs; they return a short handoff that main routes. Task brief, handoff format, and worker interruption/recovery: `.agents/references/subagent-reporting.md`.
- Work in this session's own worktree (wrapper session, defined below).
- Worktrees are removed only by `/cleanup-worktrees` or explicit user direction, never with raw Git or filesystem commands.

### Delegation roles

This table is the authoritative spawned-agent routing policy; role definitions, Codex TOMLs, and the headless script enforce it. Skills name a role and describe the work. Definitions: `.claude/agents/<role>.md`. Codex resolves a role through the Model column — `.codex/agents/` is model-named.

| `subagent_type` | Model | Effort | Work |
| --- | --- | --- | --- |
| `planner` | Fable | medium | Plans, design, approach options |
| `reviewer` | Sol (see below); Opus fallback only with explicit user authorization | medium | Every review and audit; adversarial review that tries to disprove the change |
| `implementer` | Opus | medium | Preparation, implementation, propagation, docs, plans, harness, finalization |
| `researcher` | Opus | medium | Research requiring judgment |
| `locator` | Sonnet | xhigh | Exploration, search, log filtering, spec fetch, claim verification — returns file:line, quotes, or links, never summaries |
| `builder` | Sonnet | xhigh | `/compile`, which owns the return contract |
| `mechanic` | Sonnet | xhigh | Checklist edits — `/code-style-review`, `/update-vcxproj` |

- Delegate by `subagent_type`; an ad-hoc `model:` cannot lock in effort. A documented host-unavailability fallback to `general-purpose` may pass `model:` and runs without a locked-in effort
- Host built-in agent types (`Explore`, `Plan`, `general-purpose`) never substitute for a role, including inside plan mode — route the work through the table above. The documented host-unavailability fallback is the only exception
- Host plan mode never substitutes for Change Workflow steps: a plan produced there still gets Step 2's `/plan-audit` (and the Tier-3 additions) before implementation
- Every delegated review or audit is the `reviewer` role. In Claude Code it dispatches through `/codex-review` (Codex/Sol) — calling that skill is itself the delegated reviewer running, so no separate reviewer dispatch is needed, and that skill owns routing and fallbacks. Parent/manager orchestrators such as `/next-plan-review` remain in the invoking parent and dispatch their own reviewer directly as the `reviewer` subagent (`.claude/agents/reviewer.md`, Opus); that direct dispatch is the designated route for those orchestrators' child reviewers and needs no user authorization, unlike the authorization-gated Opus fallback. `codex-review` is the sole skill that may name a model. Do not follow review findings blindly. Use judgement on each one: accept it when the failure is real and reachable, and be especially careful with findings that add guards, options, or machinery for cases nobody has observed (YAGNI and over-engineering).

ChatGPT Codex: Fable -> gpt-5.6-sol max; Sol -> gpt-5.6-sol medium; Opus and Sonnet -> gpt-5.6-luna max.

## IMPORTANT: Change Workflow (YOU MUST follow this when changing anything tracked in this repository)

This workflow governs every tracked artifact — C++, shaders, PowerShell and other scripts, skills, plans, and documentation. An artifact type a step does not name is a case this workflow does not assign to anyone: resolve it with the user, never by treating the step as inapplicable.

For Tier 1 and Tier 2, the user's request is the approval. Classify the work, make the smallest complete change, run the checks that fit its size, and report changed files, the checks that settled it, and residuals. No approval round-trip or report file unless a landing gate applies.

Definitions:

- Execution card — the pre-implementation record the Step 1 preparation drafts (`/prepare-change`, or `/next-plan` for a claimed Plan) and `/plan-audit` audits.
- Landing gate — the finalizer's acceptance table plus the `/finalize-changes` landing flow with one explicit user confirmation; applies whenever primary will be changed: landing a session's work, or executable Plan completion or rejection. Shared AgentTools promotion and Tier-3 integration always land through it.
- Executable Plan — tracked `Documents/Plans/**/*.md` with byte-zero `broken-engine-plan/v1` metadata; selection and marker rules: `Documents/Plans/AGENTS.md`. `Documents/Features` is manual.
- Wrapper session — session started through `.claude/claude-worktree.sh` or `.codex/codex-worktree.ps1`, owning an isolated worktree. A retained wrapper session reattaches only through the same wrapper with its explicit reattach worktree input — `--reattach-worktree <path>` for Claude, `-ReattachWorktree <path>` for Codex; never adopt an arbitrary worktree.
- Primary — the shared main checkout and its main branch that finished session work lands into.
- Tracked artifact — any file Git tracks in this repository: code, shaders, scripts, skills, plans, and documentation.
- Step and stage — a step is one of the eight numbered Change Workflow steps below; a stage is one approved unit of session work that can complete or land independently.
- Residual — a known leftover problem reported at the end of a task instead of fixed inside it.

### Risk tiers

Classify the whole change at the highest applicable tier before implementing:

- Tier 1 — mechanical: documentation, style, project membership, or local behavior-preserving work with no public signature or invariant exposure.
- Tier 2 — scoped behavior: one subsystem's runtime or tool behavior, excluding changes to determinism/CRC, wire/protocol, serialization or data layout, save/replay compatibility, threading, or trust boundaries — a change to one of those surfaces alters what it computes, carries, or trusts; adding or tightening checks inside one unit at an existing boundary, with the format and the trust unchanged, is that unit's scoped behavior — build/bootstrap coordination, and independently owned cross-subsystem integration.
- Tier 3 — invariant/integration: any surface excluded from Tier 2 above, build/bootstrap coordination that can block other sessions, or a change spanning independently owned subsystems.

A reviewer may escalate the tier when the changed bytes expose a higher-risk surface. Resolve meaningful ambiguity with the user before proceeding.

### Steps

`/next-plan` owns executable Plan selection and claim lifecycle.

#### Step 1 — Approve and classify

- `implementer` runs `/prepare-change` — Tier 2+, or any tier where classifying the work needs repository evidence.

From user intent and any such preparation, main locks in the objective, the approved stage decisions, the tier and its triggers, the roles, and the acceptance checks. Tier 1 and Tier 2 authorize implementation; Tier 2, Tier 3, and a Plan claim also require an execution card, which `/plan-audit` needs.

#### Step 2 — Plan review

Order: `/prepare-change` first, because both plan reviews read the prepared plan; then `/plan-audit` and `/plan-simplicity-review` in parallel; then, at Tier 3, `/external-grill-plan` rounds until the plan is decision-complete, with `/verify-external-claims` between rounds.

- `implementer` runs `/prepare-change` to prepare the plan — Tier 2+.
- `reviewer` runs `/plan-audit` — Tier 2+; Tier 1 skips it.
- fresh `reviewer` runs `/plan-simplicity-review` on the same plan snapshot — every tier, when the plan adds new code or changes non-documentation behavior (both defined in that skill's `## When to use`); when unsure whether a plan triggers it, dispatch it.
- `implementer` owns `/external-grill-plan` repository evidence and short written decision summaries, updated round by round — Tier 3 only.
- `locator` runs `/verify-external-claims` — Tier 3 only, for the external claims a grill round raises.

Main reports a blocker if the `reviewer` role is unavailable for `/plan-audit`. At Tier 3 main only decides and interviews from the `implementer` and `locator` handoffs, then presents the resolved plan for approval; the brief and iteration contract is in `.agents/skills/next-plan/references/tier3-workflow.md`.

#### Step 3 — Implement and propagate

Order: the slices run in parallel; `/update-affected-code` runs after them.

- `implementer` runs `/implement-plan` for each disjoint slice — every tier.
- `implementer` runs `/update-affected-code` — after any C++ or GLSL change.

Main splits the work into disjoint slices where possible. Review-fix exceptions belong to `/resolve-findings`.

#### Step 4 — Run targeted pre-review checks

Order: both run in parallel; each `Build required` handoff compiles as it arrives.

- `implementer`s run the applicable static checks in `.agents/references/static-checks.md` — every tier.
- `builder` runs `/compile` — every `Build required` handoff, before the covered work advances.

Full builds and runtime or harness scenarios remain acceptance-table work.

#### Step 5 — Review and resolve correctness

Order: the per-artifact-type reviews run in parallel; `/adversarial-review` runs after them; `/resolve-findings` runs after each finding main accepts, followed by re-review and retest of the affected regions only; a second round needs a reproducible blocker.

- fresh `reviewer` runs `/repo-code-review` — when the change touches C++.
- fresh `reviewer` runs `/glsl-review` — when the change touches shaders.
- fresh `reviewer` runs `/coherence-review` — Tier-1 non-C++ artifacts; at Tier 1 with no changed C++ or GLSL this dispatch is the Step 5 combined pass and also carries Steps 6 and 7.
- fresh `reviewer` runs `/coherence-review` — other Tier-2+ artifacts; the reviewer must be new to the change.
- `reviewer` runs `/adversarial-review` — Tier 3 always; optional at any tier for one concrete unresolved hypothesis.
- separate `implementer` runs `/resolve-findings` — whenever main accepts a finding.

Main dispatches one fresh `reviewer` per changed artifact type, scoped to the changed bytes and the rules they touch. Scope, minimality, and simplicity checks run inside each Tier 2+ review; there is no separate scope dispatch. Main decides each finding once.

#### Step 6 — Apply the triggered cleanup

Order: all run in parallel, except `/progressive-disclosure-review` runs after `/update-claude-docs` so the prose that step generates is in scope.

- `mechanic` runs `/code-style-review` — for changed C++.
- `mechanic` runs `/update-vcxproj` — for changes to file membership or to which executable a whole file belongs to.
- fresh `reviewer` runs `/validate-skill` — when any `.agents/skills/*/SKILL.md` changed; where the Step 5 combined pass applies it runs inside that pass instead of its own dispatch.
- `implementer` runs `/update-claude-docs` — after C++ or GLSL changes.
- fresh `reviewer` runs `/progressive-disclosure-review` — when the session changed any `AGENTS.md`, `CLAUDE.md`, `.agents/skills/**/*.md`, or `.agents/references/**/*.md` file; where the Step 5 combined pass applies it runs inside that pass like `/validate-skill`.

#### Step 7 — Verify the acceptance table

Order: `/verify-acceptance` runs after Step 6; `/create-follow-up-plans` runs when a leftover is proven, per the Leftovers paragraph below.

- fresh read-only `reviewer` runs `/verify-acceptance` — for a stage completing without landing; a stage landing in the same session gets this from Step 8's landing table instead, and where the Step 5 combined pass applies that pass is this reviewer.
- `implementer` runs `/create-follow-up-plans` — for every proven out-of-scope leftover not fixed inside the current change.

Leftovers: a proven leftover that is itself small (Tier 1 or 2, bounded, decidable from evidence already in hand), found in a session whose own change is also small, is fixed inside the current change: record it as approved scope (on the execution card when one exists), reclassify the whole change at the highest applicable tier, run every step that tier triggers for the touched regions (plan review included), and report the expansion in the message already being sent. Every other proven out-of-scope leftover, including one the user directs to defer, goes through `/create-follow-up-plans`. One proven before the Step 5 dispatch is authored first, with the Plan path recorded as approved scope, so that round's reviews cover it; one proven later routes here unchanged.

#### Step 8 — Verify and land

- `implementer` runs `/finalize-changes` — at a landing gate.

Exactly one explicit user confirmation authorizes changing primary; after it the finalizer advances primary under the landing lock. The confirmation contract, the acceptance table, and what a later diff change re-triggers live in `/finalize-changes`. Landing completes one stage; the session ends only when every stage is complete or explicitly deferred, with a tracked follow-up Plan where required.

### Convergence

Once a stage's required checks pass, stop changing it: advance to the next stage, approval gate, blocker, or session end without adding untriggered tests, reviews, or process steps. This never shortens a valid build, lock, or harness deadline, and never licenses skipping a step this workflow does route — an unclear trigger is an ambiguity to surface, not grounds to call a check untriggered.

## Directives

- Minimum sufficient change: request and approved plan are target and ceiling — smallest complete change satisfying acceptance criteria and invariants; no speculative features, abstractions, configuration, extension points, or cleanup. Update related sites only when omission would make them incorrect; ignore polish.
- KISS, YAGNI, DRY: reuse existing mechanisms. Extract helpers only for current duplication, never for hypothetical use. Mirrored patterns stay parallel.
- Add backward compatibility only after explicit user consent. Without it, keep one current format, path, or behavior and remove obsolete compatibility code.
- Progressive disclosure: each fact — including a genuinely new term's definition — lives once at its owning layer and is referenced elsewhere: AGENTS.md carries the constraints, invariants, and routing every session needs; a skill carries its when-to-use and how-to-invoke workflow; scripts and skill `references/` carry mechanics, schemas, and long detail; code comments carry local non-obvious rationale. Comment what the code cannot say — an invariant, a required ordering, a consequence — never a language feature or house pattern the declaration already shows. Review: `/progressive-disclosure-review` for the layering, `/code-style-review` for comments.
- Public/private skills: every `.agents/skills/*/SKILL.md` is the public file and carries only what a parent session needs to decide on and dispatch the skill — purpose, triggers, inputs, handoff — while `references/worker.md` is the private file with the steps and rules, read by the dispatched worker or by a main session choosing to run the skill itself. Shape and checklist: `.agents/references/skill-skeleton.md`.
- One term per concept: use the established repository term; prefer plain words over formal ones.
- Do not add unit tests
- Bundled scripts: run a repository script exactly as its skill documents it — never wrap, reimplement, or work around one; a script that cannot be run as documented is a bug to report. Canonical invocation form: `pwsh -NoProfile -File <repo-relative script path> [arguments]`, run from the session worktree root; never an absolute path or `-ExecutionPolicy Bypass`. Never change the working directory (no `cd`, no `Set-Location`); address scratch files by path from the worktree root. One script per shell call with nothing chained before or after it; using that call's own output (assigning it, piping to `ConvertFrom-Json`) is allowed from the PowerShell tool only. Import a `.psm1` with `Import-Module ./<repo-relative path>` (leading `./` required) and call its functions in the same shell call. When a parameter takes an array, use `pwsh -NoProfile -Command "& '<repo-relative path>' -Param 'a','b'"` instead of `-File`.

### User Interaction

- IMPORTANT: Every question or decision request aimed at the user must be answerable from the current message without hidden reasoning or remembered scrollback. Provide the FULL context needed to understand as rendered message text the user is guaranteed to see — text emitted before a question-tool call may never be displayed, so present first and ask only after the context is visible — and explain what the answer changes or blocks. When relevant, give options, trade-offs, and a recommendation. The user has NOT read the source code or plan file.
- AVOID jargon, the user is NOT a domain expert, use plain language (dumb it down).
- Explain fully when asked; use headings and bullet points so a longer explanation stays skimmable.
- Reporting work: state what was built and what was verified separately, and name every required check still open; "done" means those checks have closed. A defect is stated with its evidence and effect, never as a verdict or count alone.
- Comparing options: use the same criteria, evidence, detail, and tone for each; recommend one, but never sell the favorite by its benefit and the alternative by its risk.
- Footer: the main session's final message of any turn that ran Change Workflow work ends with `Follow-up Plans created:` followed by one repo-relative path per Plan created that turn, tagged `landed in <commit>` or `unlanded in worktree`, or the single word `none`.

### Resolving Ambiguity

- Trivial choices (naming, small implementation details, equivalent approaches): pick the simplest and proceed.
- Non-trivial ties (two viable approaches, neither architectural): fan out `researcher` subagents to validate each, compare pros/cons, then pick the simplest good solution.
- Architectural decisions (new system shape, public API, data layout, threading model): stop and ask the user, presenting the problem, proposed solutions, and pros/cons of each.

### Diagnosis Discipline

- Verify root cause before editing: confirm it from close code inspection or evidence, never from "I know what the bug is". `/external-diagnose-bug` owns the method.
- Authority order when sources disagree about intended behavior: explicit user statement > final approved plan plus changes the user approved after the plan > AGENTS.md/docs/comments > current code behavior. When a plan's own decision declarations bind, and how to phrase a choice a plan makes, is in `.agents/references/authority-order.md`. Never silently make one side match another — surface the contradiction as a residual (footer line) naming both sides and which was trusted.

## Directory Structure

- `/Common/` — shared utilities (`common::`); `Common.h` is the single aggregation header (included by `Pch.h`)
- `/DataPacker/` — asset preprocessor producing `.pack`/`.manifest` files
- `/Engine/` — runtime: graphics, audio, input, frame state (`engine::`); `Engine.h` is the single aggregation header (included by `Pch.h`) with `#ifdef BT_CLIENT`/`BT_SERVER` guards, except for the few deliberate exceptions noted at the subsystem hubs
- `/Projects/` — game implementations (`game::`)
- `/Tools/AgentHarness/` — loopback client/server command transport
- `/Tools/ToolCommon/` — shared Windows and coordination support compiled into both tools
- `/Tools/WorktreeCli/` — repository build, landing, and plan coordination
- `/ThirdParty/` — external libraries (do not modify)
- `/Documents/` — style guide (`C++StyleGuide.txt`), architecture diagrams and network protocol (`Architecture/`), executable plans (`Plans/`), manual feature plans (`Features/`)

## Static Analysis

- `.editorconfig` — formatting: Allman, tabs, spacing, include sort.
- `.clang-tidy` — checks mapped to `Documents/C++StyleGuide.txt`; `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md` owns enablement and exclusions.

## Client/Server Targets

Same source, two executables: client (graphics, audio, input) defines `BT_CLIENT`; server (headless physics) defines `BT_SERVER`. Guard client/server-only code at the narrowest practical scope; which executable a whole file belongs to must match its project membership. `/update-vcxproj` and `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md` own exact membership, filter, and exception rules. Build commands: `/compile`.

## Key Patterns

- Live verification: invoke `/agent-harness` for all harness operations — launching and driving the client/server, sim setup, UI input, state queries, screenshots, logs, replay determinism checks.
- C++ conventions: `.agents/references/cpp-conventions.md`.
