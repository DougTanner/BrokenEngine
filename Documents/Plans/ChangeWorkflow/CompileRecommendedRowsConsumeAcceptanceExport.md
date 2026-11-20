<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T00:55:13.083Z","dependsOn":[]} -->
# Fix: /compile builder — recommendedTargets rows consume a later acceptance build's export

## Context
Observed symptom: during a `/next-plan` run whose change edited DataPacker
export-job headers (`DataPacker/Source/ExportJobs/ExportIsland.h`,
`DataPacker/Source/ExportJobs/ExportShader.h`) and `Common/DataFile.h`, main
dispatched a `/compile` builder to build only DataPacker Release as a compile
check, stating no agent-harness trigger. Following
`.agents/skills/compile/references/worker.md` step 2 (`recommendedTargets`
"not advisory") and step 3 ("Build the request's targets plus every
`recommendedTargets` row"), the builder also built the BrokenEngineSandbox
Client/Server Debug pair and ModelViewer Debug. Each of those game builds
resolved `dataBuildMode` Local (`inputs-differ`) and ran DataPacker on the
edited tree, which re-exported the Islands and Shader chunks into the shared
`DataPackerCache`.

The execution card's post-change acceptance check, dispatched next as its own
builder run, required a Client Release Local generation build whose DataPacker
log shows `"Islands" is dirty` and `"Shader" is dirty`. That run's DataPacker
step logged no `is dirty` line and no `Exported:` line, because the earlier
extra Debug builds had already consumed the one-time export. The check was not
met by the run dispatched for it; main accepted the earlier Debug run's log as
the evidence instead. The acceptance builder also built extra
`recommendedTargets` rows (Server Debug, ModelViewer Debug, DataPacker
Release) beyond its requested Client Release.

The worker text has no exemption for a dispatch whose extra Local game build
would consume a later acceptance build's one-time export; the only exemption
today is the interleaved-edit dispatch in `.agents/skills/compile/SKILL.md`
`## Inputs`.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 11a39967-dc22-4a15-bf55-4a77b15f6caa
- Worktree/branch UUID: 583d6a45-ee2d-4df1-b610-487ad6f08df3
- Session branch: claude/583d6a45-ee2d-4df1-b610-487ad6f08df3
- Worktree: .claude\worktrees\BrokenEngine\583d6a45-ee2d-4df1-b610-487ad6f08df3
- Landing ref: claude/583d6a45-ee2d-4df1-b610-487ad6f08df3, the session branch
  above, which survives exactly as long as the worktree recorded above.
  The review ref the invoker passes to /next-plan-review has two cases. When
  the `Observed in an earlier session` line is present, it is that line's
  landed commit hash, with no lookup. When that line is absent, the recording
  session is the observing session, and worktree reuse moves its branch past
  its landing, so the invoker never reviews the ref itself: it resolves the ref
  to the commit that added this Plan with
  `git log --follow --diff-filter=A -n 1 --format=%H <ref> -- <this plan path>`
  and passes that commit. When the recorded ref is gone or that command prints
  nothing (a later session moved this Plan), it runs the same command with
  `<ref>` omitted. A periodic Plan-history squash can make either command
  return an unrelated aggregate commit, and a landing that also deletes a
  similar file can make it return another session's commit, so review the
  result only when the commit is attributable to the observing session recorded
  above alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode, with the review ref
the provenance block above defines, supplying the recorded client and, on
Claude, the recorded conversation session ID; a Codex review supplies the
client and that review ref only. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

The author recommends, as a starting point for that investigation, a narrow
brief-driven exemption parallel to the existing interleaved-edit exemption: a
dispatch whose brief states that a later acceptance build depends on a
one-time Local export builds only its requested targets. Whether the exemption
belongs in the skill's inputs, the worker's step 3, or the delegator's brief
guidance is for the fix session to decide from the root cause.

## Critical files
- `.agents/skills/compile/references/worker.md` — steps 2 and 3
  (`recommendedTargets` rows and the build-every-row rule)
- `.agents/skills/compile/SKILL.md` — `## Inputs` (the requested-targets
  minimum and its interleaved-edit exemption)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files named above: worker.md
  steps 2-3 and SKILL.md `## Inputs`

## Out of scope
- The landed change the session produced
- `.agents/skills/compile/references/runtime-data-mode.md` mode selection and
  DataPacker dirty detection or shared-cache behavior
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (trigger: scoped tool behavior — which targets one `/compile`
builder dispatch builds); escalate if the fix reaches build/bootstrap
coordination. The fix keeps every changed-path target built somewhere before
landing. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  a compile-check dispatch followed by a Local generation acceptance build on
  the same edited data inputs lets the acceptance build's DataPacker log show
  the expected `is dirty` lines
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
