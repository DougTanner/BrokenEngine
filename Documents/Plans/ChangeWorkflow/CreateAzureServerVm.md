<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-18T15:25:05.648Z","dependsOn":["Documents/Plans/Engine/SharedSceneExportData.md","Documents/Plans/Engine/TrackDeltasReceivedBeforeFullStateAdoption.md"]} -->
# Create the Azure server VM with /deploy-server and record its address

## Context

The `/deploy-server` skill (`.agents/skills/deploy-server/`) holds a create-only subscription-scope Bicep template (`assets/main.bicep`, `assets/server.bicep`), deployed with the BrokenEngineSandbox parameters file `Projects/BrokenEngineSandbox/DeployServer/main.bicepparam` (instance `brokenengine`), and the provisioning procedure, `SKILL.md` `## Steps` 1-10, for the dedicated server's Azure VM, its daily schedule workflow, and its $200 budget stop.
No VM is provisioned. Nothing may be created in Azure, and the server may not be started, until the Plans in this Plan's `dependsOn` are done. That list covers every Plan outside the Azure hosting work. This Plan adds the monitor access configuration before the create-only deployment, then provisions the VM after the user's spending confirmation.

Gap this Plan closes:

- `Documents/Plans/Engine/ConnectClientToHostedServer.md` `## Server address` reads `Recorded address: none`, so that Plan stops BLOCKED when claimed, and `Documents/Plans/ChangeWorkflow/DeployAndRunHostedServer.md` waits behind it.
- `.agents/skills/deploy-server/references/azure-facts.md` `## Provisioned OS disk` reads `Size and tier: none`.

The current template defines three NSG rules and does not admit the Server Monitor. Its new fourth rule must be present before provisioning; the template is create-only and this Plan does not repair an existing VM. All build, provider, image, quota, and what-if checks in the skill run against the configuration being deployed.

Required configuration:

- Subscription: the pay-as-you-go subscription named "Azure subscription", already confirmed as pay-as-you-go. The user signs in as the account owner.
- No Azure resource is created before the user's own explicit spending yes at `SKILL.md` step 6 in this run. A yes given in an earlier session, or this Plan's text, is not that yes.
- Inbound access: UDP 27015 from any source. RDP (TCP 3389), SSH (TCP 22), and the Server Monitor (TCP 27017) only from the user's current public IPv4. The user's IP is dynamic, so step 3 asks for it at run time and never reuses an earlier address.
- Schedule: the VM runs 08:00-23:00 US Eastern every day, following daylight saving.
- Budget: $200 a month. At that spend the VM is deallocated and stays off until the user runs the skill's documented turn-back-on step.

## Design

1. Add `Admin-Monitor` after `Admin-SSH` in `.agents/skills/deploy-server/assets/server.bicep`: priority 130, direction `Inbound`, access `Allow`, protocol `Tcp`, source address `'${adminSourceAddress}/32'`, source port `*`, destination address `*`, destination port `'27017'`. Keep the other three rules unchanged.
2. Update `.agents/skills/deploy-server/SKILL.md` description and `## When to use` to include re-admitting the Server Monitor port. The `## Operations` re-admit command runs for each of `Admin-RDP`, `Admin-SSH`, and `Admin-Monitor`. Step 6 lists TCP 27017 from the user's /32; step 8 checks exactly four rules. Add `Admin-Monitor` to the NSG rule list in `references/azure-facts.md` `## Names`. Validate the Bicep and these instruction changes before any live provisioning.
3. Run `/deploy-server` `## Steps` 1-10 as written, in the main session, because the steps interview the user and hold the spending confirmation. The skill is the procedure. If step 6 ends without the user's yes, nothing is created; the recommended route is to defer this Plan rather than complete it.
4. With `publicIp` and the OS disk size and tier from the step 10 report, record two values:
   - In `ConnectClientToHostedServer.md` `## Server address`, replace `none` on the `Recorded address:` line with `publicIp`.
   - In `azure-facts.md` `## Provisioned OS disk`, replace `none` on the `Size and tier:` line with the size and tier from step 8's `az disk show`.
5. Run the recorded-value and final-power-state checks under `## Acceptance criteria`.

Risk tier: Tier 2. Trigger: one skill's tool behavior with external spending and network exposure in the user's Azure subscription.
The spending is gated by the step 6 yes and capped by the budget stop. The
exposure is fixed by the template's NSG rules, including TCP 27017 only from the
user's /32. No repository runtime surface changes: no C++, determinism/CRC,
serialization, `.pack`, replay, wire, affinity, threading, allocation, shader,
build, or trust change. The monitor secret and bind contract remain owned by
`Engine/Source/Server/AGENTS.md` `## Monitor Channel`; no ENet change is part of
this Plan. Live verification is Azure CLI reads and an RDP port check, not
`/agent-harness`.

## Critical files

- `.agents/skills/deploy-server/assets/server.bicep`
- `.agents/skills/deploy-server/SKILL.md`
- `.agents/skills/deploy-server/references/azure-facts.md`
- `Documents/Plans/Engine/ConnectClientToHostedServer.md`

## In scope

- The `Admin-Monitor` NSG rule, the skill's description/re-admit/inbound/check text, and the `azure-facts.md` NSG list specified in Design steps 1-2.
- One create-only deployment of `.agents/skills/deploy-server/assets/main.bicep` through `SKILL.md` `## Steps` 1-10, after the user's explicit yes at step 6. This includes step 5's `az provider register` only after its own yes, step 7's failure handling, and step 9's budget-stop run and turn-back-on.
- The `Recorded address:` line in `ConnectClientToHostedServer.md` `## Server address`.
- The `Size and tier:` line in `azure-facts.md` `## Provisioned OS disk`.

## Out of scope

- Any template or skill change beyond the monitor rule and its named text updates. Any change to resources, names, schedule times, budget amount, or the other inbound rules. A further template or skill defect found during the run stops the run and is reported to the user.
- Redeploying onto an existing `brokenengine-server-rg`, repair, resize, or teardown, apart from the skill's `az group delete` after the user's explicit yes.
- VM-side configuration, deploying the game server, and starting it: `DeployAndRunHostedServer.md` owns those.
- Any C++ change. Any other line of `ConnectClientToHostedServer.md`, and any other Plan, apart from the scheduler's completion edits.
- Recording the public IP anywhere other than the `Recorded address:` line.

## Acceptance criteria

Before live provisioning:

- Code read confirms exactly four template NSG rules, with `Admin-Monitor` as Design step 1 specifies and the other rules unchanged; every named skill and facts section lists the fourth rule, and re-admit covers all three admin rules.
- `az bicep build` of `assets/main.bicep` succeeds; the applicable static checks and `/external-skill-creator` validation of `.agents/skills/deploy-server/` pass.

Live Azure checks, each after the user's step 6 yes; expected settings are those of `assets/server.bicep`:

- `az deployment sub create` reports `provisioningState` `Succeeded` and a nonempty `outputs.publicIp.value`, and the password file no longer exists.
- `az network nsg rule list` lists exactly `Game-UDP-27015` (priority 100, `Udp`, source `*`, port 27015), `Admin-RDP` (110, `Tcp`, the user's /32, port 3389), `Admin-SSH` (120, `Tcp`, the user's /32, port 22), and `Admin-Monitor` (130, `Tcp`, the user's /32, port 27017).
- `az vm show -d` reports `Standard_D2s_v6`, image SKU `2025-datacenter-azure-edition`, a `StandardSSD_LRS` OS disk, `securityProfile.securityType` `TrustedLaunch`, `storageProfile.diskControllerType` `NVMe`, and `osProfile.computerName` `gameserver`. `az disk show` on `brokenengine-server-osdisk` gives its size and tier.
- `az network public-ip show` reports `sku.name` `Standard` and `publicIPAllocationMethod` `Static`.
- The schedule workflow reports state `Enabled`, `identity.type` `SystemAssigned`, and a `Recurrence` trigger: frequency `Day`, interval 1, `timeZone` `Eastern Standard Time`, hours [8, 23], minutes [0], `startTime` `2026-01-01T00:00:00`.
- The action group is enabled with one `logicAppReceivers` entry whose `resourceId` is the budget-stop workflow.
- The budget is amount 200, `Monthly`, with exactly one enabled notification: `Actual`, `GreaterThanOrEqualTo`, threshold 100, `contactEmails` [the signed-in `user.name`], `contactGroups` [the action group's ID].
- `az role assignment list -g brokenengine-server-rg` lists exactly three resource-group-scope assignments: the schedule workflow's identity with Desktop Virtualization Power On Off Contributor, and the budget-stop workflow's identity with that role and with Logic App Operator.
- While the VM runs, `Test-NetConnection <publicIp> -Port 3389` reports `TcpTestSucceeded : True`.
- Step 9: after the `budget` test notification, or the direct callback POST fallback, with the producing path recorded, a new budget-stop run finishes, the VM reaches `PowerState/deallocated` within 15 minutes, and the schedule workflow is `Disabled`. After the turn-back-on enable, the schedule workflow is `Enabled` and the VM is still deallocated.
- After step 4 of `## Design`: the `Recorded address:` line equals `az network public-ip show -g brokenengine-server-rg -n brokenengine-server-ip --query ipAddress -o tsv`, and the `Size and tier:` line equals the `az disk show` result.
- At the end, `az vm get-instance-view` reports `PowerState/deallocated`.
- Diff read: apart from the scheduler's completion edits, the repository changes are limited to the monitor rule and named instruction/facts updates plus the two recorded values.

## Notes

- The `dependsOn` Plans, every Plan outside the Azure hosting work, carry the user's decision that nothing is created until they are done. `ConnectClientToHostedServer.md` depends on this Plan because it compiles the recorded IP.
- Costs to show at step 6 are in `azure-facts.md` `## Cost examples`: $0.193/hour running, $13.25/month always, about $101.31/month on the schedule. The first 08:00 US Eastern after creation starts the VM before any game server is deployed.
- The live steps need Azure CLI 2.53 or later and the user's `! az login`.
