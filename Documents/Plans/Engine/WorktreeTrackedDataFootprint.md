<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-03T23:50:08.577Z","dependsOn":[]} -->
# Investigate and decide: stop copying the tracked baked data into every session worktree

## Context
Every session worktree carries its own physical copy of the Git-tracked baked
data under `Engine/Data/Islands` and `Engine/Data/Textures`. Nothing under
`Engine/Data` is ignored, so `git worktree add` writes all of it into each new
worktree.

Measured in the authoring worktree:
- `Engine/Data/Islands` 687 MB, `Engine/Data/Textures` 302 MB (`du -sm`), 989 MB
  together.
- Those two trees are 437 of the repository's 1813 tracked files
  (`git ls-files Engine/Data/Islands` 284, `git ls-files Engine/Data/Textures`
  153, `git ls-files` 1813), so they are a quarter of the tracked files and the
  large majority of the checkout's bytes.
- A never-built worktree is about 1.3 GB on disk; one with a build `Output`
  folder is about 3.5 GB (manager-supplied measurement).
- `C:\Users\dougt\.claude\worktrees\BrokenEngine` held 110 worktrees on
  2026-09-03, 109 of them created between 2026-09-01 and 2026-09-03 — roughly
  145 GB of duplicated checkouts (manager-supplied measurement). Linked
  worktrees share Git history but each one is a full working-file checkout.

Most sessions never read these bytes. `.agents/skills/compile/references/runtime-data-mode.md`
already splits worktree builds into Shared and Local: Shared is the default and
consumes the primary checkout's generated `Output\Data`, running no DataPacker
step at all, so it never opens a tracked source asset. Local is mandatory only
when changed paths touch `DataPacker/**`, `Engine/Data/**`,
`Projects/BrokenEngineSandbox/Data/**`, `Common/DataFile.h`, or the generated
header, exporter, compression, chunk-layout, or pack/manifest contracts. The
runtime itself reads only `.pack` chunks. So a Shared-mode session — the
overwhelming majority — pays 989 MB of copy and disk for files it never touches.

Existing mechanism the investigation must weigh before inventing anything:
- The Local trigger's path rules are already computed from the changed-path set:
  `.agents/skills/compile/scripts/Resolve-CompileContext.ps1:34` holds the
  `Engine/Data/**` trigger, and `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1:263-271`
  consumes the resulting `triggerMatches`.
- `DataPacker --materialize-data` (`DataPacker/Source/Main.cpp:728-746`) applies
  to the generated *output* Data root only; the two tracked source roots are
  passed to it as inputs and are not themselves materialized.
- `FileManager::EnsureLocal`/`MaterializeOutput`
  (`DataPacker/Source/FileManager.cpp:495-588`) is the working precedent for the
  mechanism this Plan is about: for an absent destination it first tries
  `CreateSymbolicLinkW` with `SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE` to
  the primary directory, and on `ERROR_PRIVILEGE_NOT_HELD`,
  `ERROR_INVALID_PARAMETER`, or `ERROR_NOT_SUPPORTED` falls back to copy-on-write
  materialization through a staging directory and an atomic publish.
- `.agents/scripts/Build-WorktreeDataPacker.ps1` seeds the worktree DataPacker
  executable from the primary prebuild, so a "take it from primary at worktree
  creation" step already exists in the bootstrap.

Worktree creation happens in exactly one place. `.claude/claude-worktree.sh` (38
lines) and `.codex/codex-worktree.ps1` (25 lines) are thin argument shims that
both exec `.agents/scripts/Start-AgentWorktreeSession.ps1`, whose
`git -C $root worktree add -b $branch $worktree $primary.Head` at line 120 is the
only creation call; the same script also performs the existing post-creation
seeding steps (shader caches, ThirdParty provisioning, DataPacker build) at lines
131-176.

No live Plan owns this: a search of `Documents/Plans` for `sparse`, `worktree`,
and `materialize` returned only unrelated matches (the sparse simulation grid,
per-Plan session provenance blocks, and DataPacker output materialization
Plans).

## Design
This work starts as an investigation, not as a decided implementation. The user
has asked explicitly that the mechanism be investigated and that the options be
presented rather than chosen in advance, so the implementing session must reach a
user decision at Change Workflow Step 2 (Tier 3: `/plan-audit`,
`/plan-simplicity-review`, then `/external-grill-plan` rounds) before writing any
bootstrap change. Nothing below is binding; the two options are staged with their
known trade-offs, and neither is recommended here.

### Option 1 — Git sparse checkout applied at worktree creation
Have `Start-AgentWorktreeSession.ps1` configure sparse checkout on the new
worktree so `Engine/Data/Islands` and `Engine/Data/Textures` are never written,
with an opt-in step that materializes them when the session turns out to need
Local data.

Known strengths: Git owns the state, so there is no second source of truth about
which files exist; excluded files carry the `SKIP_WORKTREE` bit, which Git itself
keeps out of `git status`; re-including a path is one documented Git command; it
costs no new C++.

Known costs and unknowns the investigation must settle: cone mode expresses
inclusion by directory, so excluding two subdirectories of an otherwise included
`Engine/Data` requires enumerating the siblings (`Audio`, `Models`, `Raw`,
`Shaders`) and re-enumerating them whenever a sibling is added — determine
whether that enumeration can be generated rather than hand-maintained, and what
happens when it drifts. Non-cone patterns avoid the enumeration but are pattern
matched, and `Engine/Data/Textures` filenames contain `[` and `]`
(`[BC4]Radial.png`, `[C]Skybox/`), which are character-class metacharacters in
those patterns. Also settle the ordering against `git worktree add`, which writes
the full tree before any post-creation step can run, so a naive
create-then-sparsify pays the copy anyway.

### Option 2 — Mirror the needs-local-data detection and supply the trees from primary
Keep the two trees out of the worktree and have DataPacker, or a script, detect
when a session needs the tracked source data and link or copy it from the primary
checkout — the same shape as `FileManager::EnsureLocal` above.

Known strengths: it reuses a mechanism that already exists and already handles
the unprivileged-symlink and copy-on-write fallbacks; the detection it mirrors
(the Local-mode trigger) is the exact condition under which the data is needed.

Known costs and unknowns: the user's explicit question is whether the detection
can be made fully deterministic inside DataPacker, and if not, whether a script
can do it — DataPacker today receives the source roots as arguments and never
inspects Git state, while the Local/Shared decision is made in PowerShell from
the changed-path set, so answer where the detection can honestly live before
choosing. A directory symlink pointing at the primary checkout also means any
write inside it lands in primary, which `runtime-data-mode.md` forbids outright
("Never copy worktree DataPacker/data source changes into the primary checkout to
test them") — establish whether a link is safe at all here, or whether only a
copy is.

### Open questions this Plan must answer before implementation
1. Local-mode interaction. A session whose changes touch `Engine/Data/**` must
   still be able to build Local. Determine the ordering: the Local trigger is
   derived from changed paths, but a path that is absent from the worktree cannot
   be changed until it is restored, so state exactly how a session that intends
   to edit island or texture assets gets them back, and whether that step is
   automatic or an explicit agent action.
2. Git behavior with absent paths. Prove, from a real experiment rather than from
   reasoning, how `git status`, `git diff`, `git add`, rebase, the session squash,
   and the `/finalize-changes` landing flow behave in a worktree where those paths
   are absent — including the case where primary changes one of them while a
   sparse session is open, and the case where the session itself must land a
   change to one of them.
3. Wrapper coverage. The evidence above says both wrappers delegate to one shared
   script, so one change should cover both; confirm there is no other worktree
   creation or reattach path (including `-ReattachWorktree`, where an existing
   full worktree may need converting) before relying on that.
4. Failure and fallback. Decide what happens when the mechanism is unavailable —
   no symlink privilege, an older Git, a partially materialized tree — and
   whether the failure is a hard stop or a documented fall back to today's full
   checkout.
5. Existing worktrees. Decide whether the change applies only to newly created
   worktrees or also reclaims space in the ~110 that already exist, and say so
   rather than leaving it implied.
6. Whether the two named directories are the right boundary, or whether the rule
   should be derived (for example, every `Engine/Data` subtree that is a
   DataPacker source input and not a build input) so it does not silently go stale
   when a new asset tree is added.

The implementing session should answer these from experiments in a scratch
worktree and from the current tree, record the answers, then take the decided
option to the user through Step 2 before editing the bootstrap.

## Critical files
- `.agents/scripts/Start-AgentWorktreeSession.ps1` (worktree creation at line
  120; existing post-creation seeding at lines 131-176)
- `.claude/claude-worktree.sh`, `.codex/codex-worktree.ps1` (wrapper shims;
  likely unchanged, to be confirmed by open question 3)
- `.agents/skills/compile/references/runtime-data-mode.md` (Shared/Local contract
  the change must stay consistent with)
- `.agents/skills/compile/scripts/Resolve-CompileContext.ps1`,
  `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` (Local trigger
  derivation and consumption)
- `DataPacker/Source/FileManager.cpp`, `DataPacker/Source/FileManager.h`,
  `DataPacker/Source/Main.cpp` (materialization precedent; only touched if
  Option 2 is chosen)
- `Engine/Data/Islands`, `Engine/Data/Textures` (the trees whose presence
  changes; their contents are never edited by this work)

## In scope
- The investigation and the recorded answers to the six open questions above
- Presenting Option 1 and Option 2 to the user at Step 2 and implementing only
  the option the user selects
- For the selected option only: the worktree-creation and post-creation region of
  `.agents/scripts/Start-AgentWorktreeSession.ps1`; the corresponding
  restore/materialize entry point (a new step in that script, a new
  `.agents/scripts/` script, or a DataPacker mode, as decided); and the
  documentation of the resulting rule in
  `.agents/skills/compile/references/runtime-data-mode.md` and
  `Documents/FreshMachineSetup.md`

## Out of scope
- Changing the contents of `Engine/Data/Islands` or `Engine/Data/Textures`, or
  removing either tree from Git tracking
- Changing the Shared/Local decision rules themselves — the trigger paths, the
  deletion-only exception, or the agent-judgment triggers
- The generated `Output\Data` materialization path that
  `--materialize-data` already owns, and the Attribution root
- The build driver's property derivation (`DataBuildMode`, `RunDataPacker`,
  `GameDataDirectory`, `GeneratedDataIncludeRoot`)
- `/cleanup-worktrees`, the landing lock, the plan scheduler, and claim handling
- Build `Output` folder size, which is a separate cost from the tracked-data copy
  this Plan addresses

## Risk tier and invariants
Expected Tier 3. Trigger: build/bootstrap coordination that can block other
sessions — every session's worktree is created by the script this work changes, a
regression there breaks session startup for every agent, and the change also
spans the wrapper bootstrap and the DataPacker/compile data-mode contract. This
is the author's classification, to be confirmed at Step 1.

Invariants to preserve:
- A session that must build Local can always obtain the real tracked source
  assets, and Local never silently falls back to Shared data.
- No worktree edit ever writes into the primary checkout.
- `git status`, the session squash, and landing continue to see the session's
  real changes, with no path silently dropped from a landed diff because it was
  absent from the worktree.
- Reattach through the documented wrapper inputs keeps working, and a worktree
  with a rebase in progress still starts.

## Acceptance criteria
- The six open questions each have a recorded, evidence-backed answer, and the
  user has selected one option before any bootstrap file is edited
- A newly created worktree that needs no Local data does not contain
  `Engine/Data/Islands` or `Engine/Data/Textures` bytes, and its measured
  on-disk size drops by approximately 989 MB
- In that worktree, `git status` reports no modification or deletion for either
  tree
- A session that changes a file under `Engine/Data/**` can restore the trees by
  the documented step and complete an authorized Local generation build through
  `/compile`
- Creating a worktree and reattaching to one both still succeed through
  `.claude/claude-worktree.sh` and `.codex/codex-worktree.ps1`
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` and `code: ok`
