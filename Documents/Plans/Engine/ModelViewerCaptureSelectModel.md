<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-25T00:38:17.618Z","dependsOn":[]} -->
# Fix: Invoke-ModelViewerCapture.ps1 — cannot capture a chosen catalog model

## Context

A Plan acceptance criterion asked for one ModelViewer screenshot per converted
test scene, taken through `/agent-harness`. Its recorded setup was a build,
then selecting each scene in the ModelViewer catalog. The runtime worker ran
the documented call:

`pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-ModelViewerCapture.ps1 -RepositoryRoot '<worktree>' -AppDataDirectory '<worktree>\Temp\ModelViewerCapture-20261025T002800Z' -RequireStable`

The call exited 2 with `capture.executable-missing`, because only the Release
`ModelViewer.exe` had been built. That part is a setup limitation:
`/compile` ModelViewer Debug clears it, so this Plan does not cover it.

The worker also found that a Debug build would not have settled the 12
per-scene criteria. The script has no input that picks the model to capture:
- Its parameters are only `-RepositoryRoot`, `-AppDataDirectory` and
  `-RequireStable` (`.agents/skills/agent-harness/scripts/Invoke-ModelViewerCapture.ps1:6-12`).
- It starts the viewer with only `--agent-port`, `--log-file` and
  `--app-data-directory` (`:191-195`), then posts F9.
- The viewer starts on catalog entry 0: `miSelected = 0`
  (`ModelViewer/Source/ModelViewerApplication.h:40`), and the catalog is sorted
  by packed path (`ModelViewer/Source/ModelViewerApplication.cpp:48-51`).
- The selection changes only through an ImGui `Selectable` click
  (`ModelViewerApplication.cpp:529-532`).

So every capture shows the first catalog model, and all 12 per-scene criteria
stayed `BLOCKED`. The worker identified the narrowest extension: a viewer start
argument or a capture-script parameter that names the catalog model to show.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: bc223e8a-e1b2-4eb2-8c8b-c670e8115568
- Worktree/branch UUID: 5ad8d6ab-571a-46db-b9c8-451dd430a8e5
- Session branch: claude/5ad8d6ab-571a-46db-b9c8-451dd430a8e5
- Worktree: .claude\worktrees\BrokenEngine\5ad8d6ab-571a-46db-b9c8-451dd430a8e5
- Landing ref: claude/5ad8d6ab-571a-46db-b9c8-451dd430a8e5, the session branch
  above, whose tip is that session's final commit and which survives exactly as
  long as the worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/Engine/ModelViewerCaptureSelectModel.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design

First, root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/5ad8d6ab-571a-46db-b9c8-451dd430a8e5` in bounded
friction mode, supplying client `claude` and the conversation session ID above.
Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author recommends a viewer start argument that names a catalog entry by
its packed path. With it, the viewer selects that entry before the first F9.
Add a matching optional capture-script parameter that passes the argument
through. This is recommended over sending pointer clicks to the catalog: those
would depend on the panel layout of a hidden window. A packed path stays
stable when other scenes are added.

When the named model is not in the catalog, the run should fail and report it
rather than capture entry 0.

## Critical files

- `.agents/skills/agent-harness/scripts/Invoke-ModelViewerCapture.ps1`
- `.agents/skills/agent-harness/references/model-viewer.md`
- `Engine/Source/LaunchOptions.h`
- `Engine/Source/LaunchOptions.cpp`
- `ModelViewer/Source/ModelViewerApplication.cpp`
- `ModelViewer/Source/ModelViewerApplication.h`
- `ModelViewer/AGENTS.md`

## In scope

- Root-cause investigation as `## Design` states.
- The smallest resulting fix, confined to:
  - the files named above: the capture script's `param` block and its
    `$viewerArguments` list;
  - `model-viewer.md` `## Capture`;
  - `ParseLaunchOptions` and `LaunchOptions` for the new argument;
  - the catalog construction and initial `miSelected` in
    `ModelViewerApplication`;
  - the `--agent-port` paragraph of `ModelViewer/AGENTS.md`
    `## Application contracts`.

## Out of scope

- The landed change the session produced.
- Building ModelViewer Debug (the `capture.executable-missing` half; the route
  is `/compile`).
- Animation clip, lighting preset, camera or sun selection from the command
  line.
- Unrelated skills or scripts, and any transcript path or transcript text in
  the repository.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior: the ModelViewer and its capture
script). Escalate if the fix reaches build/bootstrap coordination. Never embed
transcript paths or home paths.

The capture must keep its non-activating hidden launch (`windowVisible`
`false`) and its exact-PID close. The viewer must still never activate or raise
its window, steal focus, or move the physical pointer (`ModelViewer/AGENTS.md`
`## Application contracts`).

## Acceptance criteria

- The recorded symptom no longer reproduces under the documented invocation.
  After a `/compile` ModelViewer Debug build, two runs of
  `Invoke-ModelViewerCapture.ps1` each name a different packed scene that is
  not catalog entry 0. Each returns code `ok`, `windowVisible` `false`, and a
  last capture whose SHA-256 differs from the other run's.
- A run that names a path missing from the catalog returns a non-`ok` result
  and does not capture entry 0.
- The static-checks runner reports every row the change triggers passing.
  Invoke it as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents.
