<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T17:13:15.638Z","dependsOn":[]} -->
# Fix: /compile — Release-only targets missing from the dispatcher-facing Inputs contract

## Context
Observed symptom: a claimed Plan's acceptance criterion
(`Documents/Plans/Engine/RunExecutableLaunchFailureResult.md`, "DataPacker
Debug and Release build through `/compile`") passed preparation and
`/plan-audit` unchallenged. The builder dispatch for DataPacker Debug|x64 then
ran
`pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target DataPacker -Configuration Debug`
and was blocked with exit 2, code `parameter.configuration-invalid`, message
"DataPacker builds Release only; -Configuration Debug is not permitted."
(`.agents/skills/compile/scripts/Invoke-CompileBuild.ps1:194`). The criterion
was unmeetable as worded; the session carried it as a residual after one
blocked build.

Current tree at the time of recording: `.agents/skills/compile/SKILL.md`
`## Inputs` asks for "the targets and configurations to build" but never says
which targets accept which configurations. The Release-only rule lives only in
`.agents/skills/compile/references/worker.md` step 3 ("DataPacker builds Release
only", and the `-Configuration` parameter note naming DataPacker, WorktreeCli,
and AgentHarness as Release only), which is marked private to the executing
session, and is enforced by `$script:ReleaseOnlyTargets = @('DataPacker',
'WorktreeCli', 'AgentHarness')` in `Invoke-CompileBuild.ps1`. Dispatchers and
Plan authors who read only the `SKILL.md` contract therefore cannot see the
rule.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 277b0f5b-697e-4d36-9fc0-f76638f4153a
- Worktree/branch UUID: 296bc044-a856-45fc-8970-634c6662d408
- Session branch: claude/296bc044-a856-45fc-8970-634c6662d408
- Worktree: .claude\worktrees\BrokenEngine\296bc044-a856-45fc-8970-634c6662d408
- Landing ref: claude/296bc044-a856-45fc-8970-634c6662d408 (the observing
  session records and lands this Plan itself; the branch survives exactly as
  long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CompileReleaseOnlyTargetsInputs.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/296bc044-a856-45fc-8970-634c6662d408` in bounded
friction mode, supplying client `claude` and the conversation session ID
recorded above. Then make the smallest fix inside the `## In scope` boundary
below. If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author recommends stating the Release-only rule once in
`.agents/skills/compile/SKILL.md` `## Inputs`, on the targets-and-configurations
bullet — DataPacker, WorktreeCli, and AgentHarness build Release only, so a
dispatcher, Plan, or acceptance criterion must not request another
configuration for them — because `## Inputs` is the contract dispatchers and
Plan authors read. The worker.md step 3 bullet and parameter note can then
reference that owning statement instead of restating it, per the root
AGENTS.md progressive-disclosure directive. Rationale for not changing the
script: its block is correct and is the enforcement point; the defect is only
that the contract hides the rule.

## Critical files
- `.agents/skills/compile/SKILL.md`
- `.agents/skills/compile/references/worker.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/compile/SKILL.md`
  `## Inputs` (the targets-and-configurations bullet) and
  `.agents/skills/compile/references/worker.md` step 3 (the "DataPacker builds
  Release only" bullet and the `-Configuration` parameter note)

## Out of scope
- The landed change the session produced, including
  `Documents/Plans/Engine/RunExecutableLaunchFailureResult.md`
- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` and its
  Release-only enforcement
- Unrelated skills/scripts, including `/prepare-change` and `/plan-audit`; any
  transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (documentation only: a skill contract statement with no script
or behavior change); escalate to Tier 2 if root-causing moves the fix into
script behavior. Never embed transcript paths or home paths. The Release-only
fact stays stated once at its owning layer.

## Acceptance criteria
- `.agents/skills/compile/SKILL.md` `## Inputs` states that DataPacker,
  WorktreeCli, and AgentHarness build Release only, and worker.md no longer
  carries a second independent statement of that rule
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
- Cost observed: one blocked build and one carried residual.
