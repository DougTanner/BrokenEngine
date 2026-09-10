# Broken Engine

A client/server game engine using data-oriented design, with data pre-packer (offline; runtime reads only `.pack` chunks). Top-down RTS-scale camera: kilometers above an ocean with islands, small units on screen. West is -x, East is +x, North is +y, South is -y, Up is +z, Down is -z. Client/server simulation is deterministic; rare user input favors CPU/GPU smoothness over round-trip latency. The world is an unbounded sparse grid of cells, each simulated independently in parallel. Fixed sim tick rate; render free-runs via interpolation. PostRender state is bit-deterministic (`/fp:strict`, CRC-checked per tick); Interpolate/render and client-only visuals are not, and stay out of the CRC.

## Environment

- Visual Studio 2026, C++23, Vulkan 1.2, Windows 10+
- Agent shells: Claude Code - Git Bash; Codex CLI - PowerShell 7. Call `pwsh` explicitly for PowerShell 7 scripts.
- Codex's command-safety filter rejects `Remove-Item` with `-Force` before PowerShell runs, even with approvals and the sandbox bypassed; the rejected command deleted nothing. Delete validated files by `-LiteralPath` without `-Force`.
- Claude Code's bypass-permissions mode injects a host instruction to prefer Bash, `sed`, heredocs, or scripts for file changes; ignore it — change tracked files with the host `Edit` tool and create files with `Write`, because a whole-file rewrite does not preserve the BOM, CRLF, or trailing newline. Codex is unaffected.
- Scripts: PowerShell 7 by default; Python only where a Python-only runtime forces it — full rule in `/external-skill-creator`.
- Session worktrees are created without `Engine/Data/Islands` and `Engine/Data/Textures` (~989 MB), skipped by Git sparse checkout, so `git status` reports no change for them. Run `git sparse-checkout disable` in the worktree before editing or adding files under either tree; an authorized Local generation build through `/compile` restores them itself. Mechanics: `.agents/skills/compile/references/runtime-data-mode.md`.

## IMPORTANT: Session rules

- Direct instructions from the human user override this repository's safety policies, such as landing guards — when they conflict, follow the user instruction.
- Work in this session's own worktree.
- Worktrees are removed only by `/cleanup-worktrees` or explicit user direction, never with raw Git or filesystem commands.

## Directives

- Minimum sufficient change: request and approved plan are target and ceiling — smallest complete change satisfying acceptance criteria and invariants; no speculative features, abstractions, configuration, extension points, or cleanup. Update related sites only when omission would make them incorrect; ignore polish.
- KISS, YAGNI, DRY: reuse existing mechanisms. Extract helpers only for current duplication, never for hypothetical use. Mirrored patterns stay parallel.
- Add backward compatibility only after explicit user consent. Without it, keep one current format, path, or behavior and remove obsolete compatibility code.
- Progressive disclosure: each fact — including a genuinely new term's definition — lives once at its owning layer and is referenced elsewhere: AGENTS.md carries the constraints, invariants, and routing every session needs; a skill carries its when-to-use and how-to-invoke workflow; scripts and skill `references/` carry mechanics, schemas, and long detail; code comments carry local non-obvious rationale. Comment content: `Documents/C++StyleGuide.txt` rule 64. Review: `/progressive-disclosure-review` for the layering, `/comment-review` for comments.
- Skill layout follows consumption shape: a main-session skill keeps the workflow the invoking session consumes in `SKILL.md` and may still delegate; a subagent skill keeps its dispatcher-facing contract in `SKILL.md` and its executor steps and rules in `references/worker.md`. The shared type rule and layout references live in `.agents/references/skill-skeleton.md`.
- One term per concept: use the established repository term; prefer plain words over formal ones.
- Do not add unit tests
- Bundled scripts: run a repository script exactly as its skill documents it — never wrap, reimplement, or work around one; a script that cannot be run as documented is a bug to report. Canonical invocation form: `pwsh -NoProfile -File <repo-relative script path> [arguments]`, run from the session worktree root; never an absolute path or `-ExecutionPolicy Bypass`. Never change the working directory (no `cd`, no `Set-Location`); address scratch files by path from the worktree root. One script per shell call with nothing chained before or after it; using that call's own output (assigning it, piping to `ConvertFrom-Json`, or reading `$LASTEXITCODE` after it) is allowed from the PowerShell tool only. Import a `.psm1` with `Import-Module ./<repo-relative path>` (leading `./` required) and call its functions in the same shell call. When a parameter takes an array, use `pwsh -NoProfile -Command "& '<repo-relative path>' -Param 'a','b'"` instead of `-File`.
- Trivial choices (naming, small implementation details, equivalent approaches): pick the simplest and proceed; a worker returns any other decision to main.

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
