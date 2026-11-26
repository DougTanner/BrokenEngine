# Plans

Tracked tasks ready to be picked up by another session through `/next-plan`.
Ideas saved for another day in `../Features/` (see `../Features/AGENTS.md`) are
manually executed and are never scheduler inputs.

## Authorship

Every Plan is agent-written. Its statements about what the user chose, asked, or
decided are the authoring agent's record, never the user's own words. Never
present Plan wording or a Plan-recorded choice to the user as the user's own
decision. When a Plan decision binds is in
`.agents/references/authority-order.md`.

## Git-backed scheduler

An executable plan starts at byte zero with exactly one metadata line:

`<!-- broken-engine-plan/v2 {"createdUtc":"2026-07-22T18:24:31.042Z","dependsOn":["Documents/Plans/Area/Prerequisite.md"]} -->`

`createdUtc` (immutable) and `dependsOn` (unique ordinal-sorted
`Documents/Plans/**/*.md` or `Projects/Coretide/Documents/Plans/**/*.md` paths)
are mandatory; optional `agentCli` (`claude`, `codex`, or `agy`) locks new
claims to that CLI. Every plan in this tree carries the marker (a BOM breaks
byte zero and fails validation); `AGENTS.md` is exempt at every level and never
carries metadata.

WorktreeCli is the only component that parses the scheduler and changes claims,
selecting the newest eligible plan by
`(createdUtc descending, normalized path)`. Existing valid dependencies block a
child; a missing dependency is a satisfied stale edge notice. A plan is excluded
without blocking other plans when its metadata is invalid, its dependencies form
a cycle, or it is absent or invalid at the primary tip. WorktreeCli derives the
active CLI from the worktree's `<cli>/<uuid>` session branch: a plan whose
session or primary `agentCli` differs is marked `excluded` by `plan list` on
session branches and refused for new claims (on non-session branches,
`plan list` stays unfiltered while `plan claim-next` refuses tagged plans unless
explicitly overridden on a targeted claim — syntax in
`Tools/WorktreeCli/AGENTS.md`). `agentCli` filtering of valid metadata never
invalidates a plan, never alters dependency blocking, cycle detection, or claim
healing, and never blocks refreshing an already-held claim.

For a scheduler health check, run
`pwsh -NoProfile -File .agents/scripts/Invoke-PlanSchedulerStateCheck.ps1` — it
folds `plan validate` into a compact status/diagnostics result; never run a raw
whole-tree `plan validate`, whose result lists every Plan and floods a session
context, except the whole-tree `--lint-only` run of
`.agents/skills/finalize-changes/references/landing-acceptance-table.md`
`## Executable Plan check`. To validate one Plan, run
`Tools/WorktreeCli/Platforms/VisualStudio2026/Output/WorktreeCli.exe plan validate --lint-only --plan <normalized Plan path> --repo '<absolute Git common directory>' --worktree '<session worktree root>'`.
The named Plan is valid when it appears in `plans`, which holds only that Plan;
a present-but-invalid Plan still exits 0 with an empty `plans` array, an absent
or untracked one exits 2 with `plan-not-found`, and `status`, `code`, and
`diagnostics` describe the whole tree.

`/next-plan` validates then uses `plan claim-next`. Claims are PC-local, one per
session, and last a fixed lifetime (`kiClaimLifetimeTicks`, in 100 ns ticks); a
claim whose owning session or worktree no longer exists is orphaned, and
self-healing releases an expired or orphaned claim automatically. Deferral is
`plan unclaim`, which makes the plan immediately eligible again.

Completion uses `plan complete`; explicit rejection uses
`plan reject --user-authorized-rejection`. Preparation removes direct child
metadata edges and deletes the target in the Git worktree. After landing
succeeds, the claim is deleted.

## Plan files

Plans live in area subdirectories, never directly at `Plans/`. The area is
decided by the files the plan's critical-files and `## In scope` sections name,
not by what motivated it:

- `Engine/` — the runtime and asset pipeline: `Engine/`, `Common/`,
  `DataPacker/`, shaders, and the `AGENTS.md` files that document them.
- `Game/` — the game built on the engine, under `Projects/`: gameplay
  collections, spawning and transfer, fleets, player input, game save and
  settings, and game-side agent commands.
- `Tools/` — the C++ tools under `Tools/`: WorktreeCli, AgentHarness,
  ToolCommon.
- `ChangeWorkflow/` — how agents work rather than what the product does: the
  root `AGENTS.md`, `.agents/`, `.claude/`, `.codex/`, skills and their scripts,
  the wrapper and scheduler scripts, and this file.

A plan spanning areas goes to the area owning most of its named files; a tie
stays in `Engine/`.

A Plan whose critical files or `## In scope` name any `Projects/Coretide/` file
lives at `Projects/Coretide/Documents/Plans/<Area>/<Name>.md` in the session
Coretide clone, follows every rule of this file, is selected, claimed, and
completed with the engine Plans, and lands with the Coretide commit. A Coretide
Plan may depend on an engine Plan; an engine Plan never names a Coretide Plan.

Every Plan sits exactly at `Documents/Plans/<Area>/<Name>.md`, or the Coretide
path above, and may own a support folder named after its file without `.md`
(`Documents/Plans/<Area>/<Name>/`), holding the files the Plan needs, every one
committed with the Plan. A ledger support file holds only rows the Plan acts on,
never excluded false positives. Each file keeps its real format and extension,
except that a Markdown file is saved as `.txt`: the scheduler reads every `.md`
under this tree as a plan document, so no `.md` file lives in a support folder.
Every support folder has its Plan beside it, and an area holds only Plans, their
support folders, and `AGENTS.md`. Delete a support folder only with
`pwsh -NoProfile -File .agents/scripts/Remove-PlanSupportFolder.ps1 -Plan '<Plan path>'`,
never with a deletion command of your own; `/next-plan`'s claim exit runs it for
every Plan it completes or rejects, and a Plan removed by hand takes its folder
through it too.

When primary deletes a Plan file the session has edited, restore the file to
its session baseline content so the landing takes primary's deletion; never
carry the edit forward or recreate the Plan.

An executable plan provides metadata, `# Title`, context, design, critical
files, a required `## In scope` section naming the specific functions, members,
or regions to change, required `## Out of scope` boundaries, risk
triggers/invariants, and observable acceptance criteria when a diff is not
decisive. Put directional prerequisites and large same-system dependencies in
`dependsOn` metadata (`.agents/references/new-plan-file.md` `## Invocation`),
not prose; minor file overlap between independent Plans is accepted.

A Plan permits at most two bounded open parts:

- An **investigation portion** for a proven, reproducible problem whose root
  cause is not yet confirmed: `## Context` records the observed symptom and its
  evidence; `## In scope` names the root-cause investigation and the region that
  bounds the fix; `## Design` has the executing session first confirm the root
  cause by the route it names (`/external-diagnose-bug` for a defect), then make
  the smallest fix inside that region, surfacing a fix outside it for
  re-planning instead of widening scope; and an acceptance criterion states that
  the recorded symptom no longer reproduces.
- A **`## Unresolved decisions` section**, allowed only for (a) open gaps
  embedded in an executable Plan saved by `/save-plan`, or (b) a follow-up Plan
  authored through `/create-follow-up-plans` under standing landing
  confirmation or for open gaps in a saved file bare `/next-plan` will not pick
  up. When filed as a follow-up Plan, `dependsOn` stays empty so it is
  immediately eligible for bare `/next-plan` selection. `## Context` follows
  the durable-evidence rule in
  `.agents/skills/create-follow-up-plans/references/worker.md` `## Rules`.
  `## Unresolved decisions` states options, trade-offs, and a recommendation.
  `## In scope` bounds what is authorized pending the user's answer: an empty
  realized change when no edit is predetermined before the user answers (using
  `.agents/skills/next-plan/SKILL.md` `## Rules`'s empty-realized-change path),
  or — for a `/save-plan` companion Plan — recording the resolved answer into
  the target saved file and updating or clearing any open sections there before
  completion so the decision survives companion deletion and `dependsOn`
  edge-stripping (`plan complete`).

Work that splits into stages (the term as
`.agents/references/change-workflow.md` `## IMPORTANT: Change Workflow` defines
it), each ending at a cut after which the tree builds and that stage's
acceptance criteria can pass, is written as one Plan per stage, whether or not
the stages edit the same files.

The two scope sections are the control the finished change is measured against
(`.agents/references/scope-authorization.md`), so write each boundary precisely
enough to enforce.

Whether a document belongs here follows the deciding test in `../AGENTS.md`
`## Planning Trees`. Work blocked on another change expresses that as a
`dependsOn` edge; work blocked on a decision outside the two open parts above is
not a Plan yet.
