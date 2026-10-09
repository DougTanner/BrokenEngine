<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T22:55:13.213Z","dependsOn":[]} -->
# Fix: agent-harness `set_log_level` / plan-audit — a Plan's Verification asked for `Verbose` lines the build compiles out, and neither audit caught it

## Context

Observed symptom. A claimed Plan's `## Verification` told the harness run to set the client `Network` log level to `Verbose` and confirm the returned level, and five of its seven acceptance criteria read `kVerbose` lines: `Sim ceiling stall ended` (`Engine/Source/GameBase.cpp:278`), `Render clock rebase` (`:603`), `Render clock starved` (`:630`), `ClockSync` (`Engine/Source/Network/Client/ClientSessionRuntime.cpp:707`), and `NetworkSimulation::Dropped` (`Engine/Source/Network/NetworkSimulation.h:169`, `:174`). The harness worker sent `set_log_level` `Verbose` for client `Network` and server `Default` and `Network`; all three returned `{"effective":"Debug"}`. `Projects/BrokenEngineSandbox/Source/Pch.h:89` sets the compile floor `keLogLevelDefault = kDebug`, and `keLogLevelNetwork` inherits it (`:94`), so every one of those lines was compiled out. Those five criteria came back `BLOCKED`. The cost was an 808 s harness run that was mostly wasted, a second verification-only temporary `Pch.h` edit to lower the floor, a rebuild, and a rerun.

The floor is already documented in two places a Plan author or auditor would not reach from the harness command contract. `Common/Log/AGENTS.md:7` describes per-project compile floors in general. `Projects/BrokenEngineSandbox/Documents/AgentHarness/cross-cell.md:11` states this project's Network floor, but only inside one recipe. The engine-shared `set_log_level` contract, `.agents/skills/agent-harness/references/command-reference.md:16`, says only "Levels below a compile-time floor clamp upward". It does not say that a clamped level means the lines below it are absent from the binary, where the floor is set, or that reaching them needs a temporary floor edit. `/plan-audit` ran twice on the Plan. Its worker step 6 (`.agents/skills/plan-audit/references/worker.md:39-42`) lists "runtime verification where relevant", and neither audit read the `set_log_level` contract or checked the `keLogLevel*` lines in `Pch.h`, although both read that file for the `keNetworkSimulation` edit.

The claimed Plan itself was deleted on completion, so it is not the fix site. The misbehaving files are outside that Plan's `## In scope`, which covered engine networking code and its documentation only.

Session provenance (machine-local; not reproducible after cleanup). The Client through Worktree fields name the session that observed the friction — the session `/next-plan-review` must reach — while the `Landing ref` line names a ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 7e1a7312-98f6-437d-ad5c-61651f58a4f3
- Worktree/branch UUID: e61b4640-26bd-4b7a-a2c0-5690beb0de97
- Session branch: claude/e61b4640-26bd-4b7a-a2c0-5690beb0de97
- Worktree: .claude\worktrees\BrokenEngine\e61b4640-26bd-4b7a-a2c0-5690beb0de97
- Landing ref: claude/e61b4640-26bd-4b7a-a2c0-5690beb0de97 (the observing session records and lands this Plan itself).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/FlagCompiledOutLogLevelsInPlanVerification.md`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design

First root-cause the friction from the current tree and this Plan's `## Context`. Only when the transcript is genuinely needed, in a new session run `/next-plan-review claude/e61b4640-26bd-4b7a-a2c0-5690beb0de97` in bounded friction mode, supplying client `claude` and the conversation session ID above. Then make the smallest fix inside the `## In scope` boundary below. If root-causing shows the fix lies outside that boundary, surface it for re-planning instead of expanding scope.

Recommended shape (the author's recommendation, to be confirmed by the root-cause step):

1. In the `set_log_level` entry of `command-reference.md`, state that a returned effective level above the requested one means the lower-level lines are compiled out of that build rather than filtered. Point to where the floor is set: each project's `keLogLevel<Category>` constants in its `Pch.h`, with `Common/Log/AGENTS.md` as the owner of the mechanism. State that a run needing those lines lowers the floor through a verification-only temporary source edit, taken together with any other temporary edit the run makes. This is the primary fix, because it puts the fact where every harness brief and Plan `## Verification` author already looks for the command's contract.
2. Add one item to `/plan-audit` worker step 6: for each log line a check relies on, compare its `LOG` level with the project's compile floor for that category. Make this change only if the root-cause step confirms the audits had no route to the fact. Step 16 ("checks that cannot observe their claimed outcome") already covers the outcome in general terms.

No C++, determinism/CRC, wire, serialization, `.pack`, replay, or build surface changes.

## Critical files

- `.agents/skills/agent-harness/references/command-reference.md` — the `set_log_level` entry (`:16`)
- `.agents/skills/plan-audit/references/worker.md` — `## Steps` step 6 (`:39-42`)

## In scope

- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `set_log_level` entry in `command-reference.md` and, only as `## Design` step 2 bounds it, `/plan-audit` worker step 6

## Out of scope

- The landed change the session produced, and the deleted claimed Plan
- `Projects/BrokenEngineSandbox/Source/Pch.h` and every `keLogLevel*` value: the floors stay as they are
- `Common/Log/AGENTS.md` and `Projects/BrokenEngineSandbox/Documents/AgentHarness/cross-cell.md`, which already state the floor correctly
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Tier 2. Trigger: scoped tool behavior, which changes how harness runs and plan audits treat log levels, confined to one skill reference and one skill worker step. No build or bootstrap coordination. Never embed transcript paths or home paths.

## Acceptance criteria

- Starting only from the `set_log_level` contract in `command-reference.md`, a reader can tell that a clamped effective level means the lower-level lines are compiled out, find where the project floor is set, and see what a run that needs those lines must do.
- If step 6 is changed: a `/plan-audit` of a Plan whose checks rely on a `kVerbose` line, without lowering the floor in its temporary edit, reports that check as unobservable.
- The static-checks runner, invoked as `.agents/references/change-workflow.md` `#### Step 5 — Run targeted pre-review checks` documents it, reports every row the change triggers passing.
