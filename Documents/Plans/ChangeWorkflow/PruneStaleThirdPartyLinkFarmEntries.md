<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-16T01:42:51.894Z","dependsOn":[]} -->
# Fix: ThirdParty provisioning rejects a rebased worktree whose link farm still holds entries the new submodule pins removed

## Context
A linked worktree's `ThirdParty/<submodule>` directory is a link farm: one
symbolic link per top-level entry of the primary checkout's submodule tree,
created by `Ensure-LinkFarm` (`.agents/scripts/Provision-WorktreeThirdParty.ps1:113-124`)
from the entries `Get-LinkFarmEntries` (:62-86) lists. `Ensure-LinkFarm` only
adds missing links (:120 skips any existing name); nothing ever removes one.
Before applying, `Assert-LinkFarmDestination` (:88-111, called at :256) throws
`Submodule provisioning root contains unexpected entry` (:98) for any name the
current primary tree no longer has, and `Submodule provisioning entry must be a
<kind> link` (:101-104) for a name whose kind changed.

When the primary checkout moves a submodule pin, a worktree provisioned at the
old pin first fails the pin check (:230-231), whose message and
`Documents/FreshMachineSetup.md:42` (baseline line 31) give the remedy: rebase
the worktree onto the primary tip. Git does not manage the contents of these
uninitialized submodule directories, so the rebase leaves the old links in
place, and the next provisioning run — session start
(`.agents/scripts/Start-AgentWorktreeSession.ps1:174`), every `/compile`
(`.agents/skills/compile/scripts/Invoke-CompileBuild.ps1:347`), and the
Sandbox and DataPacker pre-build steps — throws at :98 for every top-level name
the new pin removed. The documented remedy therefore does not recover the
worktree, and nothing short of hand-deleting links in `ThirdParty/` does.

Originating gap: the ThirdParty submodule update executed from
`Documents/Features/Engine/UpdateThirdPartySubmodules.md` requires that
afterwards other worktrees need only the documented rebase remedy (that
document's line 17), and it puts provisioning tooling out of its scope.
Adversarial-review finding ADV001 (Critical) showed the remedy is
insufficient. Top-level names that update removed: DirectXTK (13 `.sln` and
`.vcxproj` files), lz4 (`.cirrus.yml`, `Makefile.inc`), openexr (`.bazelrc`,
`bazel`, `INSTALL.md`, `util`, `WORKSPACE.bazel`), tinygltf (14, including
`tiny_gltf.h`, `json.hpp`, `cmake`, `appveyor.yml`), tinyobjloader (4 CI files),
zlib (`nintendods`, `old`, `zconf.h.cmakein`). No top-level entry changed kind
in that update. Observed: worktree
`C:\Users\dougt\.claude\worktrees\BrokenEngine\01c32faa-90d0-4b1a-b00b-de7aa45c3fdd`,
pinned at tinygltf `81bd50c1`, still links `ThirdParty/tinygltf/appveyor.yml`
and `ThirdParty/tinygltf/cmake` to primary paths the new pin `0d9ce1a9` no
longer has.

## Design
Recommended: treat a link the provisioner itself created for a name the current
pin no longer supplies as stale, and remove it during the existing apply
phase, so the rebase remedy is complete with no manual step.

- Stale entry (author's recommendation): an item of the destination that is a
  reparse point whose canonical target (`Get-LinkTarget`, :11-17) equals
  `Get-AgentCanonicalPath (Join-Path <primary submodule source> <item name>)`,
  and whose name is either absent from the current entries or present with the
  other kind (file versus directory). Matching the exact target the
  provisioner would have created for that name confines removal to its own
  links. Rationale: anything else — an ordinary file or directory, or a link
  aimed elsewhere — keeps today's throw, so the guard against a populated or
  foreign destination is unchanged.
- `Assert-LinkFarmDestination` needs the submodule's primary source root to
  compute that target, because a removed name has no entry to read it from;
  pass `$record.Source` at :256. It stays mutation-free and returns the stale
  items, which travel in the `$linkFarmPlans` record (:257) to the apply phase.
- `Ensure-LinkFarm` removes each stale item before its existing creation loop,
  using the form `Undo-ProvisioningArtifacts` already uses (:131); the loop
  then recreates a kind-changed name as a correct link and records it for
  rollback as today.
- Rollback (author's recommendation): do not restore a removed stale link.
  Rationale: `Get-LinkFarmEntries` lists every non-`.git` entry of the primary
  submodule tree, including ignored files, so a name absent from the entries
  has no target and its link is dangling; a kind-changed link points at the
  wrong link kind. Restoring either recreates exactly the state the next run
  removes again.

No new script, parameter, or documentation is needed: the existing rebase
remedy text (`Provision-WorktreeThirdParty.ps1:231`,
`Documents/FreshMachineSetup.md:42`) becomes true as written.

## Critical files
- `.agents/scripts/Provision-WorktreeThirdParty.ps1`

## In scope
- `Assert-LinkFarmDestination` (:88-111): add the primary source-root
  parameter, classify stale entries by the rule above instead of throwing for
  them, and return them.
- The link-farm planning loop (:251-259): pass `$record.Source` and carry the
  stale items in each `$linkFarmPlans` record.
- `Ensure-LinkFarm` (:113-124): accept and remove the stale items before
  creating missing links.

## Out of scope
- Any change to the pin check (:223-236), the pin stamp (:238-249, 281-288),
  `Get-LinkFarmEntries`, the Output directory links (:262-273), or
  `Undo-ProvisioningArtifacts`.
- Pruning anything other than a link whose target is the provisioner's own
  target for that name under the same submodule.
- `.agents/scripts/Start-AgentWorktreeSession.ps1`, `/compile` scripts,
  `.vcxproj` pre-build steps, `Documents/FreshMachineSetup.md`, and the
  `Documents/Features/Engine/UpdateThirdPartySubmodules.md` procedure.
- Recovering other existing worktrees by hand; they recover on their next
  provisioning run after this lands.

## Risk tier and invariants
- Change Workflow Tier 3. Trigger: build/bootstrap coordination that can block
  other sessions — every session start, every `/compile`, and the Sandbox and
  DataPacker pre-build steps in every worktree run this script, so a defect
  blocks every session.
- The primary checkout is never mutated: the link-farm work stays inside the
  existing `-not $root.Equals($primaryRoot, ...)` guards (:254, :261).
- Validation still precedes mutation: no removal happens unless every
  submodule and Output destination passes its assertions.
- No determinism/CRC, serialization, wire, save, or replay exposure.

## Acceptance criteria
- In a linked worktree, a planted link
  `ThirdParty/<submodule>/<name>` aimed at
  `<primary>\ThirdParty\<submodule>\<name>`, where the primary tree has no
  `<name>`, is removed by the next provisioning run, which then succeeds
  (session start completes, or `/compile` reports no `provisioning.failed`).
- In the same setup, an ordinary file at that name, or a link aimed at any
  other target, still fails provisioning with the existing
  `unexpected entry` message.
- A worktree whose link farm already matches the current pins provisions with
  with no removal.

## Notes
- Verification goes through session start or `/compile`, which own running
  provisioning; the `/compile` worker reference forbids running the script by
  hand as a separate agent command.
- The observed worktree above is a natural reproduction once rebased, but it
  belongs to another session; a planted link in the fix session's own
  worktree reproduces the same failure.
