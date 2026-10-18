<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-18T15:25:05.648Z","dependsOn":["Documents/Plans/Engine/EngineGameHookSweep.md","Documents/Plans/Engine/FlightPlanePresentation.md","Documents/Plans/Engine/NavigationPerFlightPlane.md"]} -->
# Create the Azure server VM with /deploy-server and record its address

## Context

The `/deploy-server` skill (`.agents/skills/deploy-server/`) holds a create-only subscription-scope Bicep template (`assets/main.bicep`, `assets/server.bicep`), deployed with the BrokenEngineSandbox parameters file `Projects/BrokenEngineSandbox/DeployServer/main.bicepparam` (instance `brokenengine`), and the provisioning procedure, `SKILL.md` `## Steps` 1-10, for the dedicated server's Azure VM, its daily schedule workflow, and its $200 budget stop.
The skill landed without its live steps being run, so nothing exists in Azure. On 2026-10-18 the user declined the spending confirmation at `SKILL.md` step 6. The user directed that nothing be created in Azure, and the server not be started, until the Engine Plans in this Plan's `dependsOn` are done. This Plan is that deferred live run.

Gap this Plan closes:

- `Documents/Plans/Engine/ConnectClientToHostedServer.md` `## Server address` reads `Recorded address: none`, so that Plan stops BLOCKED when claimed, and `Documents/Plans/ChangeWorkflow/DeployAndRunHostedServer.md` waits behind it.
- `.agents/skills/deploy-server/references/azure-facts.md` `## Provisioned OS disk` reads `Size and tier: none`.

State observed on 2026-10-18. The skill's steps re-run each check, so none of them is skipped:

- `az bicep build` of `assets/main.bicep` passed.
- Resource providers `Microsoft.Compute`, `Microsoft.Network`, `Microsoft.Logic`, and `Microsoft.Insights` are registered on the subscription.
- The read-only pre-checks of `SKILL.md` step 5 passed:
  - the image `MicrosoftWindowsServer:WindowsServer:2025-datacenter-azure-edition:latest` is `V2` with an NVMe `DiskControllerTypes` feature;
  - `Standard_D2s_v6` in `eastus` lists `V2` and has no `TrustedLaunchDisabled` capability;
  - Dsv6-family and Total Regional vCPU usage were 0 of 10.
- `az deployment sub what-if` listed 13 `Create` changes and no diagnostics.

User decisions for this run (this Plan's record of them, per `Documents/Plans/AGENTS.md` `## Authorship`):

- Subscription: the pay-as-you-go subscription named "Azure subscription", already confirmed as pay-as-you-go. The user signs in as the account owner.
- No Azure resource is created before the user's own explicit spending yes at `SKILL.md` step 6 in this run. A yes given in an earlier session, or this Plan's text, is not that yes.
- Inbound access: UDP 27015 from any source. RDP (TCP 3389) and SSH (TCP 22) only from the user's current public IPv4. The user's IP is dynamic, so step 3 asks for it at run time and never reuses an earlier address.
- Schedule: the VM runs 08:00-23:00 US Eastern every day, following daylight saving.
- Budget: $200 a month. At that spend the VM is deallocated and stays off until the user runs the skill's documented turn-back-on step.

## Design

1. Run `/deploy-server` `## Steps` 1-10 as written, in the main session, because the steps interview the user and hold the spending confirmation. The skill is the procedure. If step 6 ends without the user's yes, nothing is created; the recommended route is to defer this Plan rather than complete it.
2. With `publicIp` and the OS disk size and tier from the step 10 report, record two values:
   - In `ConnectClientToHostedServer.md` `## Server address`, replace `none` on the `Recorded address:` line with `publicIp`.
   - In `azure-facts.md` `## Provisioned OS disk`, replace `none` on the `Size and tier:` line with the size and tier from step 8's `az disk show`.
3. Run the two end checks under `## Acceptance criteria`.

Risk tier: Tier 2. Trigger: one skill's tool behavior with external spending and network exposure in the user's Azure subscription.
The spending is gated by the step 6 yes and capped by the budget stop. The exposure is fixed by the template's NSG rules. No repository runtime surface changes: no C++, determinism/CRC, serialization, `.pack`, replay, wire, affinity, threading, allocation, shader, build, or trust change. Live verification is Azure CLI reads and an RDP port check, not `/agent-harness`. The repository change is two recorded lines.

## Critical files

- `.agents/skills/deploy-server/SKILL.md` (the procedure; read, not changed)
- `.agents/skills/deploy-server/references/azure-facts.md`
- `Documents/Plans/Engine/ConnectClientToHostedServer.md`

## In scope

- One create-only deployment of `.agents/skills/deploy-server/assets/main.bicep` through `SKILL.md` `## Steps` 1-10, after the user's explicit yes at step 6. This includes step 5's `az provider register` only after its own yes, step 7's failure handling, and step 9's budget-stop run and turn-back-on.
- The `Recorded address:` line in `ConnectClientToHostedServer.md` `## Server address`.
- The `Size and tier:` line in `azure-facts.md` `## Provisioned OS disk`.

## Out of scope

- Any edit to `.agents/skills/deploy-server/SKILL.md`, `assets/`, or another `azure-facts.md` section. Any change to the template's resources, names, schedule times, budget amount, or inbound rules. A template or skill defect found during the run stops the run and is reported to the user.
- Redeploying onto an existing `brokenengine-server-rg`, repair, resize, or teardown, apart from the skill's `az group delete` after the user's explicit yes.
- VM-side configuration, deploying the game server, and starting it: `DeployAndRunHostedServer.md` owns those.
- Any C++ change. Any other line of `ConnectClientToHostedServer.md`, and any other Plan, apart from the scheduler's completion edits.
- Recording the public IP anywhere other than the `Recorded address:` line.

## Acceptance criteria

Live Azure checks, each after the user's step 6 yes; expected settings are those of `assets/server.bicep`:

- `az deployment sub create` reports `provisioningState` `Succeeded` and a nonempty `outputs.publicIp.value`, and the password file no longer exists.
- `az network nsg rule list` lists exactly `Game-UDP-27015` (priority 100, `Udp`, source `*`, port 27015), `Admin-RDP` (110, `Tcp`, the user's /32, port 3389), and `Admin-SSH` (120, `Tcp`, the user's /32, port 22).
- `az vm show -d` reports `Standard_D2s_v6`, image SKU `2025-datacenter-azure-edition`, a `StandardSSD_LRS` OS disk, `securityProfile.securityType` `TrustedLaunch`, `storageProfile.diskControllerType` `NVMe`, and `osProfile.computerName` `gameserver`. `az disk show` on `brokenengine-server-osdisk` gives its size and tier.
- `az network public-ip show` reports `sku.name` `Standard` and `publicIPAllocationMethod` `Static`.
- The schedule workflow reports state `Enabled`, `identity.type` `SystemAssigned`, and a `Recurrence` trigger: frequency `Day`, interval 1, `timeZone` `Eastern Standard Time`, hours [8, 23], minutes [0], `startTime` `2026-01-01T00:00:00`.
- The action group is enabled with one `logicAppReceivers` entry whose `resourceId` is the budget-stop workflow.
- The budget is amount 200, `Monthly`, with exactly one enabled notification: `Actual`, `GreaterThanOrEqualTo`, threshold 100, `contactEmails` [the signed-in `user.name`], `contactGroups` [the action group's ID].
- `az role assignment list -g brokenengine-server-rg` lists exactly three resource-group-scope assignments: the schedule workflow's identity with Desktop Virtualization Power On Off Contributor, and the budget-stop workflow's identity with that role and with Logic App Operator.
- While the VM runs, `Test-NetConnection <publicIp> -Port 3389` reports `TcpTestSucceeded : True`.
- Step 9: after the `budget` test notification, or the direct callback POST fallback, with the producing path recorded, a new budget-stop run finishes, the VM reaches `PowerState/deallocated` within 15 minutes, and the schedule workflow is `Disabled`. After the turn-back-on enable, the schedule workflow is `Enabled` and the VM is still deallocated.
- After step 2 of `## Design`: the `Recorded address:` line equals `az network public-ip show -g brokenengine-server-rg -n brokenengine-server-ip --query ipAddress -o tsv`, and the `Size and tier:` line equals the `az disk show` result.
- At the end, `az vm get-instance-view` reports `PowerState/deallocated`.
- Diff read: apart from the scheduler's completion edits, the two recorded lines are the only repository change.

## Notes

- The `dependsOn` Engine Plans carry the user's decision that nothing is created until they are done. `ConnectClientToHostedServer.md` depends on this Plan because it compiles the recorded IP.
- Costs to show at step 6 are in `azure-facts.md` `## Cost examples`: $0.193/hour running, $13.25/month always, about $101.31/month on the schedule. The first 08:00 US Eastern after creation starts the VM before any game server is deployed.
- The live steps need Azure CLI 2.53 or later and the user's `! az login`.
