<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-18T00:33:29.435Z","dependsOn":[]} -->
# Fix: /compile handoff — game-data row shortens GameDataDirectory, so /agent-harness claims the wrong data path

## Context
Observed symptom: during a `/next-plan` run, a `/compile` builder handoff for a
BrokenEngineSandbox game build reported its game-data `Decisive checks` row
with the `GameDataDirectory` value shortened by an ellipsis:
`/p:GameDataDirectory=...\Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\Output\Data`.
The build ran in Shared data mode, where that directory lives under the primary
checkout, not under the session worktree. Main expanded the elided prefix as the
session worktree root and dispatched `/agent-harness` with it; the claim step
`pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1 ... -GameDataDirectory '<session worktree>\Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\Output\Data'`
exited 1 with `claim.game-data-unreadable`, message "Pack manifests could not be
read: '<path>' is not a directory."
(`.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1:191` builds the
detail, `:314` emits the code). The full, unshortened path existed only in the
dispatch's gitignored build envelope file that the handoff's `Evidence` row
cites. Cost: one wasted `/agent-harness` dispatch and a second dispatch with the
corrected primary-checkout path.

Emitter: `.agents/skills/compile/SKILL.md` `## Handoff` asks the game-data
`Decisive checks` row for the envelope's `/p:DataBuildMode`, `/p:RunDataPacker`,
and `/p:GameDataDirectory` "switch values" only; nothing there, in
`.agents/skills/compile/references/worker.md`, or in
`.agents/references/subagent-handoff.md` requires that value to be the full,
unshortened path. The consumer, `.agents/skills/agent-harness/SKILL.md`
`## Inputs`, requires "the latest `/compile` result's ... normalized
`GameDataDirectory`", so the handoff field the consumer reads directly is the
one the emitter lets a worker elide.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 836c77ec-4656-4ba9-be6f-5ae767c31597
- Worktree/branch UUID: 83bf9ca2-a09b-4453-b4b0-626db191e6f6
- Session branch: claude/83bf9ca2-a09b-4453-b4b0-626db191e6f6
- Worktree: .claude\worktrees\BrokenEngine\83bf9ca2-a09b-4453-b4b0-626db191e6f6
- Landing ref: claude/83bf9ca2-a09b-4453-b4b0-626db191e6f6 (this observing
  session records and lands this Plan itself; the branch survives exactly as
  long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CompileHandoffFullGameDataDirectory.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode with the landing ref
above, supplying client `claude` and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author recommends requiring the game-data `Decisive checks` row to carry the
full normalized `GameDataDirectory` value verbatim, with no ellipsis or other
shortening, because `/agent-harness` `## Inputs` consumes that value directly
from the handoff and a shortened value cannot be expanded correctly without
knowing the data mode (Shared data lives under the primary checkout).

## Critical files
- `.agents/skills/compile/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the game-data `Decisive checks` row in
  `.agents/skills/compile/SKILL.md` `## Handoff`

## Out of scope
- The landed change the session produced
- `.agents/skills/compile/references/worker.md`, the compile scripts, the build
  envelope format, and `Invoke-HarnessClaim.ps1`
- `.agents/skills/agent-harness/SKILL.md` `## Inputs` (already correct)
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: the `/compile` handoff contract that
`/agent-harness` consumes); escalate if the fix reaches build/bootstrap
coordination. Never embed transcript paths or home paths.

## Acceptance criteria
- `.agents/skills/compile/SKILL.md` `## Handoff` requires the game-data row's
  `GameDataDirectory` to be the full normalized path, unshortened, so a Shared
  mode build's handoff names the primary-checkout data directory that
  `Invoke-HarnessClaim.ps1 -GameDataDirectory` accepts without
  `claim.game-data-unreadable`
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
