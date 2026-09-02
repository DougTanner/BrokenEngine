<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-02T21:39:59.795Z","dependsOn":[]} -->
# Fix: implement-plan — the `Build required` handoff field has no form for Release-only targets

## Context
The implementer handoff contract in
`.agents/skills/implement-plan/SKILL.md:57-58` defines the field as
`Build required: <target, configuration/platform, selected project-member .cpp;
for headers, consuming targets and configuration/platform; or none>`. That
wording asks for a configuration/platform for every changed `.cpp` and states no
exception for the targets that build in one configuration only.

`.agents/skills/compile/scripts/Invoke-CompileBuild.ps1:48` declares
`$script:ReleaseOnlyTargets = @('DataPacker', 'WorktreeCli', 'AgentHarness')`,
and `:189-192` blocks any other configuration for those targets before any child
process runs, stopping with the typed block
`parameter.configuration-invalid` and the message
"<Target> builds Release only; -Configuration <Configuration> is not permitted."

Observed in this session, on a DataPacker-only C++ change:
- The implementer emitted `Build required: DataPacker, Debug|x64 and
  Release|x64`, following the field's wording literally.
- Main copied that field into the builder brief, making "both builds succeed" an
  acceptance criterion.
- The builder spent one `/compile` invocation on the Debug configuration, which
  was rejected with `parameter.configuration-invalid` before building anything,
  and returned the unsatisfiable half of the acceptance criterion as a residual.
- Main then reconciled the acceptance criterion by hand, accepting the Release
  build alone as satisfying it.

The rework the friction forced: one wasted builder `/compile` call, one residual
round-trip, and a hand-written acceptance-criterion correction by main.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 6bef704a-a3c4-4dd0-a33d-5680aad61740
- Worktree/branch UUID: c16c7e19-3a39-4e47-b11e-66cf5b64c486
- Session branch: claude/c16c7e19-3a39-4e47-b11e-66cf5b64c486
- Worktree: .claude\worktrees\BrokenEngine\c16c7e19-3a39-4e47-b11e-66cf5b64c486
- Landing ref: claude/c16c7e19-3a39-4e47-b11e-66cf5b64c486
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/c16c7e19-3a39-4e47-b11e-66cf5b64c486` in bounded
friction mode, supplying client `claude` and conversation session ID
`6bef704a-a3c4-4dd0-a33d-5680aad61740`. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

As a starting hypothesis, not a decision: the author recommends stating in the
`Build required` field's own wording that a Release-only target carries
`Release|x64` alone, with the list of those targets referenced from its single
owning location rather than restated — `/compile` and
`Invoke-CompileBuild.ps1:48` already own it — so the same fact is not duplicated
into a skill body. The rationale is that the emitter is the only place the wrong
value is produced, one sentence removes the unsatisfiable acceptance line at its
source, and no script behavior changes. The reviewing session should confirm or
replace this with whatever the evidence supports, including finding that the
better owner of the correction is a different file among the ones named below.

## Critical files
- `.agents/skills/implement-plan/SKILL.md` — the `Build required` handoff field
  at `:57-58` that emitted the unsatisfiable value
- `.agents/skills/compile/SKILL.md` — the documented build route the value is
  consumed by, for comparison and for where the Release-only fact is owned
- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` — read-only reference
  for `$script:ReleaseOnlyTargets` (`:48`) and the rejection at `:189-192`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the handoff-field wording in
  `.agents/skills/implement-plan/SKILL.md` and, only if the evidence puts the
  correction there instead, the corresponding documented statement in
  `.agents/skills/compile/SKILL.md`

## Out of scope
- Any behavior change to `Invoke-CompileBuild.ps1`, including relaxing or
  renaming the `parameter.configuration-invalid` block or the Release-only
  target list
- The landed change the session produced, and the DataPacker C++ it touched
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (documentation wording in a skill body, with no script or
behavior surface). Escalate to Tier 2 if root-causing concludes the fix must
change tool behavior, which is outside this Plan's boundary and returns for
re-planning. Invariant preserved: the Release-only target list keeps exactly one
owning location and is referenced, never copied, per the progressive-disclosure
directive. Never embed transcript paths or home paths.

## Acceptance criteria
- A handoff emitted for a change to a Release-only target names `Release|x64`
  only, so the resulting builder acceptance criterion is satisfiable by one
  `/compile` invocation
- No file outside `## Critical files` changes, and
  `Invoke-CompileBuild.ps1` is unchanged
- /validate-skill passes for any changed SKILL.md;
  `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid`, `code: ok`
