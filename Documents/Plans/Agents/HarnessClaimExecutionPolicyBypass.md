<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T15:14:33.054Z","dependsOn":[]} -->
# Fix: agent-harness claim script passes `-ExecutionPolicy Bypass` to its provisioning child process

## Context

The root `AGENTS.md` bundled-scripts directive defines the canonical way to run
a repository PowerShell script and states plainly: "Never pass
`-ExecutionPolicy Bypass`."

`.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1:136-139` launches
the repository provisioning script as a child process and passes that exact
flag:

```powershell
$provision = Invoke-NativeCapture $powerShell @(
	'-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $provisioner, '-RepositoryRoot', $root)
```

`$provisioner` is the repository-owned `.agents/scripts/Provision-WorktreeThirdParty.ps1`.
This is the only occurrence of the flag in any `.agents/skills/agent-harness`
file. It is a script-to-script launch rather than an agent shell invocation, so
the directive's wording does not name it literally, but it is a repository
script run in the sense the directive governs and it contradicts the rule's
intent: the repository declares that repository scripts run under normal machine
policy, and this one launch silently opts out of that.

Pre-existing: the line arrived with the file's initial commit
`e571f6f11f62e5cb7f9f57c2705a0a38e3391d40` (2026-08-10), well before the session
that observed it, and outside that session's claimed Plan boundary
(`Documents/Plans/Agents/AgentHarnessClaimWaitPath.md`, which owned the claim
wait path only). The observation is therefore a pre-existing out-of-scope
residual, not an acceptance failure of that change.

Why it matters beyond tidiness: `Bypass` is the weakest PowerShell execution
policy setting, so this launch runs whatever is at the provisioner path without
the machine's configured script-trust checks. Every other repository script,
including this one itself, already runs under normal policy, which makes the
exception both unnecessary-looking and unexplained.

## Design

The decision is already made; investigation only confirms or refutes one narrow
condition.

Default branch (expected): delete `'-ExecutionPolicy', 'Bypass',` from the child
argument array, leaving `'-NoProfile', '-File', $provisioner, '-RepositoryRoot',
$root`. The provisioning script then runs under the same machine execution
policy that already has to permit repository scripts, since the fresh-machine
bootstrap in `Documents/FreshMachineSetup.md` requires PowerShell 7 and runs
repository scripts directly without setting any per-invocation policy override.

Exception branch (only if investigation proves it): if there is a concrete,
currently supported configuration in which the harness must run under a machine
policy that blocks repository scripts — so removing the flag breaks provisioning
for a real supported setup — then keep the flag exactly as it is and instead add
one sentence at the root `AGENTS.md` bundled-scripts directive recording this
single internal child launch as an explicit allowed exception, naming the script
and the reason. Proof means an identified supported configuration, not a
hypothetical locked-down machine.

Do not restructure the launch, change `Invoke-NativeCapture`, change how the
provisioner is located, or touch the provisioner itself in either branch.

## Critical files

- `.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1` — the child
  launch argument array at `:136-139`; the sole change site in the default branch
- `AGENTS.md` — the bundled-scripts directive under `## Directives`; touched only
  in the exception branch, and then by one added sentence
- `.agents/scripts/Provision-WorktreeThirdParty.ps1` — read-only context: the
  launched script whose behavior must be unchanged
- `Documents/FreshMachineSetup.md` — read-only context: the bootstrap
  assumptions cited by the default branch

## In scope

- Removing `'-ExecutionPolicy', 'Bypass',` from the provisioner child-process
  argument array in `Invoke-HarnessClaim.ps1`, plus any sibling launch in that
  same script passing the same flag if one exists at implementation time
- In the exception branch only, and instead of that removal: one added sentence
  at the root `AGENTS.md` bundled-scripts directive documenting this one launch
  as an allowed exception

## Out of scope

- Every other script, project file, and wrapper in the repository that passes the
  flag, including `.claude/claude-worktree.sh`, the `gaea2-shared` scripts,
  `.agents/scripts/Test-CollectionLayoutFixtures.ps1`, `.agents/scripts/Detect-Python.ps1`,
  `README.md`, and the `.vcxproj` prebuild commands
- `Provision-WorktreeThirdParty.ps1`'s own behavior, contract, and output
- Any change to how `Invoke-HarnessClaim.ps1` locates PowerShell, captures child
  output, reports exit codes, or waits for a foreign lock owner
- Changes to `Documents/FreshMachineSetup.md` or any other machine setup
  documentation beyond citing it
- The claim wait behavior owned by `Documents/Plans/Agents/AgentHarnessClaimWaitPath.md`

## Risk tier and invariants

Expected Tier 1-2: scoped tool behavior in one bundled script, most likely a
single-line edit with no public signature or invariant exposure; Tier 1 if the
exception branch makes it a documentation-only change. Escalate only if
investigation shows the launch participates in shared build/bootstrap
coordination in a way the removal would change. The claim script's typed result
shape, its provisioning exit-code reporting, and its stdout discipline must stay
exactly as they are: the provisioner still runs as a child process whose exit
code and captured output drive `claim.provision-failed`.

## Acceptance criteria

- A repository-wide search for `-ExecutionPolicy` under
  `.agents/skills/agent-harness/` returns no match, or, in the exception branch,
  the flag remains and the root `AGENTS.md` bundled-scripts directive names this
  launch as an allowed exception
- A live `/agent-harness` run of the claim path provisions and claims
  successfully on this machine, with the provisioning child process exiting `0`
- WorktreeCli `plan validate` exits `0` with `status: valid` and `code: ok`

## Notes

This Plan is keyed to (`Invoke-HarnessClaim.ps1` provisioner child launch,
`-ExecutionPolicy Bypass`). `Documents/Plans/Agents/AgentHarnessOwnerTokenReclaimLoop.md`
touches the same skill but owns owner-token bookkeeping across repeated
claim/release iterations, and is not a duplicate. No dependency edge exists in
either direction: the two changes touch disjoint regions.
