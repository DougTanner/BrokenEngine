---
name: deploy-server
description: >-
  Provision a game project's dedicated-server Azure VM instance from a
  create-only Bicep template after the user explicitly confirms the spending,
  and run its raw Azure operations. Use when provisioning a project's hosted
  server VM, stopping its billing, re-admitting RDP and SSH after the user's
  public IP changed, or turning the VM back on after its $200 budget stop.
---

# Deploy Server

## Purpose

Create a game project's server Azure VM instance once from the `assets/`
template and the project's parameters file after the user's explicit spending
yes, and run the raw operations that stop it, re-admit the user, and turn it
back on.

## When to use

- Provisioning a project's Azure server VM, once per instance, while
  `<instance>-server-rg` does not exist.
- Stopping the VM's billing, re-admitting RDP and SSH after the user's public
  IPv4 changed, or turning the VM back on after a budget stop: `## Operations`.
- Later operations on the VM, which
  `Documents/Plans/ChangeWorkflow/DeployAndRunHostedServer.md` adds here.

## Inputs

Every run settles the game project `<Name>` (its directory under
`Projects/`): the user names it, or it is the only project with a
`Projects/<Name>/DeployServer/main.bicepparam`, the project's parameters file.
The run confirms that file exists and reads `<instance>` from its
`instanceName` line; every resource-group, resource, and deployment name below
is `<instance>` followed by a fixed suffix. A project without that file has no
instance: adding one (a copy of an existing project's file with the new
project's own `instanceName`) is a repository change outside the run.

Provisioning needs these from the user:

- An Azure pay-as-you-go subscription. A user without one creates it in the
  Azure portal. Microsoft says a brand-new subscription can take up to 48 hours
  before budgets can be created
  (https://learn.microsoft.com/en-us/azure/cost-management-billing/costs/tutorial-acm-create-budgets
  `## Prerequisites`); until then step 7 fails at the budget, before any billed
  resource exists.
- A signed-in account holding Owner, or Contributor plus User Access
  Administrator or Role Based Access Control Administrator, on the
  subscription, because the template creates resources and role assignments.
  The account that created the subscription holds Owner.
- Azure CLI 2.53 or later, per `az version`; the user installs it when it is
  missing. `az bicep build` installs Bicep on first use.
- Sign-in: the user runs `! az login` and selects the pay-as-you-go
  subscription as default, or runs `! az account set --subscription <name>` for
  another.
- The user's public IPv4 address, dotted, with no prefix length.
- `%LOCALAPPDATA%\BrokenEngine\DeployServer\admin-password.txt`, which the user
  creates: its first line is the VM admin password, at least 12 characters with
  upper case, lower case, digits, and symbols. The user keeps the password for
  RDP; step 7 deletes the file after a successful deployment.

## Schedule and budget stop

- The VM runs 08:00-23:00 US Eastern time every day, following daylight
  saving: the schedule workflow starts it at 08:00 and deallocates it at 23:00,
  from the first 08:00 after creation.
- The $200 budget and its stop are per instance: the budget belongs to
  `<instance>-server-rg` and stops only that instance's VM. At $200 of actual
  monthly spend the budget emails the signed-in account and calls the
  budget-stop workflow, which disables the schedule workflow and then
  deallocates the VM. The VM stays off, while the disk and IP keep billing about
  $13.25/month, until the user runs the turn-back-on step in `## Operations`.
  Nothing re-enables it automatically, including at the start of a new month.
- Turning the VM on after a budget stop always enables the schedule workflow
  first, never a bare VM start: a VM started with the schedule disabled has no
  23:00 deallocate.

Prices, billing states, and cost examples: `references/azure-facts.md`.

## Operations

Each operation uses the project and `<instance>` settled per `## Inputs`. Each
operation that starts the VM bills $0.193/hour while it runs; run it only after
the user's explicit yes.

- Stop billing:

  ```powershell
  az vm deallocate -g <instance>-server-rg -n <instance>-server
  ```

- Re-admit the user after their public IPv4 changed, once for `Admin-RDP` and
  once for `Admin-SSH`:

  ```powershell
  az network nsg rule update -g <instance>-server-rg --nsg-name <instance>-server-nsg -n <Admin-RDP|Admin-SSH> --source-address-prefixes <new IPv4>/32
  ```

- Turn back on after a budget stop: enable the schedule workflow.

  ```powershell
  az rest --method post --url "https://management.azure.com/subscriptions/{subscriptionId}/resourceGroups/<instance>-server-rg/providers/Microsoft.Logic/workflows/<instance>-server-schedule/enable?api-version=2019-05-01"
  ```

  Then, only when the user also wants the VM on before the next 08:00 and
  confirms the billing, start it:

  ```powershell
  az vm start -g <instance>-server-rg -n <instance>-server
  ```

## Steps

Provisioning. Run every Azure command from the PowerShell tool. The deployment
commands in steps 5 and 7 each run in one PowerShell call that first sets the
three variables the project's parameters file reads, which die with that call:

```powershell
$env:BROKENENGINE_ADMIN_PASSWORD = Get-Content -LiteralPath "$env:LOCALAPPDATA\BrokenEngine\DeployServer\admin-password.txt" -TotalCount 1
$env:BROKENENGINE_ADMIN_SOURCE_ADDRESS = '<IPv4>'
$env:BROKENENGINE_ALERT_EMAIL = '<user.name>'
az deployment sub <what-if|create> --name <instance>-server --location eastus --parameters Projects/<Name>/DeployServer/main.bicepparam <step options>
```

1. Confirm the `## Inputs` subscription, role, CLI, and sign-in items, walking
   a user with no Azure account through creating the subscription and asking
   them to install the CLI when `az version` fails or reports below 2.53. Done
   when the user is signed in to the chosen subscription with a CLI of 2.53 or
   later.
2. Build the template with standard output discarded. Done when it exits 0 and
   prints no error diagnostic.

   ```powershell
   az bicep build --file .agents/skills/deploy-server/assets/main.bicep --stdout > $null
   ```

3. Settle the project and `<instance>` per `## Inputs`, and ask the user for
   their public IPv4 address. Done when `<instance>` is read and the answer is a
   dotted IPv4 address.
4. Ask the user to create the password file from `## Inputs`. Done when the
   user confirms the file exists.
5. Run the pre-checks in this order. Done when every check passes; on any
   failure stop before anything is created and report it to the user.
   - `az account show`: the subscription name, and `user.name` becomes
     `alertEmail`.
   - `az group exists -n <instance>-server-rg` returns `false`: the
     create-only gate.
   - `az vm image show --urn MicrosoftWindowsServer:WindowsServer:2025-datacenter-azure-edition:latest`
     succeeds, reports `hyperVGeneration` `V2`, and lists a
     `DiskControllerTypes` feature whose value includes `NVMe`.
   - `az vm list-skus -l eastus --size Standard_D2s_v6`: the `Standard_D2s_v6`
     entry's `HyperVGenerations` capability includes `V2`, and the entry has no
     `TrustedLaunchDisabled` capability.
   - `az provider show -n <namespace> --query registrationState -o tsv`:
     `Registered` for `Microsoft.Compute`, `Microsoft.Network`,
     `Microsoft.Logic`, and `Microsoft.Insights`. Else tell the user
     registering is free and creates nothing; on their explicit yes run
     `az provider register -n <namespace> --wait`, else stop.
   - `az vm list-usage -l eastus`: the Dsv6-family vCPU entry and the Total
     Regional vCPUs entry each have at least 2 vCPUs free (limit minus current
     value).
   - The what-if, with step options
     `--no-pretty-print --query "{changes: changes[].{resourceId: resourceId, changeType: changeType}, diagnostics: diagnostics}"`:
     the step 6 confirmation list, not a gate. A parameter or template error,
     including a password under 12 characters, stops here.
6. Show the confirmation summary and wait for the user's explicit yes. Done
   when the user's own message says yes; anything else ends the run with
   nothing created.
   - The subscription.
   - The template's resource list (the resource group plus every resource in
     `assets/server.bicep`) next to the what-if list (each `resourceId` and
     `changeType`), with every what-if diagnostic and every entry whose
     `changeType` is not `Create`.
   - The OS disk `<instance>-server-osdisk` the VM creates, absent from the
     what-if list.
   - The inbound rules: UDP 27015 from any source, TCP 3389 and 22 from the
     user's /32.
   - `## Schedule and budget stop`, including that the 08:00 start runs before
     the game server is installed on the VM, and the turn-back-on step.
   - Costs: $0.193/hour running, $13.25/month always, about $101.31/month on
     the daily schedule, the automation about $0.
7. Deploy with step options
   `--query "{provisioningState: properties.provisioningState, outputs: properties.outputs}"`.
   Done when the state is `Succeeded`, `outputs.publicIp.value` is read, and
   the password file is deleted; otherwise when the failure handling below has
   run and been reported.
   - On `Succeeded`, delete the password file
     (`Remove-Item -LiteralPath '<expanded %LOCALAPPDATA%>\BrokenEngine\DeployServer\admin-password.txt'`).
   - On failure, when `az vm show -g <instance>-server-rg -n <instance>-server`
     finds the VM: first, when the schedule workflow exists, disable it (block
     below) so the 08:00 start cannot restart the VM; then run the
     `## Operations` deallocate.
   - On failure, report which resources the deployment created and stop. A
     budget or role-assignment failure leaves no billed resource. Delete the
     leftover group (`az group delete -n <instance>-server-rg`) only after the
     user's explicit yes.

   ```powershell
   az resource show -g <instance>-server-rg -n <instance>-server-schedule --resource-type Microsoft.Logic/workflows --query properties.state -o tsv
   az rest --method post --url "https://management.azure.com/subscriptions/{subscriptionId}/resourceGroups/<instance>-server-rg/providers/Microsoft.Logic/workflows/<instance>-server-schedule/disable?api-version=2019-05-01"
   ```

8. Verify the created resources while the VM runs. Done when each read below
   matches its `assets/server.bicep` settings and the RDP check succeeds; report
   any mismatch to the user.
   - `az network nsg rule list -g <instance>-server-rg --nsg-name <instance>-server-nsg -o json`:
     exactly the three rules.
   - `az vm show -g <instance>-server-rg -n <instance>-server -d`: size,
     image SKU, OS disk type, `securityProfile.securityType`,
     `storageProfile.diskControllerType`, and `osProfile.computerName`.
   - `az disk show -g <instance>-server-rg -n <instance>-server-osdisk --query "{sizeGb: diskSizeGB, tier: tier}"`:
     the OS disk size and tier.
   - `az network public-ip show -g <instance>-server-rg -n <instance>-server-ip`:
     `sku.name` `Standard` and `publicIPAllocationMethod` `Static`.
   - `az resource show -g <instance>-server-rg -n <instance>-server-schedule --resource-type Microsoft.Logic/workflows`:
     state `Enabled`, `identity.type` `SystemAssigned`, and the recurrence.
   - `az monitor action-group show -g <instance>-server-rg -n <instance>-server-budget-ag --query "{enabled: enabled, receivers: logicAppReceivers[].{name: name, resourceId: resourceId}}"`:
     enabled, one receiver on the budget-stop workflow.
   - `az consumption budget list -g <instance>-server-rg`, and the budget
     read below: amount 200, Monthly, and the one notification.
   - `az role assignment list -g <instance>-server-rg -o json`: exactly the
     three assignments.
   - `Test-NetConnection <publicIp> -Port 3389` reports
     `TcpTestSucceeded : True`.

   Budget read:

   ```powershell
   az rest --method get --url "https://management.azure.com/subscriptions/{subscriptionId}/resourceGroups/<instance>-server-rg/providers/Microsoft.Consumption/budgets/<instance>-server-budget?api-version=2026-06-01"
   ```
9. Run the budget-stop path once, the way a budget does, then turn the schedule
   back on. Done when a new budget-stop run has deallocated the VM and disabled
   the schedule, and the enable has left it `Enabled` with the VM deallocated;
   otherwise when the fallback deallocate has run and the failure is reported.
   - Take the budget-stop runs read (block below) and keep its run names.
   - Send the action group's `budget` test notification to its `budget-stop`
     receiver (block below), in one PowerShell call that never prints the
     callback URL.
   - About once a minute for up to 15 minutes, poll the runs read, the VM
     power state, and the schedule state until a run absent from the kept names
     has finished, the VM reports `PowerState/deallocated`, and the schedule is
     `Disabled`.
   - With no new run after 15 minutes, run the compare-and-post call (block
     below) and poll the same way when it prints `match`; on `mismatch` report
     the failed check. Record which path, the test notification or the direct
     POST, produced the evidence.
   - When the new run's action errors show an authorization failure, wait 10
     minutes and trigger once more by the same path.
   - Then run the `## Operations` enable without `az vm start`, and read the
     schedule state (`Enabled` expected). If no run deallocated the VM, run the
     `## Operations` deallocate and report the failed check.

   Runs read, then a new run's actions:

   ```powershell
   az rest --method get --url "https://management.azure.com/subscriptions/{subscriptionId}/resourceGroups/<instance>-server-rg/providers/Microsoft.Logic/workflows/<instance>-server-budget-stop/runs?api-version=2019-05-01" --query "value[].{name: name, startTime: properties.startTime, status: properties.status}"
   az rest --method get --url "https://management.azure.com/subscriptions/{subscriptionId}/resourceGroups/<instance>-server-rg/providers/Microsoft.Logic/workflows/<instance>-server-budget-stop/runs/<run name>/actions?api-version=2019-05-01" --query "value[].{name: name, status: properties.status, code: properties.code, error: properties.error}"
   ```

   VM power state and schedule state:

   ```powershell
   az vm get-instance-view -g <instance>-server-rg -n <instance>-server --query "instanceView.statuses[?starts_with(code,'PowerState')].code" -o tsv
   az resource show -g <instance>-server-rg -n <instance>-server-schedule --resource-type Microsoft.Logic/workflows --query properties.state -o tsv
   ```

   Test notification (the stored callback URL contains `&`, so it is passed
   in escaped double quotes past Command Prompt):

   ```powershell
   $receiverId = az resource show -g <instance>-server-rg -n <instance>-server-budget-stop --resource-type Microsoft.Logic/workflows --query id -o tsv
   $callbackUrl = az monitor action-group show -g <instance>-server-rg -n <instance>-server-budget-ag --query "logicAppReceivers[0].callbackUrl" -o tsv
   az monitor action-group test-notifications create --action-group <instance>-server-budget-ag -g <instance>-server-rg --alert-type budget --add-action logicapp budget-stop $receiverId "`"$callbackUrl`"" --query "{state: state, actions: actionDetails[].{name: name, status: status, subState: subState}}"
   ```

   Compare and post:

   ```powershell
   $stored = az monitor action-group show -g <instance>-server-rg -n <instance>-server-budget-ag --query "logicAppReceivers[0].callbackUrl" -o tsv
   $current = az rest --method post --url "https://management.azure.com/subscriptions/{subscriptionId}/resourceGroups/<instance>-server-rg/providers/Microsoft.Logic/workflows/<instance>-server-budget-stop/triggers/manual/listCallbackUrl?api-version=2019-05-01" --query value -o tsv
   if ($stored -ceq $current) { Invoke-RestMethod -Method Post -Uri $current | Out-Null; 'match' } else { 'mismatch' }
   ```

10. Report to the user. Done when the report gives `publicIp`, the OS disk size
    and tier, the budget-stop evidence path, and the final power state from the
    step 9 read (`PowerState/deallocated` expected; the VM next starts at
    08:00 US Eastern).

## Rules

- Before the user's explicit yes in step 6, run only read-only commands:
  `az version`, `az account show`, `az group exists`, `az vm image show`,
  `az vm list-skus`, `az provider show`, `az vm list-usage`, `az bicep build`,
  and `az deployment sub what-if`. The one exception is step 5's
  `az provider register`, which runs only after its own explicit user yes.
- Deploy only onto a subscription where `az group exists -n <instance>-server-rg`
  returns `false`. The skill has no redeploy, update, repair, resize, or
  teardown path; recovery from a partial deployment is the user-directed
  `az group delete -n <instance>-server-rg` after their explicit yes.
- Pass the admin password only through `BROKENENGINE_ADMIN_PASSWORD` and the
  project's parameters file; keep it off every command line and out of the
  conversation.
- Keep the budget-stop trigger's callback URL in shell variables only: project
  it out of every printed result and never add `--debug` to a command that
  carries it.
- Report the public IP to the user; record it nowhere in this package.

## References

- [`references/azure-facts.md`](references/azure-facts.md) — name patterns,
  prices, billing states, the schedule and budget facts, cost examples, and the
  provisioned OS disk record.
- [`assets/main.bicep`](assets/main.bicep) — the subscription-scope template:
  the resource group, the parameters, and the `publicIp` output.
- [`assets/server.bicep`](assets/server.bicep) — the resource-group module:
  every resource and setting.
- `Projects/<Name>/DeployServer/main.bicepparam` — the project's parameters
  file: assigns `instanceName` and reads the admin password, the user's IPv4,
  and the alert email from the three `BROKENENGINE_*` environment variables.
