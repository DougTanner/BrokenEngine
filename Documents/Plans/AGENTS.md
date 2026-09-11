# Plans

Tracked refactor and bugfix plans. Capability additions in `../Features/` (see `../Features/AGENTS.md`) are manually executed and are never scheduler inputs.

## Git-backed scheduler

An executable plan starts at byte zero with exactly one metadata line:

`<!-- broken-engine-plan/v1 {"createdUtc":"2026-07-22T18:24:31.042Z","dependsOn":["Documents/Plans/Area/Prerequisite.md"]} -->`

Both keys are mandatory. `createdUtc` is immutable after creation. `dependsOn` is a unique ordinal-sorted list of normalized `Documents/Plans/**/*.md` paths. Every plan document in this tree carries the marker; a missing marker — including one preceded by a BOM, so it is not at byte zero — is a validation error naming the file. `AGENTS.md` and `CLAUDE.md` are exempt at every level of the tree and must never carry metadata.

WorktreeCli is the only component that parses the scheduler and changes claims. It selects the newest eligible executable plan by `(createdUtc descending, normalized path)`. Existing valid dependencies block a child; a missing dependency is a satisfied stale edge reported as a notice. A plan is excluded from selection — left out without affecting other plans, which stay claimable — when its metadata is invalid, its dependencies form a cycle, or it is not present and valid at the primary tip.

For a scheduler health check, run
`pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` — it folds `plan validate` into a compact status/diagnostics result; never run a raw whole-tree `plan validate`, whose result lists every Plan and floods a session context. To validate one Plan, run `Tools\WorktreeCli\Platforms\VisualStudio2026\Output\WorktreeCli.exe plan validate --lint-only --plan <normalized Plan path> --repo <absolute Git common directory> --worktree <session worktree root>`. The named Plan is valid when it appears in `plans`, which holds only that Plan; a present-but-invalid Plan still exits 0 with an empty `plans` array, an absent or untracked one exits 2 with `plan-not-found`, and `status`, `code`, and `diagnostics` describe the whole tree.

`/next-plan` validates then uses `plan claim-next`. Claims are PC-local, one per session, and fixed at 48 hours; a claim whose owning session or worktree no longer exists is orphaned, and self-healing releases an expired or orphaned claim automatically. Deferral is `plan unclaim`, which makes the plan immediately eligible again.

Completion uses `plan complete`; explicit rejection uses `plan reject --user-authorized-rejection`. Preparation removes direct child metadata edges and deletes the target in the Git worktree. After landing succeeds, the claim is deleted.

## Plan files

Plans live in area subdirectories, never directly at `Plans/`. The area is decided by the files the plan's critical-files and `## In scope` sections name, not by what motivated it:

- `Engine/` — the runtime and asset pipeline: `Engine/`, `Common/`, `DataPacker/`, shaders, and the `AGENTS.md` files that document them.
- `Game/` — the game built on the engine, under `Projects/`: gameplay collections, spawning and transfer, fleets, player input, game save and settings, and game-side agent commands.
- `Tools/` — the C++ tools under `Tools/`: WorktreeCli, AgentHarness, ToolCommon.
- `ChangeWorkflow/` — how agents work rather than what the product does: the root `AGENTS.md`, `.agents/`, `.claude/`, `.codex/`, skills and their scripts, the wrapper and scheduler scripts, and this file.

A plan spanning areas goes to the area owning most of its named files; a tie stays in `Engine/`.

An executable plan provides metadata, `# Title`, context, design, critical files, a required `## In scope` section naming the specific functions, members, or regions to change, required `## Out of scope` boundaries, risk triggers/invariants, and observable acceptance criteria when a diff is not decisive. Put directional prerequisites in metadata, not prose.

The two scope sections are the control the finished change is measured against, so write each boundary precisely enough to enforce: the Review and resolve correctness step's review of each changed artifact type treats a changed region no `## In scope` clause covers, or one an `## Out of scope` line names, as unauthorized.

A document presenting options rather than a decision-complete implementation belongs in `../Investigations/` (see `../Investigations/AGENTS.md`) until the decision exists. Work blocked on another change expresses that as a `dependsOn` edge; work blocked on a decision is not a Plan yet.
