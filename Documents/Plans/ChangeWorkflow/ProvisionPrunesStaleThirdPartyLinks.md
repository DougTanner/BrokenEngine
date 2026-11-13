<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-12T23:49:27.299Z","dependsOn":[]} -->
# Fix: ThirdParty provisioning stops on a stale link-farm entry after a submodule pin bump

## Context

In a session worktree each primary submodule under `ThirdParty/` is a link
farm: one symbolic link per top-level entry of the primary checkout's submodule
tree. `.agents/scripts/Provision-WorktreeThirdParty.ps1` rebuilds the expected
entry list from the primary source tree on every run (`Get-LinkFarmEntries`,
called at `:253`), then `Assert-LinkFarmDestination` (`:88-111`) walks the
worktree's existing link farm and throws on:

- any entry whose name is not in the expected list (`:96-98`,
  `Submodule provisioning root contains unexpected entry '<path>'.`);
- any entry whose kind changed between file and directory (`:101-103`,
  `Submodule provisioning entry must be a <kind> link: '<path>'.`).

`Ensure-LinkFarm` (`:113-124`) only adds missing entries; nothing removes a
link the primary tree no longer has. So when a primary submodule pin bump
removes or renames a submodule's top-level entry (or turns a file into a
directory), a worktree that already holds the old link farm keeps the stale
link, and its next provisioning run stops. A fresh worktree is unaffected: its
link farm starts empty.

Root cause, confirmed by code inspection and not yet reproduced live: the link
farm has no pruning path for entries the current pin no longer has, and the
destination assertion treats every such entry as foreign.

Reachable today on these paths, each running provisioning against an existing
worktree after the pins changed:

- The documented remedy for a pin mismatch — rebase the worktree onto the
  primary tip (the pin-mismatch message at `:231`) — followed by `/compile`,
  whose build entry provisions first
  (`.agents/skills/compile/scripts/Invoke-CompileBuild.ps1:277-281`, typed stop
  `provisioning.failed`), or by an `/agent-harness` claim
  (`.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1:259`).
- `/reset-worktree`, which fast-forwards a reused worktree and reruns session
  setup through `.agents/scripts/Initialize-AgentWorktreeSession.ps1`, which
  calls this script (`Initialize-AgentWorktreeSession.ps1:111`).

Pin bumps happen on primary, and the saved feature
`Documents/Features/Engine/UpdateThirdPartySubmodules.md` (`:51-57`) plans one
that moves tinygltf's top-level header into `attic/`.

## Design

1. Reproduce before editing: in a scratch session worktree, add a symbolic
   link under one `ThirdParty/<submodule>/` link farm whose name the primary
   submodule tree lacks and whose target is that name joined to the primary
   submodule path (the shape a removed entry leaves), then run provisioning
   (`/compile` of any target is enough). Confirm the `unexpected entry` stop.
   If it does not reproduce, or a different cause shows, stop and confirm the
   root cause through `/external-diagnose-bug` before changing anything.
2. Fix inside `## In scope`. Author's recommendation, with rationale: treat an
   existing link-farm entry as stale, and replace it, only when it is a
   symbolic link whose target is exactly the primary submodule source path
   joined with that entry's own name — the only shape `Ensure-LinkFarm`
   creates — and the expected list no longer has that name, or has it with the
   other kind. Remove a stale entry before `Ensure-LinkFarm` adds the current
   entries, and record each removal in `$createdArtifacts` so the existing
   rollback (`Undo-ProvisioningArtifacts`) can restore it. Every other
   unexpected entry (an ordinary file or directory, or a link to any other
   target) keeps the current stop, because it may hold a user's own change.
   Removal is per exact link path; no wildcard or enumerated deletion beyond
   the one entry being tested.

If the fix needs a change outside `## In scope`, surface it for re-planning
instead of widening scope.

## Critical files

- `.agents/scripts/Provision-WorktreeThirdParty.ps1`

## In scope

- Reproduction and root-cause confirmation as `## Design` step 1 states.
- `.agents/scripts/Provision-WorktreeThirdParty.ps1`: `Assert-LinkFarmDestination`,
  `Ensure-LinkFarm`, `Undo-ProvisioningArtifacts`, and the link-farm loop at
  `:251-259` and `:268-280`, only as far as stale-entry detection, removal, and
  its rollback require.

## Out of scope

- The pin-mismatch check and its message (`:223-236`), the pin stamp
  (`:238-249`, `:281-288`), and the shared Output directory links
  (`Assert-DirectoryLinkDestination`, `Ensure-DirectoryLink`).
- Removing a whole submodule from `.gitmodules`; nested-metadata and reparse
  point checks on the primary source tree.
- Callers: `Initialize-AgentWorktreeSession.ps1`, `Invoke-CompileBuild.ps1`,
  `Invoke-HarnessClaim.ps1`, `/reset-worktree`, and their documents.
- Any ThirdParty submodule content or pin.

## Risk tier and invariants

Tier 2 — trigger: scoped tool behavior, build/bootstrap coordination confined
to one worktree's ThirdParty link farm (`.agents/references/risk-tiers.md`,
Tier 2). The script holds only a transient per-worktree WorktreeCli session
claim (`:175-178`) and blocks no other session; escalate to Tier 3 if the fix
reaches the shared primary outputs or a PC-global lock. Invariants: the primary
checkout is never touched (`:254`, `:261`); provisioning never deletes anything
it did not itself create in that shape; a failed run rolls back what it
changed; a fresh worktree's output is unchanged.

## Acceptance criteria

- The `## Design` step 1 reproduction, rerun after the fix, provisions
  successfully, removes the stale link, and leaves every current entry linked
  to its primary source.
- A link-farm entry that is an ordinary file or directory, or a link to any
  other target, still stops provisioning with the current message.
- Provisioning of a freshly created worktree prints
  `Shared worktree dependencies validated` as before.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing.
